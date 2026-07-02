#ifndef DIAGNOSTICS_DIAGNOSTICS_H
#define DIAGNOSTICS_DIAGNOSTICS_H

#include <stddef.h>

#include "models/diagnostic.h"
#include "models/source_location.h"

/* Growable, run-lifetime list of diagnostics. Owned by the ParserContext;
 * appended to by the lexer (lexical), parser (syntax, Phase 4), and semantic
 * validators (Phase 6); consumed by reporting. Order of insertion == input
 * order — reporting must not reorder (golden-test determinism). */
typedef struct DiagList {
    Diagnostic *items;
    size_t count;
    size_t capacity;

    /* O(1) statistics, maintained by diag_list_add — the summary engine
     * and the max-errors cap read these; nothing rescans the list. */
    size_t errors;
    size_t warnings;
    size_t lexical;
    size_t syntax;
    size_t semantic;
} DiagList;

void diag_list_init(DiagList *list);
void diag_list_free(DiagList *list);

/* Appends by value; the list takes ownership of the Diagnostic's strings. */
void diag_list_add(DiagList *list, Diagnostic d);

/* --- Lexical diagnostic factories ---------------------------------------
 * The lexer reports *what it saw*; the sentence describing it lives here.
 * This keeps message prose out of rules.l per the module boundaries. */

void diag_lexical_invalid_char(DiagList *list, SrcSpan span, const char *lexeme);
void diag_lexical_unterminated_string(DiagList *list, SrcSpan span);

/* --- Syntax diagnostic factory -------------------------------------------
 * Called from the parser's custom error report (grammar.y epilogue). The
 * parser classifies its table-derived expected-token set structurally
 * (which token family is acceptable next); this module turns that plus the
 * progress cursor into role vocabulary ("SrcIP") and narrative prose
 * ("Source IP missing after protocol"). Structure from the parser,
 * language from here. */

typedef enum ExpectedClass {
    EXPECT_ACTION,          /* alert | drop | pass */
    EXPECT_PROTOCOL,        /* tcp | udp | icmp */
    EXPECT_ADDRESS,         /* field level: any | IP | CIDR | variable | ! | [ */
    EXPECT_PORT,            /* field level: any | PORT | : | ! | [ */
    EXPECT_ADDR_ELEM,       /* inside a list or after '!': no `any` allowed */
    EXPECT_PORT_ELEM,       /* inside a list or after '!': no `any` allowed */
    EXPECT_LIST_DELIM,      /* , | ] — between list elements */
    EXPECT_DIRECTION,       /* -> | <> */
    EXPECT_OPTIONS_OR_END,  /* ( | EOL — after the destination port */
    EXPECT_OPTION_KEY,      /* option keyword — right after '(' */
    EXPECT_OPTION_OR_CLOSE, /* option keyword | ) — after a completed option */
    EXPECT_COLON,           /* : — after an option key */
    EXPECT_OPTION_VALUE,    /* string | number | identifier — after ':' */
    EXPECT_SEMICOLON,       /* ; — after an option value */
    EXPECT_END_OF_RULE,     /* only EOL is acceptable — after ')' */
    EXPECT_OTHER            /* fallback: report the raw token list */
} ExpectedClass;

/* expected_names: token spellings straight from the parser tables
 * (yysymbol_name aliases), joined into the explanation as ground truth.
 * found_symbol: alias of the offending lookahead; found_text: its raw
 * lexeme as scanned (may be NULL/empty for EOL/EOF). */
void diag_syntax_error(DiagList *list, SrcSpan span, int progress,
                       ExpectedClass klass,
                       const char *const *expected_names, int expected_count,
                       const char *found_symbol, const char *found_text);

/* --- Semantic diagnostic factory -----------------------------------------
 * Validators detect faults over Rule structures; this module owns severity
 * and every word of prose (fault table in diagnostics.c). Semantic
 * diagnostics never depend on parser state — only on the Rule and the
 * validator's finding. */

typedef enum SemFault {
    SEM_FAULT_IP_OCTET,        /* ERROR: IPv4 octet outside 0-255 */
    SEM_FAULT_CIDR_PREFIX,     /* ERROR: CIDR mask outside /0-/32 */
    SEM_FAULT_PORT_RANGE,      /* ERROR: port outside 1-65535 */
    SEM_FAULT_SID_MISSING,     /* ERROR: rule declares no sid */
    SEM_FAULT_SID_REPEATED,    /* ERROR: more than one sid option in a rule */
    SEM_FAULT_SID_NOT_NUMERIC, /* ERROR */
    SEM_FAULT_SID_NOT_POSITIVE,/* ERROR: sid must be > 0 */
    SEM_FAULT_REV_NOT_NUMERIC, /* ERROR */
    SEM_FAULT_EMPTY_MSG,       /* WARNING: legal but useless */
    SEM_FAULT_EMPTY_CONTENT,   /* ERROR: unmatched-able predicate */
    SEM_FAULT_UNKNOWN_KEY,     /* ERROR: key outside the supported subset */
    SEM_FAULT_ICMP_WITH_PORTS, /* WARNING: cross-field coherence */
    SEM_FAULT_RANGE_ORDER,     /* ERROR: port range lower bound > upper */
    SEM_FAULT_DUP_LIST_ENTRY,  /* ERROR: identical entry twice in one list */
    SEM_FAULT_REDUNDANT_ENTRY, /* WARNING: entry subsumed by a wider range */
    SEM_FAULT_ALL_NEGATED_LIST /* WARNING: every list entry negated */
} SemFault;

/* field: short field name ("DstPort", "sid", or the offending option key);
 * value: offending value text, NULL when not applicable. */
void diag_semantic(DiagList *list, SrcSpan span, SemFault fault,
                   const char *field, const char *value);

/* Cross-rule duplicate needs provenance of the first occurrence. */
void diag_semantic_duplicate_sid(DiagList *list, SrcSpan span,
                                 const char *sid_text,
                                 int first_rule, int first_line);

#endif /* DIAGNOSTICS_DIAGNOSTICS_H */
