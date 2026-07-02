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
    EXPECT_ADDRESS,         /* any | IP | CIDR | variable (Src vs Dst by progress) */
    EXPECT_PORT,            /* any | PORT            (Src vs Dst by progress) */
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

#endif /* DIAGNOSTICS_DIAGNOSTICS_H */
