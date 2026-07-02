# tests

Golden-file test suite. Each case is an input rule file in `data/` paired
with an expected-output file in `expected/` (mirrored layout); the runner
executes the binary and diffs verbatim.

- `data/valid/` — must produce zero diagnostics
- `data/syntax_errors/` — one malformed construct per file; valid rules are
  deliberately placed *after* broken ones, because error recovery is the
  tested property
- `data/semantic_errors/` — parse-clean rules with invalid values
- `data/stress/` — generator script for ~10k-rule files (generated data is
  gitignored; only the generator is committed)

Exit codes are part of the contract and asserted: 0 clean, 1 violations,
2 engine failure. See docs/ARCHITECTURE.md §8.
