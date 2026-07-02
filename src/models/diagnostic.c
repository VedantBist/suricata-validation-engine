#include "models/diagnostic.h"

#include <stdlib.h>

const char *diag_category_name(DiagCategory category)
{
    switch (category) {
    case DIAG_LEXICAL:  return "LEXICAL";
    case DIAG_SYNTAX:   return "SYNTAX";
    case DIAG_SEMANTIC: return "SEMANTIC";
    }
    return "UNKNOWN";
}

const char *diag_severity_name(DiagSeverity severity)
{
    switch (severity) {
    case DIAG_ERROR:   return "ERROR";
    case DIAG_WARNING: return "WARNING";
    }
    return "UNKNOWN";
}

void diagnostic_free(Diagnostic *d)
{
    if (d == NULL) {
        return;
    }
    free(d->expected);
    free(d->found);
    free(d->field);
    free(d->value);
    free(d->message);
    free(d->explanation);
    d->expected = d->found = d->field = d->value = NULL;
    d->message = d->explanation = NULL;
}
