# Grammar Evolution Log

One section per phase. Records the grammar subset in force, token inventory,
and any documented conflict exceptions (with counterexample analysis).
The zero-conflict policy from ARCHITECTURE.md §7 applies.

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
