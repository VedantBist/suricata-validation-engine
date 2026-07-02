# tests

Golden-file test suite. Each case is an input rule file in `data/` paired
with an expected-output file in `expected/` (mirrored layout); the runner
executes the binary and diffs verbatim.

- `data/lexer/` — token-dump goldens (run with --dump-tokens)
- `data/parser/` — syntax goldens (run with --syntax-only, isolating them
  from semantic churn): clean structures, per-position missing-field
  diagnostics, malformed options, invalid-token lines, and the canonical
  recovery case (broken rule N, valid rules N+1..end). Valid rules are
  deliberately placed *after* broken ones, because error recovery is the
  tested property
- `data/semantic/` — full-pipeline goldens (run with --validate):
  parse-clean rules with invalid values, sid discipline including
  cross-rule duplicates, and the canonical syntax-valid/semantic-invalid
  distinction (port 70000)
- `data/cli/` — output-mode and policy goldens over one fixed mixed input:
  --json (validate + syntax-only), --summary, --quiet, --max-errors; plus
  goldenless checks for --help and argument validation (bad flags exit 2)
- `data/stress/` — deterministic generator for ~10k-rule files (generated
  data is gitignored; only the generator is committed); STRESS=1 runs five
  tiers: lexer dump, full pipeline with dual corruption (2%+2%, asserting
  valid+syntax+semantic==total and diagnostics==syntax+semantic, plus a
  --timing smoke check), JSON integration (python parses the report and
  cross-checks totals), corruption-heavy (20%+10% — recovery stability),
  and determinism (two runs byte-diffed)

Exit codes are part of the contract and asserted: 0 clean, 1 violations,
2 engine failure. See docs/ARCHITECTURE.md §8.
