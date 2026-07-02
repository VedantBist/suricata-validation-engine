# reporting

Renderers for collected diagnostics and run statistics: human-readable text
first (Rule # / Line # / Expected / Found / Error blocks + run summary),
JSON later. Consumes the diagnostics list and `RunStats`; creates or
interprets nothing.

Output must be deterministic (input order, no timestamps/absolute paths) —
the golden-file test suite diffs it verbatim.
