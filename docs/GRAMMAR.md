# Grammar Evolution Log

One section per phase. Records the grammar subset in force, token inventory,
and any documented conflict exceptions (with counterexample analysis).
The zero-conflict policy from ARCHITECTURE.md §7 applies.

## Phase 5 — grammar delta: generic option keys (implemented)

One production added (zero conflicts preserved):

```
option_key → OPTION_KEY | IDENT
```

Rationale: "whether a key is supported" is a semantic question, not a
structural one. `foo:"bar";` now parses cleanly and the semantic layer
reports the unknown key — previously it was (mis)classified as a syntax
error. Classifier consequence: option-key positions now include IDENT in
their expected sets, so the key-position classes outrank the value class
in the priority chain (a value position after ':' never contains
OPTION_KEY, so the ordering is unambiguous).

## Phase 4 — recursive options grammar (implemented)

Conflict status: **zero** (unchanged; bison -Werror).

```
rule        → action header
            | action header options      ← lookahead-disjoint: EOL reduces,
options     → ( option_list )              '(' shifts; no empty nonterminal
option_list → option
            | option_list option         ← left recursion: each option
option      → OPTION_KEY : option_value ;  reduces+appends at its ';', stack
option_value→ STRING | NUMBER | IDENT      stays flat at any list length
```

Decisions of record:

- **Recovery granularity stays line-level.** No `error` production inside
  the options block: a malformed option invalidates its rule as a unit and
  recovery syncs on the same EOL anchor as Phase 3. This is what preserves
  the one-syntax-diagnostic-per-line policy; intra-block resynchronization
  (e.g. `error ;`) would report multiple errors per line and is explicitly
  out of scope.
- **Ownership chain, one direction:** lexeme → Option (option_create) →
  OptionList (append) → Rule (attach). Each %destructor frees exactly its
  own level, so panic recovery releases whatever was on the stack —
  in-flight value, half-built option, partially appended list, or completed
  header Rule — exactly once. Verified leak-free under recovery-heavy
  stress.
- **Option-region diagnostics classes:** COLON, Value, SEMICOLON, OptionKey,
  "OptionKey or ')'", "Options or end of rule", end-of-rule — all derived
  from the table-extracted expected sets; the two genuinely mixed sets
  ({OPTION_KEY, ')'} and {'(', EOL}) get dedicated classes rather than
  progress-cursor disambiguation.
- Unknown option keys (`foo:`) lex as IDENT and fail structurally with the
  supported-keys hint; unterminated strings keep the Phase 2 guarantee
  (closed at EOL + lexical diagnostic) and then surface the structural
  error (`Expected: SEMICOLON, Found: end of line`).

## Phase 3 — header grammar + recovery backbone (implemented)

Conflict status: **zero** shift/reduce, **zero** reduce/reduce (bison -Werror).

```
rule_file   → ε | rule_file line
line        → rule EOL
            | error EOL          ← panic-mode recovery, sync on EOL; yyerrok
rule        → action header
header      → protocol src_address src_port direction dst_address dst_port
action      → alert | drop | pass
protocol    → tcp | udp | icmp
src_address → address            ← unit wrappers advance the progress cursor
dst_address → address              with positional knowledge; token shapes
src_port    → port_spec            stay shared
dst_port    → port_spec
address     → any | IP | CIDR | VARIABLE
port_spec   → any | PORT
```

Error-handling configuration of record:

- `%define parse.error custom` — syntax errors route through
  `yyreport_syntax_error`, which reads the expected-token set from the
  parser tables via the yypcontext API. Expected/Found is parser state,
  never input-pattern matching.
- `%define parse.lac full` — lookahead correction. Without LAC, LALR default
  reductions execute before the error is detected and the expected set can
  include impossible tokens; with LAC the parser verifies the lookahead is
  eventually shiftable first, so expected sets are exact and errors are
  reported at the first offending token.
- LAC consequence for the progress cursor: a unit reduction whose follow set
  excludes the offending lookahead never runs its action, so at error time
  the cursor holds the last reduction that had a viable lookahead.
  Diagnostics therefore maps Src/Dst positions with `<=` thresholds
  (see role_for in diagnostics.c), not stage equality.
- Cascade prevention: during recovery bison discards tokens (destructors
  free their values) without reporting; `yyerrok` at the recovery EOL
  re-arms reporting for the next line. Net effect: at most one syntax
  diagnostic per line, and an error on line N can never suppress an early
  error on line N+1.
- `%destructor` on every value-carrying type — panic-mode discards must not
  leak (verified by the stress tier under leaks).

Rules with options blocks are syntax errors in this subset
(`Expected: end of rule`) until Phase 5.

## Phase 2 — token inventory (implemented)

Owned by `src/parser/grammar.y` (stub): Bison generates the canonical enum in
`build/gen/parser.tab.h`; no hand-written token enum exists anywhere.

| Category    | Tokens |
|-------------|--------|
| Actions     | `ALERT` `DROP` `PASS` |
| Protocols   | `TCP` `UDP` `ICMP` |
| Keywords    | `ANY` |
| Operators   | `ARROW ->` `BIDIR <>` |
| Punctuation | `LPAREN` `RPAREN` `COLON` `SEMICOLON` `COMMA` |
| Values      | `IP` `CIDR` `PORT` `NUMBER` `VARIABLE` `STRING` `OPTION_KEY` `IDENT` |
| Structural  | `EOL` (explicit; recovery anchor) |
| Error       | `INVALID_TOKEN` (unclassifiable input, carries the offending lexeme) |

Lexical decisions of record:

- **PORT vs NUMBER** is context, not spelling: bare integers are `PORT` at
  paren depth 0 (header) and `NUMBER` inside the options parens. The depth
  counter resets at every EOL so an unbalanced `)` cannot leak into the next
  rule.
- **The lexer classifies, never judges**: `999.999.999.999` is an `IP` token,
  `70000` is a `PORT` token — value validation is semantic (Phase 6).
- **Option keys** (`msg` `sid` `rev` `content`) lex as `OPTION_KEY` with the
  key in the lexeme; unknown identifiers lex as `IDENT` (a token, not a
  lexical error — rejecting them is the parser/semantic layer's call).
- **Strings** store their lexeme without the surrounding quotes; escapes pass
  through uninterpreted. An unterminated string closes at end of line with a
  LEXICAL diagnostic (still yielding a `STRING` token) so one missing quote
  cannot swallow subsequent rules.
- **EOL** is emitted only for lines that produced tokens; blank/comment lines
  consume no rule numbers. A synthetic `EOL` is emitted at EOF when the last
  line lacks a newline.

## Phase 1 — target subset (design only, not yet implemented)

```
RuleFile   → lines, one rule per line; EOL is a grammar token
Rule       → Action Header Options
Action     → alert | drop | pass
Header     → Protocol SrcIP SrcPort Direction DstIP DstPort
Protocol   → tcp | udp | icmp
SrcIP/DstIP    → any | IP | CIDR | VARIABLE
SrcPort/DstPort→ any | PORT
Direction  → -> | <>
Options    → ( OptionList )
OptionList → Option | OptionList Option
Option     → OPTION_KEY : VALUE ;
Recovery   → error EOL   (panic-mode, sync on end of line)
```
