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

## Phase 5 — Semantic validation
Validator pass over Rule objects (hooks into dispatch_rule_accepted): port
range, IP octets, CIDR mask, missing SID, duplicate SID (registry),
option-value typing (sid/rev want numbers). SEMANTIC diagnostics distinct
from SYNTAX in reports. Semantic golden tests.

## Phase 6 — Advanced grammar
Negation, port ranges and lists, bracketed IP lists. Grammar grows;
recovery and diagnostics architecture unchanged.

## Phase 7 — Hardening & polish
10k-rule stress runs with timing budget, max-errors cap, summary statistics,
optional JSON renderer, docs completion.
