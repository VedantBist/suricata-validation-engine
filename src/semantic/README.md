# semantic

Value-level and cross-rule validation over completed Rule objects: port range
(1–65535), IP octet bounds, CIDR mask bounds, missing SID, duplicate SID (via
the context's SID registry).

Runs only on rules that parsed cleanly. Never touches tokens or grammar.
Emits SEMANTIC diagnostics — multiple per rule are allowed, since the parsed
structure is trustworthy. New checks register in the validator dispatch list;
adding one must never require parser changes.
