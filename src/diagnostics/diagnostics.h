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

#endif /* DIAGNOSTICS_DIAGNOSTICS_H */
