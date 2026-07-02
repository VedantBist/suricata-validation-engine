#!/usr/bin/env bash
# Golden-file test runner (see docs/ARCHITECTURE.md §8).
#
# Every case is an input file in tests/data/<suite>/ paired with a golden
# output in tests/expected/<suite>/<case>.txt; the binary's stdout+stderr is
# diffed verbatim. Expected exit code is 0 unless a sidecar
# tests/expected/<suite>/<case>.exit file says otherwise.
#
# Suites:
#   lexer/   — run with --dump-tokens
#   stress   — STRESS=1: generate ~10k rules, assert completion + counts
#              (+ heap check via `leaks` on macOS, since ASan is unusable
#              on some macOS toolchains — see Makefile)

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

if [ "${STRESS:-0}" = "1" ]; then
    echo
    echo "== stress (10k rules) =="
    stress_file="$ROOT/build/stress.rules"
    python3 "$ROOT/tests/data/stress/generate_stress.py" 10000 > "$stress_file"

    runner=("$BIN")
    if command -v leaks >/dev/null 2>&1; then
        runner=(leaks --atExit --quiet -- "$BIN")
    fi
    start=$(date +%s)
    summary="$("${runner[@]}" --dump-tokens "$stress_file" 2>&1 | grep -E '^(==|.*leaks for)')"
    rc=$?
    elapsed=$(( $(date +%s) - start ))

    echo "$summary"
    if [ $rc -ne 0 ]; then
        fail_case "stress" "exit code $rc"
    elif ! echo "$summary" | grep -q "rules: 10000 .* lexical errors: 0"; then
        fail_case "stress" "summary does not show 10000 clean rules"
    else
        echo "ok    stress (${elapsed}s)"
        pass=$((pass + 1))
    fi
fi

echo
echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
