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
"$QUARK_BIN" main.qk
