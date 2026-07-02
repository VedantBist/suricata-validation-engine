#include "diagnostics/diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
