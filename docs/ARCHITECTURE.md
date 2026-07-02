# System Architecture — Suricata Rule Validation Engine

Status: Phase 1 (design baseline). This document is the engineering reference for
all implementation phases. Changes to module boundaries or data flow must be
reflected here first.

---

## 1. Design goals

1. Validate large Suricata rule files (~10,000+ rules) in a single pass.
2. Never terminate on malformed input — recover and continue at the next rule.
3. Produce structured, human-readable diagnostics: rule #, line #, expected
   token(s), found token, explanation.
4. Keep lexical, syntactic, and semantic analysis strictly separated.
5. Stay maintainable: each module has one owner-responsibility and a small,
   explicit interface. No module prints from inside another module's domain.

Non-goals (permanently): full Suricata grammar coverage on day one, regex-only
validation, any parser generator other than Flex/Bison.

---

## 2. Repository structure

```
suricata-validation-engine/
├── Makefile                     # build entry point (Phase 2)
├── README.md
├── docs/
│   ├── ARCHITECTURE.md          # this document
│   ├── ROADMAP.md               # phased implementation plan
│   └── GRAMMAR.md               # grammar evolution log, one section per phase
├── src/
│   ├── core/                    # CLI (main.c), run orchestration (validate.c),
│   │                            #   dispatch seam, token-dump mode, exit codes
│   ├── lexer/                   # rules.l — tokenization + position tracking ONLY
│   ├── parser/                  # grammar.y — structure recognition + error recovery ONLY
│   ├── models/                  # Rule, Endpoint/PortSpec element containers,
│   │                            #   Option, Diagnostic, ParserContext
│   ├── semantic/                # semantic.c (pass table), sid_registry,
│   │   └── validators/          #   header.c / options.c / rule.c passes
│   ├── diagnostics/             # diagnostic collection, expected-class mapping,
│   │                            #   syntax + semantic fault tables (ALL prose)
│   ├── reporting/               # report_text (verdicts/diagnostics/summary),
│   │                            #   report_json (schema v1)
│   └── utils/                   # checked allocation helpers
├── tests/
│   ├── data/
│   │   ├── lexer/               # token-dump goldens
│   │   ├── parser/              # syntax + recovery goldens (--syntax-only)
│   │   ├── semantic/            # full-pipeline goldens (--validate)
│   │   ├── cli/                 # output-mode and policy goldens
│   │   └── stress/              # deterministic generator (seeded) for 10k tiers
│   ├── expected/                # golden output files, mirrors data/ layout
│   └── run_tests.sh             # golden-file test runner + stress tiers
├── samples/                     # small demo files for manual runs
├── configs/                     # future: severity config, limits (e.g. max-errors)
└── build/                       # ALL generated artifacts — gitignored, never committed
    ├── gen/                     # bison/flex output (parser.tab.c/.h, lex.yy.c)
    ├── obj/                     # object files
    └── bin/                     # final binary: suricata-validate
```

Header policy: each module keeps its public header next to its sources
(`src/models/rule.h`, `src/diagnostics/diagnostics.h`, ...). Compilation uses
`-Isrc`, so includes are namespaced by module: `#include "models/rule.h"`.
A module may include another module's public header; it may never reach into
another module's `.c` internals.

### Module responsibilities and ownership boundaries

| Module        | Owns                                                                 | Must never do                                  |
|---------------|----------------------------------------------------------------------|------------------------------------------------|
| `core`        | Engine lifecycle, file handling, per-run orchestration, exit codes, stats | Grammar knowledge, validation logic         |
| `lexer`       | Character → token classification, line/column tracking, lexical errors | Validate values, build structures, print     |
| `parser`      | Token → structure, grammar conformance, error recovery, Expected/Found capture | Validate values, print, format messages |
| `models`      | Data type definitions, constructors/destructors, memory lifecycle    | Business logic of any kind                     |
| `semantic`    | Value checks (port range, IP octets, CIDR mask) and cross-rule checks (SID registry) | Touch tokens or grammar             |
| `diagnostics` | Diagnostic accumulation, token→human vocabulary mapping, explanation lookup | Render output, decide exit codes         |
| `reporting`   | Turning collected diagnostics + stats into output formats            | Create or interpret diagnostics                |
| `utils`       | Generic helpers with zero project knowledge                          | Depend on any other module                     |

Dependency direction (arrows = "may depend on"):

```
core ──> parser ──> lexer
  │        │
  │        └──────> models, diagnostics
  ├──> semantic ──> models, diagnostics
  ├──> reporting ─> models
  └──> models, diagnostics, utils        (everything may use utils)
```

`reporting` and `semantic` never depend on the parser or lexer. The lexer and
parser never depend on `semantic` or `reporting`. This is what keeps
"syntax vs semantics" a real boundary instead of a comment.

---

## 3. System architecture & data flow

```
                ┌─────────────────────────────────────────────┐
                │                  core/engine                 │
                │  open file → run parse loop → run summary    │
                └───────┬──────────────────────────┬──────────┘
                        │ drives                   │ after EOF
                        ▼                          ▼
        ┌────────────── parser (Bison) ──────┐   reporting
        │ requests tokens; builds Rule;      │   (render diagnostics
        │ on error: recover to EOL,          │    + statistics)
        │ capture Expected/Found             │
        └───────┬──────────────┬─────────────┘
                │ yylex()      │ per completed Rule
                ▼              ▼
         lexer (Flex)     core: dispatch Rule ──> semantic validators
         tokens + line/col         │                    │
                                   └────────┬───────────┘
                                            ▼
                                   diagnostics engine
                                   (accumulates Diagnostic records)
```

Key architectural decisions:

- **Streaming, not whole-file AST.** Rules are validated one at a time as the
  parser completes them, then freed. At 10k+ rules we never hold the full file
  as objects in memory. The only cross-rule state retained is a lightweight
  SID registry (`sid → first rule#/line#`) for duplicate detection.
- **One shared `ParserContext`.** All per-run state (current rule #, current
  line, partial rule under construction, diagnostics sink, stats) lives in a
  single context struct owned by `core` and threaded to the parser via
  Bison's `%parse-param`. The lexer accesses it through one documented
  accessor. No scattered globals. (If reentrancy is ever needed, this is the
  single seam to upgrade — reentrant Flex/Bison accept the same struct.)
- **Diagnostics are data.** Nothing in `src/` prints during analysis. Every
  problem becomes a `Diagnostic` record in the context; `reporting` renders
  them once, after the run. Per-rule verdicts stream as rules complete —
  emitted by core's dispatch layer through `reporting`, never by the parser.
- **The dispatch seam.** One deliberate inversion of the core→parser arrow:
  grammar actions call `core/dispatch.h` (`dispatch_rule_accepted` /
  `dispatch_rule_rejected`) as each line completes. This is what keeps the
  pipeline streaming — the parser hands each Rule off immediately instead of
  accumulating results for core to collect. The interface is two functions;
  semantic validation (Phase 6) hooks inside `dispatch_rule_accepted`.

---

## 4. Rule lifecycle (parser pipeline)

The lifecycle of one line of input:

```
1. core          reads the file, starts yyparse() once for the whole file
2. lexer         classifies characters into tokens (ACTION, PROTOCOL, IP,
                 CIDR, PORT, ARROW, ...), stamping each with line/column;
                 emits EOL at newline; counts rule numbers on non-blank lines
3. parser        matches tokens against the grammar
     ├─ success: grammar actions assemble a Rule object (models/)
     │           → core dispatches it to semantic validation
     │              ├─ clean  → mark VALID, free Rule
     │              └─ issues → Diagnostic(SEMANTIC) records, mark INVALID, free Rule
     └─ failure: Bison enters error state
                 → diagnostics captures Expected set + Found token + position
                 → recovery rule discards tokens up to EOL, parser resumes
                 → partial Rule freed, rule marked INVALID(SYNTAX)
4. diagnostics   accumulates all records for the run
5. reporting     after EOF: renders each diagnostic + a run summary
                 (total / valid / syntax-invalid / semantic-invalid)
6. core          returns exit code (0 = all valid, 1 = violations found,
                 2 = engine/IO failure)
```

Stage contract: a rule that fails **syntax** never reaches **semantic**
validation (there is no trustworthy structure to validate). A rule that parses
cleanly always reaches semantic validation, even if a previous rule failed.

Worked example of the syntax/semantic boundary:

```
alert tcp any any -> any 70000 (msg:"x"; sid:5;)
```
- Lexer: `70000` tokenizes as PORT (lexer classifies, never judges values)
- Parser: structure matches the grammar → syntactically VALID, Rule built
- Semantic: port validator rejects 70000 (> 65535) → SEMANTIC diagnostic

---

## 5. Internal data models (conceptual — implemented in `src/models/`)

**Rule** — the structured result of parsing one rule.
```
Rule {
  action        (alert | drop | pass)
  protocol      (tcp | udp | icmp)
  src_ip, dst_ip     : typed value (ANY | IP | CIDR | VARIABLE) + lexeme
  src_port, dst_port : typed value (ANY | PORT) + lexeme
  direction     (-> | <>)
  options       : list of Option
  rule_number, line   (provenance for diagnostics)
}
```

**Option** — one `key:value;` pair inside `( ... )` (implemented Phase 4).
```
Option { key, value_kind (STRING|NUMBER|IDENT), value, span }
```
Ownership chain is one-directional: lexeme → Option → OptionList → Rule.
Each parser %destructor frees exactly its own level, which is what makes
panic-mode recovery leak-free at any reduction depth.

**Diagnostic** — one problem, fully self-describing (renderable without any
other context).
```
Diagnostic {
  category     LEXICAL | SYNTAX | SEMANTIC
  severity     ERROR | WARNING
  rule_number, line, column
  expected     human vocabulary, e.g. "SrcIP"        (syntax only)
  found        token name + lexeme, e.g. ARROW "->"  (syntax only)
  message      one-line statement of the defect
  explanation  human-readable elaboration, e.g.
               "Source IP missing after protocol"
}
```

**ParserContext** — all mutable per-run state, owned by core.
```
ParserContext {
  current_rule_number, current_line, current_column
  partial_rule          (Rule under construction; freed on error)
  diagnostics           (growable list of Diagnostic)
  sid_registry          (sid → first occurrence, for duplicate detection)
  stats                 (RunStats)
}
```

**ValidationResult / RunStats** — the run outcome for reporting and exit codes.
```
RunStats {
  total_rules, valid, syntax_invalid, semantic_invalid
  diagnostics_count per category
}
```

Memory ownership: whoever constructs frees, except Rule, which is constructed
by parser actions and ownership transfers to core at dispatch; core frees it
after validation. Diagnostics live for the whole run inside the context and
are freed by core at shutdown.

---

## 6. Diagnostics & recovery architecture

### Position and rule tracking
- The lexer is the single source of truth for `line`/`column` (it advances
  them; nothing else may).
- Rule number = ordinal of the non-blank, non-comment line, incremented by the
  lexer when it emits EOL for a line that produced tokens. Blank lines and
  comments do not consume rule numbers.

### Syntax errors — Expected vs Found
- The grammar is built with `%define parse.error custom`, which gives us a
  `yyreport_syntax_error` hook with access to the parser's own tables: the
  exact set of token kinds acceptable in the current state, plus the offending
  lookahead. This is ground truth — Expected/Found is *derived from parser
  state*, never guessed by pattern-matching the input.
- `%define parse.lac full` (lookahead correction) makes those expected sets
  exact: the parser verifies a lookahead is eventually shiftable before
  committing reductions, so errors surface at the first offending token and
  the expected list never contains impossible tokens.
- The parser classifies the expected set structurally (action / protocol /
  address / port / direction / end-of-rule); the diagnostics module turns
  that into role vocabulary and narrative. Src vs Dst positions have
  identical token sets, so a *progress cursor* (RuleProgress in the context,
  advanced by grammar reduction actions, reset at every EOL) disambiguates —
  still parser reduction state, never input guessing. Structure from the
  parser, language from diagnostics: the grammar file stays free of prose.

### Recovery strategy (the core requirement)
- The lexer emits an explicit `EOL` token — newlines are grammar-visible, not
  skipped whitespace. This gives the grammar a guaranteed synchronization
  point aligned with the "one rule per line" input contract. `EOL` is emitted
  only for lines that produced at least one token; blank and comment-only
  lines emit nothing, so the token stream is exactly a sequence of
  `<rule tokens> EOL` groups and the grammar needs no empty-line production.
- The grammar's top level is a list of lines, with a dedicated recovery
  production conceptually of the form: *on error, discard everything up to
  the next EOL, record the diagnostic, reset error state (`yyerrok`), destroy
  the partial Rule, continue*. Panic-mode recovery with EOL as the
  synchronization token.
- Corner cases the design commits to handling:
  - **Error at EOF** (last line malformed, no trailing newline): lexer emits a
    synthetic EOL at EOF so the recovery production always has its anchor.
  - **Unterminated string** (`msg:"HTTP` ... newline): the lexer terminates
    the string at the newline and emits a LEXICAL diagnostic, so one bad quote
    cannot swallow subsequent rules.
  - **Cascading errors**: at most one SYNTAX diagnostic per rule line;
    everything after the first error on a line is discarded silently.

### Lexical errors
Characters the lexer cannot classify produce a LEXICAL diagnostic (with
position) and are surfaced to the parser as a real `INVALID_TOKEN` — not
silently dropped — so syntax diagnostics can report
`Found: INVALID_TOKEN "@"` from genuine parser state. One character at a
time, so scanning always makes progress.

### Semantic errors
Emitted only by `semantic/` validators operating on completed Rule objects,
inside `dispatch_rule_accepted` — after a clean parse, before the rule is
freed. Organization: an ordered table of independent passes
(`validators/header.c`, `validators/options.c`, `validators/rule.c`);
adding a check never touches the parser. Checks: port range (1–65535), IP
octet ranges, CIDR mask bounds (/0–/32), sid presence/uniqueness/numeric/
positive (cross-rule duplicates via the SID registry), rev numeric, empty
msg/content, unknown option keys, icmp/port coherence. Multiple semantic
diagnostics per rule are allowed (unlike syntax) — the structure is
trustworthy, so we can report every bad value at once. Severity matters:
WARNINGs (empty msg, ports on icmp) do not invalidate the rule.
Semantic diagnostics carry Field/Value instead of Expected/Found and never
depend on parser state. `--syntax-only` disables the layer entirely
(structural lint; also isolates parser golden tests).

---

## 7. Build & toolchain strategy

Single top-level `Makefile` (implemented in Phase 2). Design:

- **Generated-file policy:** Bison and Flex output goes to `build/gen/`, never
  next to sources, never committed. `.gitignore` covers `build/` entirely.
- **Generation order:** bison runs first (`-d` produces the token header),
  flex second (its scanner includes that header), then compilation of
  generated + handwritten sources into `build/obj/`, linked to
  `build/bin/suricata-validate`.
- **Tool flags (baseline):**
  - bison: `-d -Wall -Wcounterexamples` — counterexamples make grammar
    conflicts debuggable instead of mysterious.
  - flex: warnings enabled; no default rule suppression tricks.
  - cc: `-std=c11 -Wall -Wextra -Isrc` (generated files may get a slightly
    relaxed warning set — we don't fix generator output).
- **Build modes:**
  - `make` (debug) — `-g -O0` + UBSan; sanitizers keep the "free every Rule"
    contract honest. ASan is opt-in (`SAN=address,undefined`): ASan binaries
    hang at startup on some macOS/Xcode combinations (shadow-memory init
    spin, verified on the project dev machine), so the test runner uses
    macOS `leaks --atExit` for heap verification instead. `YYDEBUG=1`
    (Bison state tracing) joins in Phase 3 with the real grammar.
  - `make release` — `-O2 -DNDEBUG`.
  - `make test` — builds debug, runs the golden-file suite.
  - `make clean` — removes `build/` only.
- Zero-conflict policy: the grammar must build with no shift/reduce or
  reduce/reduce conflicts; any deliberate exception must be documented in
  `docs/GRAMMAR.md` with its counterexample analysis.

---

## 8. Testing strategy

Philosophy: **golden-file testing**. The engine's observable product is its
diagnostic output, so tests assert on exactly that. Every test case is an
input rule file paired with an expected-output file; the runner executes the
binary and diffs. This makes every regression visible as a readable diff and
makes adding a test as cheap as writing two small text files.

```
tests/
├── data/
│   ├── valid/              # must produce zero diagnostics
│   ├── syntax_errors/      # one construct per file: missing IP, bad direction,
│   │                       #   missing paren, garbage line, unterminated string...
│   ├── semantic_errors/    # parse-clean, invalid values: port 70000, bad octet,
│   │                       #   /33 CIDR, duplicate SID, missing SID
│   └── stress/             # generate_stress.py → ~10k mixed rules (generated,
│                           #   only the generator is committed)
├── expected/               # golden outputs, mirrors data/
└── run_tests.sh            # runner: execute, diff, report pass/fail table
```

Test design rules:
- **Recovery is the tested property, not an accident.** Syntax-error files
  deliberately place valid rules *after* broken ones; the golden output must
  show the later rules as VALID. A file where rule 3 is garbage and rules 4–10
  are clean is the single most important test in the suite.
- **Determinism:** diagnostics are reported in input order; output contains no
  timestamps or absolute paths, so goldens diff cleanly.
- **Stress tier:** the generator produces ~10k rules with a configurable error
  ratio. Assertions: run completes, `valid + invalid == total`, wall-clock
  under a sane budget, and (in debug builds) zero sanitizer reports — the
  streaming memory model is verified here.
- **Exit codes are part of the contract** and asserted by the runner:
  0 = clean, 1 = violations found, 2 = engine failure.

Unit-level tests (e.g., calling semantic validators directly) come later via a
small test harness binary; golden files are the backbone first because they
test the integrated behavior the project is actually judged on.

---

## 9. Reporting & operational behavior (Phase 7)

- **Renderers are consumers.** Both `report_text` and `report_json` read
  the same inputs — the diagnostics list (insertion order) and the run
  counters — and neither can reach parser or validator state. Adding an
  output format is a new file in `reporting/`, nothing else.
- **JSON schema v1** is deterministic: fixed field order, no timestamps,
  optional fields omitted when inapplicable. Memory for a report is
  bounded by diagnostic count, never rule count — rules were freed as they
  streamed by, so even a 10k-rule file with a JSON report holds only its
  errors in memory.
- **Statistics are O(1)**: severity/category counters are maintained by
  `diag_list_add`, not by rescanning; the summary engine and the
  max-errors cap read the same counters.
- **max-errors lifecycle**: the cap is evaluated in the dispatch layer
  after each completed rule — never mid-rule. When reached, the grammar's
  line actions `YYACCEPT`; bison's accept path destroys any remaining
  stack symbols through the same %destructors recovery uses, so early
  stop is leak-free by construction. The lexer stops with the parser, so
  `rule_count` counts only scanned lines and the accounting invariant
  holds for partial runs too.
- **Timing goes to stderr** (`--timing`): stdout stays byte-deterministic
  and golden-diffable regardless of flags.
- **Engine self-check**: every run verifies
  `valid + syntax-invalid + semantic-invalid == total`; a violation is an
  engine bug and exits 2.

## 10. Invariant philosophy

Invariants here are enforced, not aspirational — each has a mechanism and
a test that fails when it breaks:

| Invariant | Enforced by | Tested by |
|-----------|-------------|-----------|
| Zero grammar conflicts | bison `-Werror` | every build |
| One syntax diagnostic per line | recovery discards + `yyerrok` at EOL | parser goldens |
| Recovery never crosses lines | EOL token contract (synthetic at EOF) | recovery goldens, corruption-heavy stress |
| One rule alive at a time | dispatch frees before next line completes | leaks under stress |
| No leaks under panic recovery | one-destructor-per-ownership-level | leaks on corruption-heavy inputs |
| `valid+syn+sem == total` | engine self-check (exit 2) | every stress tier |
| `diagnostics == syn+sem` on lexically-clean input | 1-error corruption engineering | stress-full, stress-heavy |
| Deterministic output | no timestamps/paths; insertion order | stress-determinism (byte-diff two runs) |

## 11. Extension seams

- **New semantic checks:** add a validator in `semantic/`, register it in the
  dispatch list — no parser changes.
- **New output formats (JSON, summary-only):** add a renderer in `reporting/`
  behind the same diagnostics list — no analysis changes.
- **Grammar growth (variables, negation, port lists):** grammar productions
  and token classes extend; the recovery anchor (EOL) and diagnostics flow are
  unchanged.
- **Configurability (`configs/`):** max-errors cap, severity overrides —
  consumed by core, invisible to lexer/parser.
