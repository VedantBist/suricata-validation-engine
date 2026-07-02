# parser

Bison grammar (`grammar.y`) and parser glue. Sole responsibility: token →
structure. Grammar actions assemble Rule objects; on syntax errors the
recovery production discards input up to the next EOL, records one
Expected-vs-Found diagnostic (derived from real parser state via
`%define parse.error custom`), and resumes.

Contract: never terminates the run on malformed input, never prints, never
validates values, contains no message text (human vocabulary lives in
`diagnostics/`). Zero grammar conflicts unless documented in
`docs/GRAMMAR.md`.
