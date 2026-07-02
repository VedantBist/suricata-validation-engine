#!/usr/bin/env bash
# Golden-file test runner (see docs/ARCHITECTURE.md §8).
#
# Every case is an input file in tests/data/<suite>/ paired with a golden
# output in tests/expected/<suite>/<case>.txt; the binary's stdout+stderr is
# diffed verbatim. Expected exit code is 0 unless a sidecar
# tests/expected/<suite>/<case>.exit file says otherwise.
#
# Suites:
#   lexer/    — run with --dump-tokens
#   parser/   — run with --syntax-only (recovery + syntax diagnostics,
#               isolated from semantic churn)
#   semantic/ — run with --validate (full pipeline: syntax + semantics)
#   stress    — STRESS=1: ~10k-rule tiers; asserts completion + count
#               invariants (+ heap check via `leaks` on macOS, since ASan
#               is unusable on some macOS toolchains — see Makefile)

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-$ROOT/build/bin/suricata-validate}"
pass=0
fail=0

fail_case() {
    echo "FAIL  $1 — $2"
    fail=$((fail + 1))
}

run_case() {
    local mode_flag=$1 input=$2 expected=$3
    local name actual rc expected_rc
    name="$(basename "$input")"

    if [ ! -f "$expected" ]; then
        fail_case "$name" "missing golden file $expected"
        return
    fi

    actual="$("$BIN" "$mode_flag" "$input" 2>&1)"
    rc=$?
    expected_rc=0
    if [ -f "${expected%.txt}.exit" ]; then
        expected_rc="$(cat "${expected%.txt}.exit")"
    fi

    if [ "$rc" -ne "$expected_rc" ]; then
        fail_case "$name" "exit code $rc, expected $expected_rc"
        return
    fi
    if ! diff -u "$expected" <(printf '%s\n' "$actual"); then
        fail_case "$name" "output differs from golden (diff above)"
        return
    fi
    echo "ok    $name"
    pass=$((pass + 1))
}

echo "== lexer suite (token dump) =="
for input in "$ROOT"/tests/data/lexer/*.rules; do
    run_case --dump-tokens "$input" \
        "$ROOT/tests/expected/lexer/$(basename "${input%.rules}").txt"
done

echo
echo "== parser suite (syntax-only) =="
for input in "$ROOT"/tests/data/parser/*.rules; do
    run_case --syntax-only "$input" \
        "$ROOT/tests/expected/parser/$(basename "${input%.rules}").txt"
done

echo
echo "== semantic suite (full validation) =="
for input in "$ROOT"/tests/data/semantic/*.rules; do
    run_case --validate "$input" \
        "$ROOT/tests/expected/semantic/$(basename "${input%.rules}").txt"
done

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
    stress_parser="$ROOT/build/stress_parser.rules"
    python3 "$ROOT/tests/data/stress/generate_stress.py" 10000 \
        --error-rate 0.02 --semantic-error-rate 0.02 > "$stress_parser" 2>/dev/null
    start=$(date +%s)
    out="$("$BIN" --validate "$stress_parser")"
    rc=$?
    elapsed=$(( $(date +%s) - start ))
    summary="$(echo "$out" | tail -1)"
    echo "$summary   (${elapsed}s)"
    # Invariants: every rule accounted for exactly once
    # (valid + syntax-invalid + semantic-invalid == total); corruption is
    # lexically clean and every injected fault yields exactly one ERROR
    # diagnostic, so diagnostics == syntax-invalid + semantic-invalid.
    read -r total valid syninv seminv diags <<< "$(echo "$summary" \
        | awk -F'[:|=]+' '{gsub(/[^0-9 ]/,""); print $0}' \
        | awk '{print $1, $2, $3, $4, $5}')"
    if [ "$rc" -ne 1 ]; then
        fail_case "stress-full" "exit $rc, expected 1"
    elif [ "$total" != "10000" ] || [ $((valid + syninv + seminv)) -ne "$total" ] \
        || [ "$diags" -ne $((syninv + seminv)) ] \
        || [ "$syninv" -eq 0 ] || [ "$seminv" -eq 0 ]; then
        fail_case "stress-full" "count invariants violated (total=$total valid=$valid syntax=$syninv semantic=$seminv diags=$diags)"
    elif leaks_check "stress-full" "$BIN" --validate "$stress_parser"; then
        echo "ok    stress-full (valid=$valid syntax-invalid=$syninv semantic-invalid=$seminv)"
        pass=$((pass + 1))
    fi
fi

echo
echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
