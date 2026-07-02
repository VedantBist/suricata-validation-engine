#!/usr/bin/env bash
# Golden-file test runner (see docs/ARCHITECTURE.md §8).
#
# Every case executes the binary and diffs stdout+stderr verbatim against a
# golden file. Expected exit code is 0 unless a sidecar <case>.exit says
# otherwise.
#
# Suites:
#   lexer/    — --dump-tokens
#   parser/   — --syntax-only (recovery + syntax diagnostics, isolated from
#               semantic churn)
#   semantic/ — --validate (full pipeline)
#   cli/      — output/behavior flags (--json, --summary, --quiet,
#               --max-errors) over a fixed mixed input
#   stress    — STRESS=1: ~10k-rule tiers; count invariants, JSON
#               integration, corruption-heavy stability, timing smoke,
#               heap check via `leaks` on macOS (ASan is unusable on some
#               macOS toolchains — see Makefile)

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-$ROOT/build/bin/suricata-validate}"
pass=0
fail=0

fail_case() {
    echo "FAIL  $1 — $2"
    fail=$((fail + 1))
}

# run_case <label> <golden> <input> [flags...]
run_case() {
    local label=$1 expected=$2 input=$3
    shift 3

    if [ ! -f "$expected" ]; then
        fail_case "$label" "missing golden file $expected"
        return
    fi

    local actual rc expected_rc
    actual="$("$BIN" "$@" "$input" 2>&1)"
    rc=$?
    expected_rc=0
    if [ -f "${expected%.txt}.exit" ]; then
        expected_rc="$(cat "${expected%.txt}.exit")"
    fi

    if [ "$rc" -ne "$expected_rc" ]; then
        fail_case "$label" "exit code $rc, expected $expected_rc"
        return
    fi
    if ! diff -u "$expected" <(printf '%s\n' "$actual"); then
        fail_case "$label" "output differs from golden (diff above)"
        return
    fi
    echo "ok    $label"
    pass=$((pass + 1))
}

echo "== lexer suite (token dump) =="
for input in "$ROOT"/tests/data/lexer/*.rules; do
    name="$(basename "${input%.rules}")"
    run_case "$name" "$ROOT/tests/expected/lexer/$name.txt" "$input" --dump-tokens
done

echo
echo "== parser suite (syntax-only) =="
for input in "$ROOT"/tests/data/parser/*.rules; do
    name="$(basename "${input%.rules}")"
    run_case "$name" "$ROOT/tests/expected/parser/$name.txt" "$input" --syntax-only
done

echo
echo "== semantic suite (full validation) =="
for input in "$ROOT"/tests/data/semantic/*.rules; do
    name="$(basename "${input%.rules}")"
    run_case "$name" "$ROOT/tests/expected/semantic/$name.txt" "$input" --validate
done

echo
echo "== cli suite (output modes and policies) =="
CLI_INPUT="$ROOT/tests/data/cli/mixed.rules"
run_case "json"             "$ROOT/tests/expected/cli/json.txt"             "$CLI_INPUT" --json
run_case "json-syntax-only" "$ROOT/tests/expected/cli/json_syntax_only.txt" "$CLI_INPUT" --json --syntax-only
run_case "summary"          "$ROOT/tests/expected/cli/summary.txt"          "$CLI_INPUT" --summary
run_case "quiet"            "$ROOT/tests/expected/cli/quiet.txt"            "$CLI_INPUT" --quiet
run_case "max-errors"       "$ROOT/tests/expected/cli/maxerrors.txt"        "$CLI_INPUT" --quiet --max-errors 1

# CLI robustness (no goldens — exit codes and stderr behavior)
if "$BIN" --help | grep -q "usage: suricata-validate"; then
    echo "ok    help"
    pass=$((pass + 1))
else
    fail_case "help" "--help did not print usage / exit 0"
fi
if "$BIN" --bogus-flag x 2>/dev/null; [ $? -eq 2 ] \
    && "$BIN" --max-errors 0 x 2>/dev/null; [ $? -eq 2 ] \
    && "$BIN" --json --summary x 2>/dev/null; [ $? -eq 2 ]; then
    echo "ok    argument-validation"
    pass=$((pass + 1))
else
    fail_case "argument-validation" "bad arguments did not exit 2"
fi

# Heap check helper: leaks(1) replaces the target's exit code with its own,
# so it runs as a separate pass from the functional assertions.
leaks_check() {
    local label=$1; shift
    if ! command -v leaks >/dev/null 2>&1; then
        return 0
    fi
    if leaks --atExit --quiet -- "$@" > /dev/null 2>&1; then
        echo "      $label: leaks clean"
    else
        fail_case "$label" "leaks reported heap leaks"
        return 1
    fi
}

# parse_summary <summary-line> -> total valid syninv seminv diags
parse_summary() {
    echo "$1" | awk -F'[:|=(]+' '{gsub(/[^0-9 ]/,""); print $0}' \
              | awk '{print $1, $2, $3, $4, $5}'
}

if [ "${STRESS:-0}" = "1" ]; then
    echo
    echo "== stress: lexer (10k rules, with options) =="
    stress_lexer="$ROOT/build/stress_lexer.rules"
    python3 "$ROOT/tests/data/stress/generate_stress.py" 10000 \
        > "$stress_lexer" 2>/dev/null
    out="$("$BIN" --dump-tokens "$stress_lexer")"
    rc=$?
    summary="$(echo "$out" | tail -1)"
    echo "$summary"
    if [ $rc -ne 0 ] || ! echo "$summary" | grep -q "rules: 10000 .* lexical errors: 0"; then
        fail_case "stress-lexer" "exit $rc or summary mismatch"
    elif leaks_check "stress-lexer" "$BIN" --dump-tokens "$stress_lexer"; then
        echo "ok    stress-lexer"
        pass=$((pass + 1))
    fi

    echo
    echo "== stress: full pipeline (10k rules, 2% syntax + 2% semantic corruption) =="
    stress_full="$ROOT/build/stress_full.rules"
    python3 "$ROOT/tests/data/stress/generate_stress.py" 10000 \
        --error-rate 0.02 --semantic-error-rate 0.02 > "$stress_full" 2>/dev/null
    out="$("$BIN" --quiet --timing "$stress_full" 2>"$ROOT/build/stress_timing.txt")"
    rc=$?
    summary="$(echo "$out" | tail -1)"
    echo "$summary"
    sed -n 's/^/      /p' "$ROOT/build/stress_timing.txt"
    read -r total valid syninv seminv diags <<< "$(parse_summary "$summary")"
    if [ "$rc" -ne 1 ]; then
        fail_case "stress-full" "exit $rc, expected 1"
    elif [ "$total" != "10000" ] || [ $((valid + syninv + seminv)) -ne "$total" ] \
        || [ "$diags" -ne $((syninv + seminv)) ] \
        || [ "$syninv" -eq 0 ] || [ "$seminv" -eq 0 ]; then
        fail_case "stress-full" "count invariants violated (total=$total valid=$valid syntax=$syninv semantic=$seminv diags=$diags)"
    elif ! grep -q "^-- timing: parse+validate .* rules/s --$" "$ROOT/build/stress_timing.txt"; then
        fail_case "stress-full" "--timing produced no timing block on stderr"
    elif leaks_check "stress-full" "$BIN" --quiet "$stress_full"; then
        echo "ok    stress-full (valid=$valid syntax-invalid=$syninv semantic-invalid=$seminv)"
        pass=$((pass + 1))
    fi

    echo
    echo "== stress: JSON integration (parse + cross-check against text summary) =="
    json_ok="$("$BIN" --json "$stress_full" | python3 -c '
import json, sys
r = json.load(sys.stdin)
s = r["summary"]
assert r["schema_version"] == 1
assert s["total_rules"] == s["valid_rules"] + s["syntax_invalid"] + s["semantic_invalid"]
assert len(r["diagnostics"]) == s["errors"] + s["warnings"]
print("json-valid total=%d diags=%d" % (s["total_rules"], len(r["diagnostics"])))
' 2>&1)"
    if [ $? -eq 0 ]; then
        echo "      $json_ok"
        echo "ok    stress-json"
        pass=$((pass + 1))
    else
        fail_case "stress-json" "$json_ok"
    fi

    echo
    echo "== stress: corruption-heavy (10k rules, 20% syntax + 10% semantic) =="
    stress_heavy="$ROOT/build/stress_heavy.rules"
    python3 "$ROOT/tests/data/stress/generate_stress.py" 10000 \
        --error-rate 0.20 --semantic-error-rate 0.10 > "$stress_heavy" 2>/dev/null
    out="$("$BIN" --summary "$stress_heavy")"
    rc=$?
    summary="$(echo "$out" | tail -1)"
    echo "$summary"
    read -r total valid syninv seminv diags <<< "$(parse_summary "$summary")"
    if [ "$rc" -ne 1 ] || [ "$total" != "10000" ] \
        || [ $((valid + syninv + seminv)) -ne "$total" ] \
        || [ "$diags" -ne $((syninv + seminv)) ]; then
        fail_case "stress-heavy" "invariants violated under heavy corruption (total=$total valid=$valid syntax=$syninv semantic=$seminv diags=$diags)"
    elif leaks_check "stress-heavy" "$BIN" --summary "$stress_heavy"; then
        echo "ok    stress-heavy (recovered ${syninv}x, semantic ${seminv}x)"
        pass=$((pass + 1))
    fi

    echo
    echo "== stress: determinism (two runs, byte-identical output) =="
    a="$("$BIN" --json "$stress_full")"
    b="$("$BIN" --json "$stress_full")"
    if [ "$a" = "$b" ]; then
        echo "ok    stress-determinism"
        pass=$((pass + 1))
    else
        fail_case "stress-determinism" "two identical runs produced different output"
    fi
fi

echo
echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
