# Grammar Evolution Log

One section per phase. Records the grammar subset in force, token inventory,
and any documented conflict exceptions (with counterexample analysis).
The zero-conflict policy from ARCHITECTURE.md §7 applies.

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
