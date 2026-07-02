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

Phase 1 complete — architecture, module boundaries, and scaffolding.
See [docs/ROADMAP.md](docs/ROADMAP.md) for the phased plan.

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
