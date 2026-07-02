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

Phase 5 complete — semantic validation engine: modular validator passes
over parsed Rule objects (IP/CIDR/port ranges, sid discipline with an O(1)
cross-rule registry, rev typing, empty strings, unknown keys, cross-field
coherence), Field/Value semantic diagnostics fully separated from syntax
diagnostics, `INVALID (semantic)` verdicts, and a `--syntax-only` mode.
The syntax/semantics boundary is now demonstrable:
`alert tcp any any -> any 70000` parses (syntax VALID) and fails
validation (port out of range). Parser backbone unchanged.
See [docs/ROADMAP.md](docs/ROADMAP.md) for the phased plan.

## Build & run

```sh
make                 # debug build (UBSan) -> build/bin/suricata-validate
make release         # optimized build
make test            # golden-file suite; STRESS=1 make test adds the 10k tiers
build/bin/suricata-validate samples/demo.rules          # full validation (default)
build/bin/suricata-validate --syntax-only file.rules    # structural lint only
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
