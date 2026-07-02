#include "semantic/semantic.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "semantic/sid_registry.h"
#include "semantic/validators/validators.h"
#include "utils/xalloc.h"

struct SemanticContext {
    SidRegistry sids;
};

SemanticContext *semantic_context_create(void)
{
    SemanticContext *sctx = xcalloc(1, sizeof(SemanticContext));
    sid_registry_init(&sctx->sids);
    return sctx;
}

void semantic_context_destroy(SemanticContext *sctx)
{
    if (sctx == NULL) {
        return;
    }
    sid_registry_free(&sctx->sids);
    free(sctx);
}

/* Registry accessor for the options validator (keeps SemanticContext
 * opaque outside semantic/). */
SidRegistry *semantic_sid_registry(SemanticContext *sctx)
{
    return &sctx->sids;
}

int sem_parse_number(const char *text, long *out)
{
    if (text == NULL || *text == '\0') {
        return 0;
    }
    for (const char *p = text; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) {
            return 0;
        }
    }
    errno = 0;
    long value = strtol(text, NULL, 10);
    if (errno == ERANGE) {
        return 0;
    }
    *out = value;
    return 1;
}

/* Ordered pass table: header first (field-order reporting), then options,
 * then cross-field checks. Adding a validator = adding a line. */
typedef void (*ValidatorPass)(SemanticContext *, const Rule *, DiagList *);

static const ValidatorPass PASSES[] = {
    sem_validate_header,
    sem_validate_options,
    sem_validate_rule,
};

int semantic_validate(SemanticContext *sctx, const Rule *rule, DiagList *diags)
{
    size_t before = diags->count;

    for (size_t i = 0; i < sizeof(PASSES) / sizeof(PASSES[0]); i++) {
        PASSES[i](sctx, rule, diags);
    }

    int errors = 0;
    for (size_t i = before; i < diags->count; i++) {
        if (diags->items[i].category == DIAG_SEMANTIC
            && diags->items[i].severity == DIAG_ERROR) {
            errors++;
        }
    }
    return errors;
}
