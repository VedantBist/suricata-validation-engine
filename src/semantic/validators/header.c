/* Header semantic validators: the lexer/parser accepted these values as
 * well-formed *structure*; this pass judges the values. 999.1.1.1 is a
 * valid IP token and an invalid address; 1024:80 is a valid range shape
 * and an inverted interval — those distinctions live here.
 *
 * Normalization happens here and only here: each port element is reduced
 * to a canonical [lo,hi] interval (single p -> [p,p], `1024:` ->
 * [1024,65535], `:80` -> [1,80]) for ordering, duplicate and subsumption
 * analysis. The Rule is read-only — normalization never mutates parser
 * output.
 *
 * Fault discipline per element: at most one ERROR (bound fault wins over
 * ordering) so diagnostics stay proportional to the number of broken
 * elements, and stress invariants stay assertable.
 *
 * Scope note: shapes that never lex/parse (1.2.3, /-1, -1, nested lists)
 * surface as lexical/syntax errors upstream — there is no structure for
 * this layer to judge. This pass covers everything that parses. */

#include <stdio.h>
#include <string.h>

#include "semantic/validators/validators.h"

/* ---- shared -------------------------------------------------------------*/

static void check_all_negated(const Rule *rule, int is_list, size_t count,
                              size_t negated, const char *field,
                              DiagList *diags)
{
    if (is_list && count > 0 && negated == count) {
        diag_semantic(diags, rule->span, SEM_FAULT_ALL_NEGATED_LIST,
                      field, NULL);
    }
}

/* ---- addresses ----------------------------------------------------------*/

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

static void check_addr_elem(const Rule *rule, const AddrElem *elem,
                            const char *field, DiagList *diags)
{
    switch (elem->kind) {
    case ADDR_ELEM_VARIABLE:
        return;   /* variable resolution is a later phase */
    case ADDR_ELEM_IP:
        if (!octets_valid(elem->text, elem->text + strlen(elem->text))) {
            diag_semantic(diags, rule->span, SEM_FAULT_IP_OCTET,
                          field, elem->text);
        }
        return;
    case ADDR_ELEM_CIDR: {
        const char *slash = strchr(elem->text, '/');
        if (!octets_valid(elem->text, slash)) {
            diag_semantic(diags, rule->span, SEM_FAULT_IP_OCTET,
                          field, elem->text);
            return;
        }
        long prefix;
        if (!sem_parse_number(slash + 1, &prefix) || prefix > 32) {
            diag_semantic(diags, rule->span, SEM_FAULT_CIDR_PREFIX,
                          field, elem->text);
        }
        return;
    }
    }
}

static void check_endpoint(const Rule *rule, const Endpoint *ep,
                           const char *field, DiagList *diags)
{
    if (ep->kind == ENDPOINT_ANY) {
        return;
    }

    size_t negated = 0;
    for (size_t i = 0; i < ep->count; i++) {
        const AddrElem *elem = &ep->items[i];
        negated += elem->negated ? 1u : 0u;

        check_addr_elem(rule, elem, field, diags);

        /* exact duplicates: same kind, same negation, same text */
        for (size_t j = 0; j < i; j++) {
            const AddrElem *prev = &ep->items[j];
            if (prev->kind == elem->kind && prev->negated == elem->negated
                && strcmp(prev->text, elem->text) == 0) {
                diag_semantic(diags, rule->span, SEM_FAULT_DUP_LIST_ENTRY,
                              field, elem->text);
                break;
            }
        }
    }
    check_all_negated(rule, ep->is_list, ep->count, negated, field, diags);
}

/* ---- ports --------------------------------------------------------------*/

typedef struct PortInterval {
    long lo, hi;
    int negated;
    int ok;   /* bounds parsed and in 1..65535 */
} PortInterval;

static int bound_ok(const char *text, long *out)
{
    return sem_parse_number(text, out) && *out >= 1 && *out <= 65535;
}

/* Renders the element as the author wrote it, for diagnostic Value lines. */
static void elem_text(const PortElem *e, char *buf, size_t size)
{
    const char *neg = e->negated ? "!" : "";
    switch (e->kind) {
    case PORT_ELEM_SINGLE:    snprintf(buf, size, "%s%s", neg, e->lo);   break;
    case PORT_ELEM_RANGE:     snprintf(buf, size, "%s%s:%s", neg, e->lo, e->hi); break;
    case PORT_ELEM_OPEN_HIGH: snprintf(buf, size, "%s%s:", neg, e->lo);  break;
    case PORT_ELEM_OPEN_LOW:  snprintf(buf, size, "%s:%s", neg, e->hi);  break;
    }
}

/* Normalizes one element to [lo,hi]; emits at most one ERROR. */
static PortInterval normalize_port_elem(const Rule *rule, const PortElem *e,
                                        const char *field, DiagList *diags)
{
    PortInterval iv = { 1, 65535, e->negated, 0 };
    char text[64];
    elem_text(e, text, sizeof(text));

    int bounds_ok = 1;
    if (e->lo != NULL && !bound_ok(e->lo, &iv.lo)) {
        bounds_ok = 0;
    }
    if (e->hi != NULL && !bound_ok(e->hi, &iv.hi)) {
        bounds_ok = 0;
    }
    if (e->kind == PORT_ELEM_SINGLE) {
        iv.hi = iv.lo;
    }

    if (!bounds_ok) {
        diag_semantic(diags, rule->span, SEM_FAULT_PORT_RANGE, field, text);
        return iv;
    }
    if (iv.lo > iv.hi) {
        diag_semantic(diags, rule->span, SEM_FAULT_RANGE_ORDER, field, text);
        return iv;
    }
    iv.ok = 1;
    return iv;
}

static void check_port_spec(const Rule *rule, const PortSpec *ps,
                            const char *field, DiagList *diags)
{
    if (ps->kind == PORT_ANY) {
        return;
    }

    enum { MAX_TRACKED = 64 };
    PortInterval seen[MAX_TRACKED];
    size_t tracked = 0;
    size_t negated = 0;

    for (size_t i = 0; i < ps->count; i++) {
        const PortElem *elem = &ps->items[i];
        negated += elem->negated ? 1u : 0u;

        PortInterval iv = normalize_port_elem(rule, elem, field, diags);
        if (!iv.ok) {
            continue;
        }

        char text[64];
        elem_text(elem, text, sizeof(text));
        for (size_t j = 0; j < tracked; j++) {
            const PortInterval *prev = &seen[j];
            if (prev->lo == iv.lo && prev->hi == iv.hi
                && prev->negated == iv.negated) {
                diag_semantic(diags, rule->span, SEM_FAULT_DUP_LIST_ENTRY,
                              field, text);
                break;
            }
            /* strictly contained in a wider non-negated entry */
            if (!prev->negated && !iv.negated
                && prev->lo <= iv.lo && iv.hi <= prev->hi
                && !(prev->lo == iv.lo && prev->hi == iv.hi)) {
                diag_semantic(diags, rule->span, SEM_FAULT_REDUNDANT_ENTRY,
                              field, text);
                break;
            }
        }
        if (tracked < MAX_TRACKED) {
            seen[tracked++] = iv;
        }
    }
    check_all_negated(rule, ps->is_list, ps->count, negated, field, diags);
}

void sem_validate_header(SemanticContext *sctx, const Rule *rule,
                         DiagList *diags)
{
    (void)sctx;

    /* Protocol and direction are grammar-constrained enums — the parser
     * cannot construct an invalid one; cross-field coherence (icmp vs
     * ports) lives in the rule-level pass. */

    check_endpoint(rule, &rule->src_ip, "SrcIP", diags);
    check_port_spec(rule, &rule->src_port, "SrcPort", diags);
    check_endpoint(rule, &rule->dst_ip, "DstIP", diags);
    check_port_spec(rule, &rule->dst_port, "DstPort", diags);
}
