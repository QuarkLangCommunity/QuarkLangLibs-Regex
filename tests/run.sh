#!/bin/sh
# QuarkLang regex 库测试
# 用法：QUARK=/path/to/quark ./run.sh     （默认找 PATH 中的 quark）
set -e
QUARK_BIN="${QUARK:-quark}"
DIR="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cp "$DIR/../regex.qk" "$TMP/regex.qk"
cp "$DIR/test_regex.qk" "$TMP/main.qk"
cd "$TMP"
out="$("$QUARK_BIN" main.qk 2>&1)" || { printf '%s\n' "$out"; exit 1; }
printf '%s\n' "$out"

# test_regex.qk counts its own failures and prints `failures=N`, but the interpreter exits 0
# either way, so `set -e` on its own left this gate blind: every assertion could fail and CI
# would still be green. The count has to be read back explicitly.
if ! printf '%s\n' "$out" | grep -qx 'failures=0'; then
	echo "regex tests reported failures (expected a final line 'failures=0')" >&2
	exit 1
fi
if printf '%s\n' "$out" | grep -q '^FAIL'; then
	echo "regex tests printed FAIL lines" >&2
	exit 1
fi
