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

## Phase 4 — Diagnostics engine
`%define parse.error custom`, Expected-vs-Found capture from parser state,
token-set → role vocabulary mapping ("SrcIP"), explanation table, structured
report format (Rule # / Line # / Expected / Found / Error). Lexical
diagnostics (bad char, unterminated string).

## Phase 5 — Options parsing (flat + recursive)
Option structs built by grammar actions, the options block added to the rule
grammar (flat `key:value;` list first, then recursive/nested forms), Rule
model extended with the option list. Sanitizer/leaks-clean under the stress
generator.

## Phase 6 — Semantic validation
Validator pass over Rule objects: port range, IP octets, CIDR mask, missing
SID, duplicate SID (registry). SEMANTIC diagnostics distinct from SYNTAX in
reports. Semantic golden tests.

## Phase 7 — Advanced grammar
Variables ($HOME_NET), negation, port ranges and lists, bracketed IP lists.
Grammar grows; recovery and diagnostics architecture unchanged.

## Phase 8 — Hardening & polish
10k-rule stress runs with timing budget, max-errors cap, summary statistics,
optional JSON renderer, docs completion.
