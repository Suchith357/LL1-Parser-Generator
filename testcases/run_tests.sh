#!/bin/sh
#
# run_tests.sh - integration test suite for the LL(1) Parser Generator
# (Team 4 shared responsibility; see PROJECT_SPECIFICATION.txt section R)
#
# Usage:  make test          (from the project root, builds if needed)
#     or  sh testcases/run_tests.sh
#
# Each scenario runs the binary on a grammar file, optionally feeds
# input strings on stdin, and greps the output for expected markers.
# Prints PASS/FAIL per test and a summary; exits non-zero on failure.

set -u

BIN=${BIN:-./ll1}
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

pass=0
fail=0
cur_ok=1
cur_tid=""
cur_desc=""

# run_case <test-id> <description> <grammar-file> <stdin-input-with-\n>
run_case() {
    cur_tid=$1
    cur_desc=$2
    if [ ! -x "$BIN" ]; then
        echo "Building binary first..."
        gcc -std=c99 -Wall -Wextra -pedantic -O2 -Iinclude src/*.c -o ll1 \
            || { echo "FAIL [$cur_tid] build failed"; fail=$((fail+1)); return; }
    fi
    printf '%b' "$4" | "$BIN" "$3" > "$TMP" 2>&1
    cur_ok=1
}

# has <literal text that must appear in the output>
has() {
    if ! grep -qF -- "$1" "$TMP"; then
        cur_ok=0
        echo "FAIL [$cur_tid] $cur_desc"
        echo "  missing expected output: $1"
    fi
}

# finish - record the result of the current case
finish() {
    if [ "$cur_ok" = 1 ]; then
        echo "PASS [$cur_tid] $cur_desc"
        pass=$((pass+1))
    else
        fail=$((fail+1))
    fi
}

# expect_reject <test-id> <description> <grammar-file> <patterns...>
# The grammar must be rejected: non-zero exit + diagnostics.
expect_reject() {
    tid=$1; desc=$2; grammar=$3; shift 3
    "$BIN" "$grammar" < /dev/null > "$TMP" 2>&1
    rc=$?
    ok=1
    [ $rc -ne 0 ] || { ok=0; echo "FAIL [$tid] $desc: expected non-zero exit"; }
    for pattern in "$@"; do
        grep -qF -- "$pattern" "$TMP" || {
            ok=0
            echo "FAIL [$tid] $desc: missing diagnostic: $pattern"
        }
    done
    if [ "$ok" = 1 ]; then
        echo "PASS [$tid] $desc"
        pass=$((pass+1))
    else
        fail=$((fail+1))
    fi
}

echo "=== LL(1) Parser Generator test suite ==="

G1=testcases/valid/expression_valid.txt

# TEST 1 - standard LL(1) grammar: FIRST, FOLLOW, table, verdict
run_case T01 "standard expression grammar" $G1 ""
has "FIRST(E) = { (, id }"
has "FIRST(Ep) = { +, # }"
has "FIRST(T) = { (, id }"
has "FIRST(Tp) = { *, # }"
has "FIRST(F) = { (, id }"
has "FOLLOW(E) = { ), $ }"
has "FOLLOW(Ep) = { ), $ }"
has "FOLLOW(T) = { +, ), $ }"
has "FOLLOW(Tp) = { +, ), $ }"
has "FOLLOW(F) = { +, *, ), $ }"
has "Grammar is LL(1)"
finish

# TEST 2 - epsilon propagation through several symbols
run_case T02 "epsilon propagation" testcases/valid/epsilon_propagation.txt ""
has "FIRST(A) = { a, # }"
has "FIRST(B) = { b }"
has "FIRST(S) = { a, b }"
has "Grammar is LL(1)"
finish

# TEST 3 - pure epsilon grammar + empty input acceptance
run_case T03 "pure epsilon grammar" testcases/edge/pure_epsilon.txt ""
has "FIRST(S) = { # }"
has "FOLLOW(S) = { $ }"
has "Grammar is LL(1)"
finish
run_case T03b "pure epsilon: empty input accepted" \
    testcases/edge/pure_epsilon.txt "\n"
has "INPUT ACCEPTED"
finish

# TEST 4 - non-LL(1) grammar: conflict reported, never overwritten
run_case T04 "LL(1) conflict detection" testcases/valid/non_ll1_conflict.txt ""
has "Conflict detected at M[S, a]"
has "Production 1: S -> a A"
has "Production 2: S -> a B"
has "Grammar is NOT LL(1)"
finish

# TEST 5 - simplest grammar + valid/invalid parsing
run_case T05 "simple grammar S -> a" testcases/valid/simple.txt ""
has "FIRST(S) = { a }"
has "FOLLOW(S) = { $ }"
has "Grammar is LL(1)"
finish
run_case T05b "simple grammar: parse 'a'" testcases/valid/simple.txt "a\n"
has "INPUT ACCEPTED"
finish
run_case T05c "simple grammar: reject 'b'" testcases/valid/simple.txt "b\n"
has "SYNTAX ERROR"
has "not a terminal of the grammar"
finish

# TEST 6 - invalid grammars must be rejected with clear diagnostics
expect_reject T06a "undefined non-terminal" testcases/invalid/undefined_nt.txt \
    "never defined"
expect_reject T06b "missing arrow" testcases/invalid/missing_arrow.txt \
    "missing '->'"
expect_reject T06c "double arrow" testcases/invalid/double_arrow.txt \
    "more than one '->'"
expect_reject T06d "invalid character" testcases/invalid/bad_symbol.txt \
    "invalid symbol"
expect_reject T06e "empty alternative" testcases/invalid/empty_alt.txt \
    "empty alternative"
expect_reject T06f "reserved end marker" testcases/invalid/end_marker.txt \
    "invalid symbol"

# TEST 7 - indirect recursion: fixed-point FIRST/FOLLOW convergence.
# NOTE: this grammar is genuinely NOT LL(1) (A -> S b and A -> # both
# claim table cell [A, c]), which the conflict report must expose.
run_case T07 "indirect recursion fixed point" \
    testcases/valid/indirect_recursion.txt ""
has "FIRST(S) = { c }"
has "FIRST(A) = { #, c }"
has "Grammar is NOT LL(1)"
finish

# TEST 8 - valid inputs on the standard grammar
run_case T08 "parse id + id * id" $G1 "id + id * id\n"
has "INPUT ACCEPTED"
has "E -> T Ep"
has "match id"
finish
run_case T08b "parse id + id" $G1 "id + id\n"
has "INPUT ACCEPTED"
finish
run_case T08c "parse ( id + id ) * id" $G1 "( id + id ) * id\n"
has "INPUT ACCEPTED"
finish

# TEST 9 - invalid inputs give positioned diagnostics
run_case T09 "reject id + * id" $G1 "id + * id\n"
has "SYNTAX ERROR"
has "No parsing-table entry for [T, *]"
finish
run_case T09b "reject id +" $G1 "id +\n"
has "SYNTAX ERROR"
finish

# TEST 10 - syntax-directed translation (letter terminals; G10 uses
# F -> ( E ) | a | b | c so the classic textbook input parses directly)
G10=testcases/valid/expression_letters.txt
run_case T10 "SDT of a + b * c" $G10 "a + b * c\n"
has "a b c * +"
has "t1 = b * c"
has "t2 = a + t1"
finish
run_case T10b "SDT of id * id + id" $G1 "id * id + id\n"
has "id id * id +"
has "t1 = id * id"
has "t2 = t1 + id"
finish

# TEST 11/12 - edge grammars
run_case T11 "compact arrow syntax" testcases/edge/no_space_arrow.txt ""
has "Grammar is LL(1)"
finish
run_case T12 "nullable non-terminal in RHS" testcases/edge/optional_symbol.txt ""
has "Grammar is LL(1)"
finish
run_case T12b "optional part: parse with and without" \
    testcases/edge/optional_symbol.txt "a b\nb\n"
has "INPUT ACCEPTED"
finish

# TEST 13 - left recursion is a valid CFG but not LL(1)
run_case T13 "left recursion reported as NOT LL(1)" \
    testcases/valid/left_recursive.txt ""
has "Grammar is NOT LL(1)"
finish

echo
echo "Tests passed: $pass   failed: $fail"
[ "$fail" = 0 ] || exit 1
exit 0
