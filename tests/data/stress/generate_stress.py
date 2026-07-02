#!/usr/bin/env python3
"""Deterministic stress-file generator (seeded PRNG — same output every run,
so assertions on counts are stable). Emits lexically valid rules covering the
Phase 2 token inventory; comment and blank lines are mixed in to exercise
rule-number accounting at scale.

Usage: generate_stress.py [rule_count] [--header-only] [--error-rate F] > out
  --header-only   emit rules without the options block (matches the current
                  parser grammar subset, Phase 3)
  --error-rate F  fraction (0..1) of rules to structurally corrupt — always
                  lexically valid, so `diagnostics == invalid rules` holds
                  (1 syntax diagnostic per malformed line, no lexical noise)

Output goes to stdout; generated files are gitignored — only this script is
committed.
"""

import argparse
import random
import sys

ACTIONS = ["alert", "drop", "pass"]
PROTOCOLS = ["tcp", "udp", "icmp"]
VARIABLES = ["$HOME_NET", "$EXTERNAL_NET", "$DNS_SERVERS"]


def endpoint(rng):
    kind = rng.randrange(4)
    if kind == 0:
        return "any"
    if kind == 1:
        return "%d.%d.%d.%d" % tuple(rng.randrange(1, 254) for _ in range(4))
    if kind == 2:
        ip = "%d.%d.%d.%d" % tuple(rng.randrange(1, 254) for _ in range(4))
        return f"{ip}/{rng.choice([8, 16, 24, 32])}"
    return rng.choice(VARIABLES)


def port(rng):
    return "any" if rng.randrange(3) == 0 else str(rng.randrange(1, 65536))


def corrupt(fields, rng):
    """Structural corruption only — never introduces unlexable characters."""
    kind = rng.randrange(4)
    if kind == 0:
        del fields[rng.randrange(1, len(fields))]   # drop a header field
    elif kind == 1:
        del fields[0]                               # drop the action
    elif kind == 2:
        fields.insert(rng.randrange(len(fields)), "->")  # stray direction
    else:
        fields.append(fields[-1])                   # duplicated trailing field
    return fields


def main():
    p = argparse.ArgumentParser()
    p.add_argument("count", nargs="?", type=int, default=10000)
    p.add_argument("--header-only", action="store_true")
    p.add_argument("--error-rate", type=float, default=0.0)
    args = p.parse_args()

    rng = random.Random(1337)
    emitted = 0
    malformed = 0
    while emitted < args.count:
        # ~5% comment lines, ~5% blank lines interleaved
        roll = rng.randrange(20)
        if roll == 0:
            print(f"# generated comment before rule {emitted + 1}")
            continue
        if roll == 1:
            print()
            continue
        emitted += 1
        action = rng.choice(ACTIONS)
        proto = rng.choice(PROTOCOLS)
        direction = "->" if rng.randrange(4) else "<>"
        fields = [action, proto, endpoint(rng), port(rng), direction,
                  endpoint(rng), port(rng)]
        if not args.header_only:
            fields.append(
                f'(msg:"stress rule {emitted}"; sid:{100000 + emitted}; '
                f"rev:{rng.randrange(1, 5)};)"
            )
        if args.error_rate > 0 and rng.random() < args.error_rate:
            fields = corrupt(fields, rng)
            malformed += 1
        print(" ".join(fields))

    print(f"generated {emitted} rules ({malformed} malformed)", file=sys.stderr)


if __name__ == "__main__":
    main()
