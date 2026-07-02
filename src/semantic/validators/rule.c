/* Rule-level semantic validators: cross-field consistency — checks that no
 * single field can answer on its own. Structural completeness needs no
 * checking (the grammar cannot build a partial Rule); what belongs here is
 * coherence between fields the parser validated independently. */

#include "semantic/validators/validators.h"

void sem_validate_rule(SemanticContext *sctx, const Rule *rule,
                       DiagList *diags)
{
    (void)sctx;

    /* Port expressions on an icmp rule parse fine and each may be
     * individually valid — but ICMP has no port concept, so the
     * combination is incoherent. Warning, not error: the rule still
     * loads. Applies to any non-`any` port form (single, range, list). */
    if (rule->protocol == PROTO_ICMP) {
        if (rule->src_port.kind == PORT_EXPR) {
            diag_semantic(diags, rule->span, SEM_FAULT_ICMP_WITH_PORTS,
                          "SrcPort", NULL);
        }
        if (rule->dst_port.kind == PORT_EXPR) {
            diag_semantic(diags, rule->span, SEM_FAULT_ICMP_WITH_PORTS,
                          "DstPort", NULL);
        }
    }
}
