# semantic

Value-level and cross-rule validation over completed Rule objects. The
parser constructs structure; this layer evaluates meaning; diagnostics
narrates failures.

Layout:
- `semantic.c` — opaque SemanticContext + the ordered pass table
- `sid_registry.*` — O(1) open-addressing sid → first-occurrence map, the
  only state that survives across rules (streaming model)
- `validators/header.c` — IP octets, CIDR prefixes, port ranges
- `validators/options.c` — sid discipline (presence/unique/numeric/positive
  + cross-rule duplicates), rev numeric, empty msg/content, unknown keys
- `validators/rule.c` — cross-field coherence (ports on icmp)

Runs only on rules that parsed cleanly, inside `dispatch_rule_accepted`,
immediately before the rule is freed. Never touches tokens or grammar.
Emits SEMANTIC diagnostics (Field/Value form) — multiple per rule allowed;
WARNINGs do not invalidate. All prose lives in `diagnostics/` (fault
table). New checks extend a validator file or add a pass to the table —
never the parser.
