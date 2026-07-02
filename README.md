# suricata-validation-engine

Production-grade validation engine for Suricata IDS/IPS rules, built on
traditional Flex (lexing) and Bison (parsing) with CFG-based syntax
analysis, a layered semantic validator, panic-mode error recovery, and
compiler-grade structured diagnostics.

The engine validates large rule files (10,000+ rules) in a single streaming
pass, recovers from malformed rules instead of terminating, and reports
parser-state-aware diagnostics:

```
Rule #3 | Line 3, Col 11 | SYNTAX ERROR
  Expected: SrcIP
  Found:    ->
  Source IP missing after protocol
  Acceptable tokens here: any, !, [, IP address, CIDR block, variable

Rule #7 | Line 7 | SEMANTIC ERROR
  Field:    DstPort
  Value:    70000
  Destination port exceeds the valid range (1-65535)
  TCP/UDP ports are 16-bit values from 1 to 65535
```

## Supported rule language

```
alert tcp !$HOME_NET [1:1024,!80] -> [10.0.0.1,192.168.1.0/24] [80,443,8080] (msg:"..."; sid:1001; rev:2;)
```

- Actions `alert|drop|pass`, protocols `tcp|udp|icmp`, directions `->|<>`
- Addresses: IPs, CIDRs, `$VARIABLES`, `any`, negation (`!X`, `![...]`,
  per-element `[a,!b]`), bracketed lists
- Ports: singles, ranges (`1:1024`, `1024:`, `:65535`), negation, lists
- Options: `msg`, `sid`, `rev`, `content` (`key:value;` blocks)

Semantic checks: IPv4 octet ranges, CIDR prefixes, port bounds and range
ordering, duplicate/subsumed list entries, sid discipline (present, single,
numeric, positive, unique across the file), rev typing, empty strings,
unknown option keys, protocol/port coherence. Warnings (e.g. ports on icmp)
surface without invalidating the rule.

## Build

Requires flex and bison ≥ 3.6 (macOS: `brew install bison`; the Makefile
auto-prefers the Homebrew install over the ancient system bison 2.3).

```sh
make                 # debug build (UBSan) -> build/bin/suricata-validate
make release         # optimized build
make test            # golden-file suite
STRESS=1 make test   # + 10k-rule stress tiers (invariants, JSON, leaks,
                     #   corruption-heavy recovery, determinism)
```

## Usage

```sh
suricata-validate rules.txt                 # full validation (default)
suricata-validate rules.txt --json          # machine-readable report
suricata-validate rules.txt --summary       # one summary line
suricata-validate rules.txt --quiet         # diagnostics + summary, no verdicts
suricata-validate rules.txt --max-errors 50 # fail-fast after 50 errors
suricata-validate rules.txt --syntax-only   # structural lint, semantics off
suricata-validate rules.txt --timing        # timing/throughput on stderr
suricata-validate --dump-tokens rules.txt   # lexer inspection
```

Exit codes: `0` clean · `1` violations found · `2` engine/usage failure.
Read from stdin with `-`. Debug builds accept `SV_PARSER_TRACE=1` for a
full shift/reduce/recovery trace.

### JSON schema (v1)

```json
{
  "tool": "suricata-validate",
  "schema_version": 1,
  "mode": "validate",
  "stopped_early": false,
  "summary": {
    "total_rules": 10000, "valid_rules": 9581,
    "syntax_invalid": 189, "semantic_invalid": 230,
    "errors": 419, "warnings": 0, "lexical_errors": 0, "lines": 11164
  },
  "diagnostics": [
    { "rule": 34, "line": 34, "category": "SEMANTIC", "severity": "ERROR",
      "field": "DstPort", "value": "70000",
      "message": "Destination port exceeds the valid range (1-65535)",
      "explanation": "TCP/UDP ports are 16-bit values from 1 to 65535" }
  ]
}
```

Optional fields (`column`, `expected`, `found`, `field`, `value`,
`explanation`) are present when applicable. Output is deterministic:
identical input yields byte-identical reports.

## Engineering invariants

The properties the test suite enforces, not just documents:

- **Zero grammar conflicts** — bison runs with `-Werror`; a conflict fails
  the build.
- **EOL-anchored recovery** — a malformed rule discards to its end of line,
  records exactly one syntax diagnostic, and the next rule parses
  unaffected. Recovery cannot cross a line boundary.
- **Streaming** — one rule alive at a time; the only cross-rule state is
  the sid registry (sid → first occurrence). Memory is bounded by
  diagnostics, never by file size.
- **One-direction ownership** — lexeme → element → container → Rule;
  parser destructors free exactly one level each, so panic recovery and
  early stop are leak-free (verified with heap checking under corruption-
  heavy stress).
- **Syntax/semantics separation** — the parser judges structure from its
  own tables (`parse.error custom` + LAC); validators judge meaning from
  Rule objects; neither reaches into the other.
- **Accounting** — `valid + syntax-invalid + semantic-invalid == total`
  (self-checked by the engine every run; violation exits 2).

## Layout

| Path | Responsibility |
|------|----------------|
| `src/core/` | CLI, run orchestration, dispatch seam, exit codes |
| `src/lexer/` | Flex tokenization + position tracking (classifies, never judges) |
| `src/parser/` | Bison grammar, error recovery, Expected/Found capture |
| `src/models/` | Rule, Endpoint/PortSpec containers, Option, Diagnostic, context |
| `src/semantic/` | Validator passes + sid registry (meaning, never structure) |
| `src/diagnostics/` | Diagnostic collection, fault tables, all prose |
| `src/reporting/` | Text + JSON renderers, summary engine |
| `src/utils/` | Generic helpers, zero project knowledge |
| `tests/` | Golden suites: lexer / parser / semantic / cli + stress tiers |
| `docs/` | [Architecture](docs/ARCHITECTURE.md) · [Roadmap](docs/ROADMAP.md) · [Grammar log](docs/GRAMMAR.md) |

Full design rationale: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
