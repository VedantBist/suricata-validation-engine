# core

Orchestration layer. Owns the engine lifecycle: opening the input file,
creating the `ParserContext`, driving the parse loop, dispatching completed
Rule objects to semantic validation, invoking reporting after EOF, and
translating run results into process exit codes (0 clean / 1 violations /
2 engine failure).

Knows nothing about the grammar and performs no validation itself.
