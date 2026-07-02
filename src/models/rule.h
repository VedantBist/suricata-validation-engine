#ifndef MODELS_RULE_H
#define MODELS_RULE_H

#include "models/option.h"
#include "models/source_location.h"

/* Structured result of parsing one rule: header (Action Protocol SrcIP
 * SrcPort Direction DstIP DstPort) plus an optional options block (Phase 4).
 * Deliberately a flat struct, not an AST hierarchy: semantic validation
 * needs fields, not trees. */

typedef enum RuleAction {
    ACTION_ALERT,
    ACTION_DROP,
    ACTION_PASS
} RuleAction;

typedef enum RuleProtocol {
    PROTO_TCP,
    PROTO_UDP,
    PROTO_ICMP
} RuleProtocol;

typedef enum RuleDirection {
    DIRECTION_TO,     /* -> */
    DIRECTION_BIDIR   /* <> */
} RuleDirection;

/* Advanced header structures (Phase 6): every address/port field is either
 * ANY or a flat element container — one element for `80`, several for
 * `[80,443,!8080]`. Negation lives at two levels: on the container
 * (`!80`, `![80,443]`) and per element (`[80,!443]`). Nested lists are
 * impossible by grammar construction, so the model never needs depth.
 * Values are stored as raw text, not judged — "999.1.1.1" and "1024:80"
 * live here untouched until the semantic pass normalizes them. */

typedef enum EndpointKind {
    ENDPOINT_ANY,
    ENDPOINT_EXPR    /* one or more AddrElems */
} EndpointKind;

typedef enum AddrElemKind {
    ADDR_ELEM_IP,
    ADDR_ELEM_CIDR,
    ADDR_ELEM_VARIABLE
} AddrElemKind;

typedef struct AddrElem {
    AddrElemKind kind;
    int negated;
    char *text;      /* heap-owned raw lexeme */
} AddrElem;

typedef struct Endpoint {
    EndpointKind kind;
    int negated;     /* container-level: !X or ![...] */
    int is_list;     /* was bracketed */
    AddrElem *items;
    size_t count;
    size_t capacity;
} Endpoint;

typedef enum PortKind {
    PORT_ANY,
    PORT_EXPR        /* one or more PortElems */
} PortKind;

typedef enum PortElemKind {
    PORT_ELEM_SINGLE,     /* 80        lo set        */
    PORT_ELEM_RANGE,      /* 80:90     lo and hi set */
    PORT_ELEM_OPEN_HIGH,  /* 1024:     lo set        */
    PORT_ELEM_OPEN_LOW    /* :65535    hi set        */
} PortElemKind;

typedef struct PortElem {
    PortElemKind kind;
    int negated;
    char *lo;        /* heap-owned; NULL where the kind says so */
    char *hi;
} PortElem;

typedef struct PortSpec {
    PortKind kind;
    int negated;
    int is_list;
    PortElem *items;
    size_t count;
    size_t capacity;
} PortSpec;

/* Container helpers (models own lifecycle; append takes ownership of the
 * element's strings). *_free_contents releases the element array and every
 * owned string but not the container itself — containers live by value on
 * the parser stack and inside Rule. */
void endpoint_init_any(Endpoint *ep);
void endpoint_init_expr(Endpoint *ep);
void endpoint_append(Endpoint *ep, AddrElem elem);
void endpoint_free_contents(Endpoint *ep);

void portspec_init_any(PortSpec *ps);
void portspec_init_expr(PortSpec *ps);
void portspec_append(PortSpec *ps, PortElem elem);
void portspec_free_contents(PortSpec *ps);

typedef struct Rule {
    RuleAction action;
    RuleProtocol protocol;
    Endpoint src_ip;
    PortSpec src_port;
    RuleDirection direction;
    Endpoint dst_ip;
    PortSpec dst_port;
    OptionList *options;   /* NULL when the rule has no options block */
    SrcSpan span;          /* provenance: rule number + line range */
} Rule;

/* Ownership contract (ARCHITECTURE.md §5): parser actions construct the
 * Rule and transfer it to core at dispatch; core frees it after validation.
 * Rules are never retained for the whole run (streaming model). */
Rule *rule_create(void);
void rule_free(Rule *rule);

#endif /* MODELS_RULE_H */
