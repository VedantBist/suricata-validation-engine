# suricata-validation-engine

Production-grade validation engine for Suricata IDS/IPS rules using Flex/Lex
and Yacc/Bison with CFG-based syntax parsing, semantic validation, and
structured diagnostics.

The engine validates large rule files (10,000+ rules), recovers gracefully
from malformed rules instead of terminating, and reports parser-state-aware
diagnostics:

```
Rule #34
Line #34

Expected:  SrcIP
Found:     ARROW

Error: Source IP missing after protocol
```

## Status

Phase 4 complete — recursive options parsing: full rule structure
`action header (key:value; ...)` with structured Option models, leak-free
recursive cleanup under panic recovery, and option-region Expected/Found
diagnostics. The Phase 3 backbone (zero-conflict grammar, `error EOL`
line-level recovery, streaming one-rule-at-a-time lifecycle) is unchanged
and re-verified byte-identical. Semantic validation arrives in Phase 5.
See [docs/ROADMAP.md](docs/ROADMAP.md) for the phased plan.

## Build & run

```sh
make                 # debug build (UBSan) -> build/bin/suricata-validate
make release         # optimized build
make test            # golden-file suite; STRESS=1 make test adds the 10k tiers
build/bin/suricata-validate samples/header_only.rules   # validate (default)
build/bin/suricata-validate --dump-tokens samples/demo.rules
SV_PARSER_TRACE=1 build/bin/suricata-validate file      # recovery trace (debug builds)
```

Requires flex and bison ≥ 3.6 (macOS: `brew install bison`; the Makefile
auto-prefers the Homebrew install over the ancient system bison 2.3).

## Layout

| Path | Responsibility |
|------|----------------|
| `src/core/` | Orchestration: engine lifecycle, run loop, exit codes |
| `src/lexer/` | Flex tokenization + position tracking (classifies, never judges) |
| `src/parser/` | Bison grammar, error recovery, Expected/Found capture |
| `src/models/` | Rule, Option, Diagnostic, ParserContext data types |
| `src/semantic/` | Value + cross-rule validators (ports, IP/CIDR, SIDs) |
| `src/diagnostics/` | Diagnostic collection, human vocabulary, explanations |
| `src/reporting/` | Deterministic renderers for diagnostics + run summary |
| `src/utils/` | Generic helpers, zero project knowledge |
| `tests/` | Golden-file suite: valid / syntax / semantic / stress |
| `docs/` | [Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Grammar log](docs/GRAMMAR.md) |
| `samples/` | Demo rule files for manual runs |

Full design: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
