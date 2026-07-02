#ifndef MODELS_DIAGNOSTIC_H
#define MODELS_DIAGNOSTIC_H

typedef enum DiagCategory {
    DIAG_LEXICAL,
    DIAG_SYNTAX,
    DIAG_SEMANTIC
} DiagCategory;

typedef enum DiagSeverity {
    DIAG_ERROR,
    DIAG_WARNING
} DiagSeverity;

/* One problem, fully self-describing: renderable without any other context.
 * All strings are heap-owned by the Diagnostic (freed by diagnostic_free).
 * expected/found/explanation may be NULL where not applicable (e.g. lexical
 * diagnostics have no Expected/Found pair — that is parser-state territory,
 * arriving in Phase 4). */
typedef struct Diagnostic {
    DiagCategory category;
    DiagSeverity severity;
    int rule_number;
    int line;
    int column;
    char *expected;     /* syntax only: role vocabulary ("SrcIP") */
    char *found;        /* syntax only: offending token */
    char *field;        /* semantic only: rule field ("DstPort", "sid") */
    char *value;        /* semantic only: offending value ("70000") */
    char *message;
    char *explanation;
} Diagnostic;

const char *diag_category_name(DiagCategory category);
const char *diag_severity_name(DiagSeverity severity);

/* Frees the owned strings (not the struct itself — Diagnostics are stored
 * by value inside the diagnostics list). */
void diagnostic_free(Diagnostic *d);

#endif /* MODELS_DIAGNOSTIC_H */
