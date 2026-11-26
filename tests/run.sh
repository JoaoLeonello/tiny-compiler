#!/bin/sh
set -u
cd "$(dirname "$0")/.."

pass=0
fail=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

for src in tests/ok/*.mc; do
    name=${src%.mc}
    input=/dev/null
    [ -f "$name.in" ] && input="$name.in"
    for flag in "" "-O"; do
        if ./minicc $flag -o "$tmp/prog" "$src" &&
            "$tmp/prog" < "$input" > "$tmp/out" 2>&1 &&
            diff -u "$name.out" "$tmp/out"; then
            pass=$((pass + 1))
        else
            echo "FAIL: $src $flag"
            fail=$((fail + 1))
        fi
    done
done

for src in tests/err/*.mc; do
    expected=$(sed -n 's|^// expect: ||p' "$src")
    if ./minicc -o "$tmp/prog" "$src" 2> "$tmp/err"; then
        echo "FAIL: $src compiled but should have failed"
        fail=$((fail + 1))
    elif grep -qF "$expected" "$tmp/err"; then
        pass=$((pass + 1))
    else
        echo "FAIL: $src expected '$expected', got: $(cat "$tmp/err")"
        fail=$((fail + 1))
    fi
done

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
