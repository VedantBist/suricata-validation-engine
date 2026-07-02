# models

Data type definitions and their lifecycle: `Rule`, `Option`, `Diagnostic`,
`ParserContext`, `RunStats`. Constructors and destructors live here; no
business logic of any kind.

Ownership convention: whoever constructs frees — except `Rule`, which is
built by parser actions and handed to `core`, which frees it after semantic
validation (streaming model: rules are not retained for the whole run).
