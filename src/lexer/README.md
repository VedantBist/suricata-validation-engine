# lexer

Flex specification (`rules.l`) and lexer support code. Sole responsibility:
character → token classification with line/column stamping. Emits an explicit
EOL token (newlines are grammar-visible — they are the parser's recovery
anchor) and a synthetic EOL at EOF.

The lexer classifies, it never judges: `999.999.999.999` is a well-formed IP
*token* here; rejecting its value is `semantic/`'s job. It builds no
structures, validates no values, prints nothing. Unclassifiable characters
and unterminated strings become LEXICAL diagnostics.

Single source of truth for line/column tracking and rule numbering.
