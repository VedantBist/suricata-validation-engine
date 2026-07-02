#include "models/rule.h"

#include <stdlib.h>
#include <string.h>

#include "utils/xalloc.h"

void endpoint_init_any(Endpoint *ep)
{
    memset(ep, 0, sizeof(*ep));
    ep->kind = ENDPOINT_ANY;
}

void endpoint_init_expr(Endpoint *ep)
{
    memset(ep, 0, sizeof(*ep));
    ep->kind = ENDPOINT_EXPR;
}

void endpoint_append(Endpoint *ep, AddrElem elem)
{
    if (ep->count == ep->capacity) {
        ep->capacity = ep->capacity ? ep->capacity * 2 : 4;
        ep->items = xrealloc(ep->items, ep->capacity * sizeof(AddrElem));
    }
    ep->items[ep->count++] = elem;
}

void endpoint_free_contents(Endpoint *ep)
{
    for (size_t i = 0; i < ep->count; i++) {
        free(ep->items[i].text);
    }
    free(ep->items);
    ep->items = NULL;
    ep->count = ep->capacity = 0;
}

void portspec_init_any(PortSpec *ps)
{
    memset(ps, 0, sizeof(*ps));
    ps->kind = PORT_ANY;
}

void portspec_init_expr(PortSpec *ps)
{
    memset(ps, 0, sizeof(*ps));
    ps->kind = PORT_EXPR;
}

void portspec_append(PortSpec *ps, PortElem elem)
{
    if (ps->count == ps->capacity) {
        ps->capacity = ps->capacity ? ps->capacity * 2 : 4;
        ps->items = xrealloc(ps->items, ps->capacity * sizeof(PortElem));
    }
    ps->items[ps->count++] = elem;
}

void portspec_free_contents(PortSpec *ps)
{
    for (size_t i = 0; i < ps->count; i++) {
        free(ps->items[i].lo);
        free(ps->items[i].hi);
    }
    free(ps->items);
    ps->items = NULL;
    ps->count = ps->capacity = 0;
}

Rule *rule_create(void)
{
    return xcalloc(1, sizeof(Rule));
}

void rule_free(Rule *rule)
{
    if (rule == NULL) {
        return;
    }
    endpoint_free_contents(&rule->src_ip);
    endpoint_free_contents(&rule->dst_ip);
    portspec_free_contents(&rule->src_port);
    portspec_free_contents(&rule->dst_port);
    option_list_free(rule->options);
    free(rule);
}
