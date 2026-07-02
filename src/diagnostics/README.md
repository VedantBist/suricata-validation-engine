# diagnostics

Diagnostic accumulation and message intelligence. Owns:

- the growable diagnostic list inside `ParserContext`
- the token-set → role-vocabulary mapping (e.g. {ANY, IP, CIDR, VARIABLE}
  after a protocol → "SrcIP")
- the explanation table keyed by (expected role, found token) →
  "Source IP missing after protocol", with a generic fallback

Produces data, never output — rendering belongs to `reporting/`. Message
text lives here so the grammar file stays free of prose.
