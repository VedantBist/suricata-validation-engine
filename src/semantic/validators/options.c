/* Option semantic validators: key support, sid discipline (presence,
 * uniqueness within the rule, numeric, positive, unique across the file),
 * rev numeric, empty-string values.
 *
 * The parser accepts any identifier as an option key (Phase 5 grammar);
 * whether a key is *supported* is decided here, which is why unknown keys
 * produce semantic — not syntax — diagnostics. */

#include <string.h>

#include "semantic/sid_registry.h"
#include "semantic/validators/validators.h"

/* Defined in semantic.c; keeps SemanticContext opaque elsewhere. */
SidRegistry *semantic_sid_registry(SemanticContext *sctx);

static int is_supported_key(const char *key)
{
    return strcmp(key, "msg") == 0 || strcmp(key, "sid") == 0
        || strcmp(key, "rev") == 0 || strcmp(key, "content") == 0;
}

static void check_sid(SemanticContext *sctx, const Rule *rule,
                      const Option *sid, int sid_count, DiagList *diags)
{
    if (sid_count == 0) {
        diag_semantic(diags, rule->span, SEM_FAULT_SID_MISSING, "sid", NULL);
        return;
    }
    if (sid->value_kind != OPTVAL_NUMBER) {
        diag_semantic(diags, sid->span, SEM_FAULT_SID_NOT_NUMERIC,
                      "sid", sid->value);
        return;
    }
    long value;
    if (!sem_parse_number(sid->value, &value)) {
        diag_semantic(diags, sid->span, SEM_FAULT_SID_NOT_NUMERIC,
                      "sid", sid->value);
        return;
    }
    if (value <= 0) {
        diag_semantic(diags, sid->span, SEM_FAULT_SID_NOT_POSITIVE,
                      "sid", sid->value);
        return;
    }

    /* Cross-rule uniqueness: register only rules whose sid is otherwise
     * clean and unrepeated, so one broken rule cannot poison the
     * provenance of a later duplicate report. */
    if (sid_count == 1) {
        SidRegistry *reg = semantic_sid_registry(sctx);
        const SidEntry *first = sid_registry_find(reg, value);
        if (first != NULL) {
            diag_semantic_duplicate_sid(diags, sid->span, sid->value,
                                        first->rule_number, first->line);
        } else {
            sid_registry_insert(reg, value, rule->span.rule_number,
                                rule->span.first_line);
        }
    }
}

void sem_validate_options(SemanticContext *sctx, const Rule *rule,
                          DiagList *diags)
{
    const Option *first_sid = NULL;
    int sid_count = 0;

    size_t option_count = rule->options != NULL ? rule->options->count : 0;
    for (size_t i = 0; i < option_count; i++) {
        const Option *opt = rule->options->items[i];

        if (!is_supported_key(opt->key)) {
            diag_semantic(diags, opt->span, SEM_FAULT_UNKNOWN_KEY,
                          opt->key, opt->value);
            continue;
        }

        if (strcmp(opt->key, "sid") == 0) {
            sid_count++;
            if (sid_count == 1) {
                first_sid = opt;
            } else {
                diag_semantic(diags, opt->span, SEM_FAULT_SID_REPEATED,
                              "sid", opt->value);
            }
        } else if (strcmp(opt->key, "rev") == 0) {
            long value;
            if (opt->value_kind != OPTVAL_NUMBER
                || !sem_parse_number(opt->value, &value)) {
                diag_semantic(diags, opt->span, SEM_FAULT_REV_NOT_NUMERIC,
                              "rev", opt->value);
            }
        } else if (strcmp(opt->key, "msg") == 0) {
            if (opt->value_kind == OPTVAL_STRING && opt->value[0] == '\0') {
                diag_semantic(diags, opt->span, SEM_FAULT_EMPTY_MSG,
                              "msg", NULL);
            }
        } else {   /* content */
            if (opt->value_kind == OPTVAL_STRING && opt->value[0] == '\0') {
                diag_semantic(diags, opt->span, SEM_FAULT_EMPTY_CONTENT,
                              "content", NULL);
            }
        }
    }

    /* Presence check runs for every rule — including rules with no options
     * block at all, which is precisely the missing-sid case. */
    check_sid(sctx, rule, first_sid, sid_count, diags);
}
