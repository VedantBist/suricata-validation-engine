# Implementation Roadmap

Each phase is a small, shippable increment. A phase is done when its tests
pass and `docs/` reflects any design deltas. No phase implements ahead of its
scope.

## Phase 1 — Architecture & scaffolding  ✅ (this commit)
Repository structure, module boundaries, data flow, diagnostics/recovery
design, build & testing strategy. No lexer/parser code.

## Phase 2 — Toolchain + minimal lexer  ✅
Makefile (debug/release, generated-file management), `rules.l` for the initial
token set (actions, protocols, IP/CIDR/PORT/ANY/VARIABLE, ARROW/BIDIR, parens,
colon/semicolon, strings, EOL), line/column tracking, a token-dump mode to
inspect lexer output directly. Golden tests for tokenization. Shipped with:
parser stub owning the token inventory, lexical diagnostics
(INVALID_TOKEN + unterminated strings), stress tier green at 10k rules /
220k tokens under UBSan + leak check.

## Phase 3 — Minimal parser + recovery backbone  ✅
`grammar.y` for the header-only rule grammar (options deferred entirely to
Phase 5), the `error → EOL` recovery production, per-rule VALID/INVALID
verdicts, exit codes. The key deliverable: a file with broken rule N still
validates rules N+1..end. Golden tests for recovery. Shipped with:
parser-state-aware Expected/Found diagnostics (parse.error custom +
parse.lac full + progress cursor), Rule model with ownership transfer and
%destructor discipline, streaming dispatch seam (core/dispatch.h),
parser stress tier (10k rules, 2% malformed: valid+invalid==total,
diagnostics==invalid, leaks clean).

*(The diagnostics engine originally planned as its own phase shipped inside
Phase 3 — parse.error custom, LAC, role vocabulary, and the structured
report format all arrived with the parser core. Numbering below reflects
the actual phasing.)*

## Phase 4 — Recursive options parsing  ✅
Options block added to the rule grammar (left-recursive `key:value;` list),
structured Option/OptionList models with a one-direction ownership chain,
full %destructor coverage for recovery-time cleanup, option-region
diagnostics classes (COLON/Value/SEMICOLON/OptionKey/close). Recovery stays
line-level on the Phase 3 EOL anchor. Stress tier upgraded to full rules
with recursive option lists and option-level corruption — leak-free.

## Phase 5 — Semantic validation  ✅
Semantic layer hooked into dispatch_rule_accepted: modular validator passes
(semantic/validators/{header,options,rule}.c behind an ordered pass table) —
IP octets, CIDR prefixes, port ranges, sid presence/uniqueness/numeric/
positive, cross-rule duplicate sid via an O(1) open-addressing registry
(the only cross-rule state), rev numeric, empty msg (warning) / content
(error), unknown option keys (moved from syntax to semantics via the
`option_key → OPTION_KEY | IDENT` grammar delta), icmp/port coherence
warnings. Field/Value semantic diagnostics, INVALID (semantic) verdicts,
`--syntax-only` mode, four semantic golden suites, full-pipeline stress
tier with dual corruption (diagnostics == syntax + semantic invariant).

## Phase 6 — Advanced header grammar  ✅
Negation (field-level and per-element), port ranges (closed + both open
forms via the existing COLON token), bracketed port/IP lists with
per-element negation, mixed compositions. Element-container models
(AddrElem/PortElem inside flat Endpoint/PortSpec — no AST). Nested lists
impossible by grammar shape; zero conflicts preserved. Semantic extensions:
range ordering/bounds via canonical [lo,hi] normalization, duplicate
entries (error), subsumed entries + fully-negated lists (warnings),
per-element IP/CIDR checks. New diagnostics classes for list-interior and
delimiter positions. Stress baseline upgraded to advanced forms
(warning-free by construction) with list corruptions — invariants and
leak-freedom preserved.

## Phase 7 — Hardening, reporting & productionization  ✅
Deterministic JSON reporting (schema v1) as a pure renderer over the same
diagnostics/counters as text output; O(1) summary statistics with an
engine-level accounting self-check (exit 2 on violation); production CLI
(--json/--quiet/--summary/--max-errors/--timing/--version, robust argument
validation); max-errors fail-fast that stops at rule boundaries via
YYACCEPT with full destructor cleanup; timing instrumentation on stderr
(stdout stays golden-diffable); stress infrastructure expanded to five
tiers (lexer, full-pipeline with invariants + timing smoke, JSON
integration cross-check, corruption-heavy 20%+10%, byte-determinism);
CLI golden suite; final documentation (invariant philosophy, reporting
architecture, README production overhaul). Grammar untouched.
