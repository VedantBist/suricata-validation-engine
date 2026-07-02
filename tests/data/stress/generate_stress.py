#!/usr/bin/env python3
"""Deterministic stress-file generator (seeded PRNG — same output every run,
so assertions on counts are stable). Emits lexically valid rules covering the
Phase 2 token inventory; comment and blank lines are mixed in to exercise
rule-number accounting at scale.

Usage: generate_stress.py [count] [--header-only] [--error-rate F]
                          [--semantic-error-rate F] > out
  --header-only            emit rules without the options block
  --error-rate F           fraction of rules to structurally corrupt —
                           always lexically valid, 1 syntax diagnostic each
  --semantic-error-rate F  fraction of rules to semantically corrupt —
                           syntactically valid, exactly 1 ERROR diagnostic
                           each (so diagnostics == syntax + semantic
                           invalid counts stays assertable)

A rule receives at most one corruption (syntactic wins the coin toss), so
the two counts never overlap.

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


def corrupt(fields, rng, has_options):
    """Structural corruption only — never introduces unlexable characters,
    and every corruption is guaranteed syntactically INVALID (so the runner
    can assert diagnostics == invalid rules)."""
    kind = rng.randrange(6 if has_options else 4)
    if kind == 0:
        # drop a header field — never the options block, which would leave
        # a perfectly valid header-only rule
        last = len(fields) - (1 if has_options else 0)
        del fields[rng.randrange(1, last)]
    elif kind == 1:
        del fields[0]                               # drop the action
    elif kind == 2:
        fields.insert(rng.randrange(len(fields)), "->")  # stray direction
    elif kind == 3:
        fields.append(fields[-1])                   # duplicated trailing field
    elif kind == 4:
        fields[-1] = fields[-1].replace(";", "", 1)  # first semicolon gone
    else:
        fields[-1] = fields[-1][:-1]                 # closing ')' gone
    return fields


def sem_corrupt(rng, header, options, last_clean_sid):
    """Semantic corruption: parses cleanly, fails validation with exactly
    one ERROR. header is the 7-element field list; options the option text."""
    kind = rng.randrange(6)
    if kind == 4 and last_clean_sid is None:
        kind = 5
    if kind == 0:
        header[6] = str(rng.randrange(65536, 99999))          # dst port range
    elif kind == 1:
        header[2] = f"999.{rng.randrange(1, 254)}.1.{rng.randrange(1, 254)}"
    elif kind == 2:
        header[5] = f"10.{rng.randrange(1, 254)}.0.0/{rng.randrange(33, 99)}"
    elif kind == 3:
        options = options.replace(f" sid:", " rev:", 1)       # sid missing
    elif kind == 4:
        import re
        options = re.sub(r"sid:\d+", f"sid:{last_clean_sid}", options)
    else:
        import re
        options = re.sub(r"sid:\d+", "sid:0", options)        # sid not positive
    return header, options


def main():
    p = argparse.ArgumentParser()
    p.add_argument("count", nargs="?", type=int, default=10000)
    p.add_argument("--header-only", action="store_true")
    p.add_argument("--error-rate", type=float, default=0.0)
    p.add_argument("--semantic-error-rate", type=float, default=0.0)
    args = p.parse_args()

    rng = random.Random(1337)
    emitted = 0
    malformed = 0
    sem_malformed = 0
    last_clean_sid = None
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
        # ICMP rules always use 'any' ports: numeric ports on icmp emit
        # coherence warnings, which would break the runner's strict
        # diagnostics == syntax + semantic invariant on the clean baseline.
        sport = "any" if proto == "icmp" else port(rng)
        dport = "any" if proto == "icmp" else port(rng)
        header = [action, proto, endpoint(rng), sport, direction,
                  endpoint(rng), dport]
        options = None
        if not args.header_only:
            # 0..5 extra content options exercise the recursive list at
            # varying depths (up to ~8 reductions per rule)
            extras = "".join(
                f' content:"blob-{i}";' for i in range(rng.randrange(6))
            )
            options = (
                f'(msg:"stress rule {emitted}"; sid:{100000 + emitted};'
                f"{extras} rev:{rng.randrange(1, 5)};)"
            )

        roll = rng.random()
        if args.error_rate > 0 and roll < args.error_rate:
            fields = header + ([options] if options else [])
            fields = corrupt(fields, rng, options is not None)
            malformed += 1
            print(" ".join(fields))
            continue
        if (args.semantic_error_rate > 0 and options is not None
                and roll < args.error_rate + args.semantic_error_rate):
            if proto == "icmp":
                header[0], header[1] = header[0], "tcp"   # avoid icmp warnings
            header, options = sem_corrupt(rng, header, options, last_clean_sid)
            sem_malformed += 1
            print(" ".join(header + [options]))
            continue

        last_clean_sid = 100000 + emitted
        print(" ".join(header + ([options] if options else [])))

    print(f"generated {emitted} rules ({malformed} syntax-corrupt, "
          f"{sem_malformed} semantic-corrupt)", file=sys.stderr)


if __name__ == "__main__":
    main()
