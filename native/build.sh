#!/bin/sh
# 构建 regex 库的 PCRE2 后端 → bin/libqkregex.so
#
# 依赖：系统 PCRE2（libpcre2-8）与 C 编译器。
#   Debian/Ubuntu: apt install libpcre2-dev
#   Fedora/RHEL:   dnf install pcre2-devel
#   macOS:         brew install pcre2
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
out="$(CDPATH= cd -- "$here/.." && pwd)/bin"
mkdir -p "$out"

CFLAGS="$(pkg-config --cflags libpcre2-8 2>/dev/null || true)"
LIBS="$(pkg-config --libs libpcre2-8 2>/dev/null || echo -lpcre2-8)"
CC="${CC:-cc}"

case "$(uname -s)" in
  Darwin) ext="dylib"; flags="-dynamiclib" ;;
  *)      ext="so";    flags="-shared" ;;
esac

# shellcheck disable=SC2086
"$CC" -O2 -fPIC $flags -o "$out/libqkregex.$ext" "$here/qkregex.c" $CFLAGS $LIBS
echo "✓ 构建: $out/libqkregex.$ext（PCRE2 后端）"
