#include "diagnostics/diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "models/context.h"   /* RuleProgress thresholds */
#include "utils/xalloc.h"

void diag_list_init(DiagList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void diag_list_free(DiagList *list)
{
    for (size_t i = 0; i < list->count; i++) {
        diagnostic_free(&list->items[i]);
    }
    free(list->items);
    diag_list_init(list);
}

void diag_list_add(DiagList *list, Diagnostic d)
{
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 16;
        list->items = xrealloc(list->items, list->capacity * sizeof(Diagnostic));
    }
    list->items[list->count++] = d;
}

static Diagnostic lexical_error(SrcSpan span)
{
    Diagnostic d;
    memset(&d, 0, sizeof(d));
    d.category = DIAG_LEXICAL;
    d.severity = DIAG_ERROR;
    d.rule_number = span.rule_number;
    d.line = span.first_line;
    d.column = span.first_column;
    return d;
}

static char *formatted(const char *fmt, const char *arg)
{
    int needed = snprintf(NULL, 0, fmt, arg);
    char *buf = xmalloc((size_t)needed + 1);
    snprintf(buf, (size_t)needed + 1, fmt, arg);
    return buf;
}

void diag_lexical_invalid_char(DiagList *list, SrcSpan span, const char *lexeme)
{
    Diagnostic d = lexical_error(span);
    d.found = xstrdup(lexeme);
    d.message = formatted("Invalid character '%s'", lexeme);
    d.explanation = xstrdup(
        "The character does not belong to any recognized Suricata token "
        "class; scanning continued so the rest of the line was still checked");
    diag_list_add(list, d);
}

void diag_lexical_unterminated_string(DiagList *list, SrcSpan span)
{
    Diagnostic d = lexical_error(span);
    d.message = xstrdup("Unterminated string literal");
    d.explanation = xstrdup(
        "The string was closed automatically at the end of the line so that "
        "following rules are not swallowed by the open quote");
    diag_list_add(list, d);
}

/* Role vocabulary + narrative for a syntax error. Src/Dst positions have
 * identical expected-token sets, so the progress cursor disambiguates
 * (<= threshold rather than equality — see RuleProgress in context.h). */
static void role_for(ExpectedClass klass, int progress,
                     const char **role, const char **narrative)
{
    switch (klass) {
    case EXPECT_ACTION:
        *role = "Action";
        *narrative = "Rule must start with an action (alert, drop, or pass)";
        return;
    case EXPECT_PROTOCOL:
        *role = "Protocol";
        *narrative = "Protocol missing or invalid after action";
        return;
    case EXPECT_ADDRESS:
        if (progress <= PROGRESS_SRC_IP) {
            *role = "SrcIP";
            *narrative = "Source IP missing after protocol";
        } else {
            *role = "DstIP";
            *narrative = "Destination IP missing after direction";
        }
        return;
    case EXPECT_PORT:
        if (progress <= PROGRESS_SRC_IP) {
            *role = "SrcPort";
            *narrative = "Source port missing after source IP";
        } else {
            *role = "DstPort";
            *narrative = "Destination port missing after destination IP";
        }
        return;
    case EXPECT_DIRECTION:
        *role = "Direction";
        *narrative = "Direction operator (-> or <>) missing after source port";
        return;
    case EXPECT_END_OF_RULE:
        *role = "end of rule";
        *narrative = "Unexpected content after the destination port "
                     "(rule options are not part of the current grammar subset)";
        return;
    case EXPECT_OTHER:
        break;
    }
    *role = "one of the acceptable tokens";
    *narrative = "Rule does not match the expected structure";
}

static char *join_names(const char *const *names, int count)
{
    size_t len = 1;
    for (int i = 0; i < count; i++) {
        len += strlen(names[i]) + 2;
    }
    char *out = xmalloc(len);
    out[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            strcat(out, ", ");
        }
        strcat(out, names[i]);
    }
    return out;
}

void diag_syntax_error(DiagList *list, SrcSpan span, int progress,
                       ExpectedClass klass,
                       const char *const *expected_names, int expected_count,
                       const char *found_symbol, const char *found_text)
{
    Diagnostic d;
    memset(&d, 0, sizeof(d));
    d.category = DIAG_SYNTAX;
    d.severity = DIAG_ERROR;
    d.rule_number = span.rule_number;
    d.line = span.first_line;
    d.column = span.first_column;

    const char *role;
    const char *narrative;
    role_for(klass, progress, &role, &narrative);
    d.expected = xstrdup(role);
    d.message = xstrdup(narrative);

    if (found_symbol == NULL) {
        found_symbol = "(unknown)";
    }
    if (found_text != NULL && *found_text != '\0'
        && strcmp(found_text, found_symbol) != 0) {
        int needed = snprintf(NULL, 0, "%s \"%s\"", found_symbol, found_text);
        d.found = xmalloc((size_t)needed + 1);
        snprintf(d.found, (size_t)needed + 1, "%s \"%s\"", found_symbol, found_text);
    } else {
        d.found = xstrdup(found_symbol);
    }

    if (expected_count > 0) {
        char *joined = join_names(expected_names, expected_count);
        int needed = snprintf(NULL, 0, "Acceptable tokens here: %s", joined);
        d.explanation = xmalloc((size_t)needed + 1);
        snprintf(d.explanation, (size_t)needed + 1,
                 "Acceptable tokens here: %s", joined);
        free(joined);
    }

    diag_list_add(list, d);
}
