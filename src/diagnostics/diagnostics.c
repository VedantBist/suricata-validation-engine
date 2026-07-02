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
    case EXPECT_ADDR_ELEM:
        *role = "AddressSpec";
        *narrative = "Address expression missing (inside a list or after '!')";
        return;
    case EXPECT_PORT_ELEM:
        *role = "PortSpec";
        *narrative = "Port expression missing (inside a list or after '!')";
        return;
    case EXPECT_LIST_DELIM:
        *role = "COMMA or ']'";
        *narrative = "Missing ',' separator or ']' closing the list";
        return;
    case EXPECT_DIRECTION:
        *role = "Direction";
        *narrative = "Direction operator (-> or <>) missing after source port";
        return;
    case EXPECT_OPTIONS_OR_END:
        *role = "Options or end of rule";
        *narrative = "Unexpected content after the destination port "
                     "(only an options block or end of line may follow)";
        return;
    case EXPECT_OPTION_KEY:
        *role = "OptionKey";
        *narrative = "Option missing after '(' "
                     "(supported keys: msg, sid, rev, content)";
        return;
    case EXPECT_OPTION_OR_CLOSE:
        *role = "OptionKey or ')'";
        *narrative = "Expected another option or ')' closing the options block";
        return;
    case EXPECT_COLON:
        *role = "COLON";
        *narrative = "Colon missing after option key";
        return;
    case EXPECT_OPTION_VALUE:
        *role = "Value";
        *narrative = "Option value missing after colon";
        return;
    case EXPECT_SEMICOLON:
        *role = "SEMICOLON";
        *narrative = "Missing semicolon after option value";
        return;
    case EXPECT_END_OF_RULE:
        *role = "end of rule";
        *narrative = "Unexpected content after the options block";
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

/* ---- Semantic fault table: severity + prose live here, not in semantic/.
 * msg_fmt may contain one %s consumed by the human field name; snprintf
 * ignores surplus arguments for formats without it. */
static const char *field_human(const char *field)
{
    if (strcmp(field, "SrcIP") == 0)   return "Source IP";
    if (strcmp(field, "DstIP") == 0)   return "Destination IP";
    if (strcmp(field, "SrcPort") == 0) return "Source port";
    if (strcmp(field, "DstPort") == 0) return "Destination port";
    return field;
}

static const struct {
    DiagSeverity severity;
    const char *msg_fmt;
    const char *explanation;
} SEM_FAULTS[] = {
    [SEM_FAULT_IP_OCTET] = { DIAG_ERROR,
        "%s contains an IP octet outside the valid range (0-255)",
        "Each dotted segment of an IPv4 address must be between 0 and 255" },
    [SEM_FAULT_CIDR_PREFIX] = { DIAG_ERROR,
        "%s has a CIDR prefix outside the valid range (/0-/32)",
        "An IPv4 CIDR mask must be between /0 and /32" },
    [SEM_FAULT_PORT_RANGE] = { DIAG_ERROR,
        "%s exceeds the valid range (1-65535)",
        "TCP/UDP ports are 16-bit values from 1 to 65535" },
    [SEM_FAULT_SID_MISSING] = { DIAG_ERROR,
        "Rule has no sid option",
        "Every rule must declare exactly one numeric signature id (sid)" },
    [SEM_FAULT_SID_REPEATED] = { DIAG_ERROR,
        "Duplicate sid option inside the rule",
        "A rule must declare exactly one sid" },
    [SEM_FAULT_SID_NOT_NUMERIC] = { DIAG_ERROR,
        "sid value is not numeric",
        "sid must be a positive integer" },
    [SEM_FAULT_SID_NOT_POSITIVE] = { DIAG_ERROR,
        "sid must be greater than zero",
        "sid 0 is reserved; signature ids start at 1" },
    [SEM_FAULT_REV_NOT_NUMERIC] = { DIAG_ERROR,
        "rev value is not numeric",
        "rev must be a non-negative integer" },
    [SEM_FAULT_EMPTY_MSG] = { DIAG_WARNING,
        "msg string is empty",
        "An empty msg makes alerts unreadable; give the rule a description" },
    [SEM_FAULT_EMPTY_CONTENT] = { DIAG_ERROR,
        "content string is empty",
        "An empty content match can never match meaningfully" },
    [SEM_FAULT_UNKNOWN_KEY] = { DIAG_ERROR,
        "Unknown option key '%s'",
        "Supported option keys in this grammar subset: msg, sid, rev, content" },
    [SEM_FAULT_ICMP_WITH_PORTS] = { DIAG_WARNING,
        "%s is meaningless for the icmp protocol",
        "ICMP has no ports; use 'any' for ports in icmp rules" },
    [SEM_FAULT_RANGE_ORDER] = { DIAG_ERROR,
        "Port range lower bound exceeds upper bound",
        "Ranges are written low:high; an inverted range matches nothing" },
    [SEM_FAULT_DUP_LIST_ENTRY] = { DIAG_ERROR,
        "Duplicate entry in %s list",
        "Each list element must be unique" },
    [SEM_FAULT_REDUNDANT_ENTRY] = { DIAG_WARNING,
        "%s list entry is subsumed by a wider range in the same list",
        "The entry adds nothing; the enclosing range already covers it" },
    [SEM_FAULT_ALL_NEGATED_LIST] = { DIAG_WARNING,
        "Every entry in the %s list is negated",
        "A fully negated list matches almost everything; "
        "this is usually an authoring mistake" },
};

static Diagnostic semantic_base(SrcSpan span, DiagSeverity severity)
{
    Diagnostic d;
    memset(&d, 0, sizeof(d));
    d.category = DIAG_SEMANTIC;
    d.severity = severity;
    d.rule_number = span.rule_number;
    d.line = span.first_line;
    d.column = span.first_column;
    return d;
}

void diag_semantic(DiagList *list, SrcSpan span, SemFault fault,
                   const char *field, const char *value)
{
    Diagnostic d = semantic_base(span, SEM_FAULTS[fault].severity);
    d.field = xstrdup(field);
    if (value != NULL) {
        d.value = xstrdup(value);
    }
    d.message = formatted(SEM_FAULTS[fault].msg_fmt, field_human(field));
    d.explanation = xstrdup(SEM_FAULTS[fault].explanation);
    diag_list_add(list, d);
}

void diag_semantic_duplicate_sid(DiagList *list, SrcSpan span,
                                 const char *sid_text,
                                 int first_rule, int first_line)
{
    Diagnostic d = semantic_base(span, DIAG_ERROR);
    d.field = xstrdup("sid");
    d.value = xstrdup(sid_text);
    d.message = xstrdup("Duplicate sid detected");
    int needed = snprintf(NULL, 0,
                          "sid %s was first used by rule #%d (line %d); "
                          "signature ids must be unique",
                          sid_text, first_rule, first_line);
    d.explanation = xmalloc((size_t)needed + 1);
    snprintf(d.explanation, (size_t)needed + 1,
             "sid %s was first used by rule #%d (line %d); "
             "signature ids must be unique",
             sid_text, first_rule, first_line);
    diag_list_add(list, d);
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
