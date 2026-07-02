# tests

Golden-file test suite. Each case is an input rule file in `data/` paired
with an expected-output file in `expected/` (mirrored layout); the runner
executes the binary and diffs verbatim.

- `data/lexer/` — token-dump goldens (run with --dump-tokens)
- `data/parser/` — validate-mode goldens: clean headers, per-position
  missing-field diagnostics, invalid-token lines, and the canonical recovery
  case (broken rule N, valid rules N+1..end). Valid rules are deliberately
  placed *after* broken ones, because error recovery is the tested property
- `data/semantic_errors/` — parse-clean rules with invalid values (Phase 6)
- `data/stress/` — deterministic generator for ~10k-rule files (generated
  data is gitignored; only the generator is committed); STRESS=1 runs both
  the lexer tier and the parser tier (2% structurally malformed rules,
  asserting valid+invalid==total and diagnostics==invalid)

Exit codes are part of the contract and asserted: 0 clean, 1 violations,
2 engine failure. See docs/ARCHITECTURE.md §8.
