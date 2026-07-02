#!/usr/bin/env python3
"""Deterministic stress-file generator (seeded PRNG — same output every run,
so assertions on counts are stable). Emits lexically valid rules covering the
Phase 2 token inventory; comment and blank lines are mixed in to exercise
rule-number accounting at scale.

Usage: generate_stress.py [rule_count] > stress.rules
Output goes to stdout; generated files are gitignored — only this script is
committed.
"""

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


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 10000
    rng = random.Random(1337)
    emitted = 0
    while emitted < count:
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
        print(
            f"{action} {proto} {endpoint(rng)} {port(rng)} {direction} "
            f'{endpoint(rng)} {port(rng)} '
            f'(msg:"stress rule {emitted}"; sid:{100000 + emitted}; rev:{rng.randrange(1, 5)};)'
        )


if __name__ == "__main__":
    main()
