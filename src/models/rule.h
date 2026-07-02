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

typedef enum EndpointKind {
    ENDPOINT_ANY,
    ENDPOINT_IP,
    ENDPOINT_CIDR,
    ENDPOINT_VARIABLE
} EndpointKind;

typedef enum PortKind {
    PORT_ANY,
    PORT_NUMBER
} PortKind;

/* text is the raw lexeme (heap-owned by the enclosing Rule), NULL for ANY.
 * Values are stored, not judged — "999.999.999.999" and "70000" live here
 * untouched until the semantic pass. */
typedef struct Endpoint {
    EndpointKind kind;
    char *text;
} Endpoint;

typedef struct PortSpec {
    PortKind kind;
    char *text;
} PortSpec;

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
