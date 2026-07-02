/* Header semantic validators: the lexer/parser accepted these values as
 * well-formed *tokens*; this pass judges the values. 999.1.1.1 is a valid
 * IP token and an invalid IP address — that distinction lives here.
 *
 * Note on scope: shapes that never lex as a single token (1.2.3, /-1, -1)
 * surface as lexical/syntax errors upstream — there is no structure for
 * this layer to judge. This pass covers everything that parses. */

#include <string.h>

#include "semantic/validators/validators.h"

/* Scans dotted-decimal groups; returns 1 when every octet is 0-255.
 * `stop` bounds the scan (the '/' of a CIDR, or end of string). */
static int octets_valid(const char *text, const char *stop)
{
    const char *p = text;
    while (p < stop) {
        long octet = 0;
        while (p < stop && *p != '.') {
            octet = octet * 10 + (*p - '0');
            if (octet > 255) {
                return 0;
            }
            p++;
        }
        if (p < stop) {
            p++;   /* skip '.' */
        }
    }
    return 1;
}

static void check_endpoint(const Rule *rule, const Endpoint *ep,
                           const char *field, DiagList *diags)
{
    switch (ep->kind) {
    case ENDPOINT_ANY:
    case ENDPOINT_VARIABLE:
        /* 'any' is always valid; variable resolution is a later phase. */
        return;
    case ENDPOINT_IP:
        if (!octets_valid(ep->text, ep->text + strlen(ep->text))) {
            diag_semantic(diags, rule->span, SEM_FAULT_IP_OCTET,
                          field, ep->text);
        }
        return;
    case ENDPOINT_CIDR: {
        const char *slash = strchr(ep->text, '/');
        if (!octets_valid(ep->text, slash)) {
            diag_semantic(diags, rule->span, SEM_FAULT_IP_OCTET,
                          field, ep->text);
        }
        long prefix;
        if (!sem_parse_number(slash + 1, &prefix) || prefix > 32) {
            diag_semantic(diags, rule->span, SEM_FAULT_CIDR_PREFIX,
                          field, ep->text);
        }
        return;
    }
    }
}

static void check_port(const Rule *rule, const PortSpec *port,
                       const char *field, DiagList *diags)
{
    if (port->kind != PORT_NUMBER) {
        return;   /* 'any' */
    }
    long value;
    if (!sem_parse_number(port->text, &value) || value < 1 || value > 65535) {
        diag_semantic(diags, rule->span, SEM_FAULT_PORT_RANGE,
                      field, port->text);
    }
}

void sem_validate_header(SemanticContext *sctx, const Rule *rule,
                         DiagList *diags)
{
    (void)sctx;

    /* Protocol and direction are grammar-constrained enums — the parser
     * cannot construct an invalid one. They stay listed as validated
     * fields because this is the hook point when protocols gain semantics
     * (e.g. icmp/port coherence lives in the rule-level pass). */

    check_endpoint(rule, &rule->src_ip, "SrcIP", diags);
    check_port(rule, &rule->src_port, "SrcPort", diags);
    check_endpoint(rule, &rule->dst_ip, "DstIP", diags);
    check_port(rule, &rule->dst_port, "DstPort", diags);
}
