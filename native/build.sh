#!/bin/sh
# 构建 regex 库的 PCRE2 后端 → bin/libqkregex.so
#
# 依赖：系统 PCRE2（libpcre2-8）与 C 编译器。
#   Debian/Ubuntu: apt install libpcre2-dev
#   Fedora/RHEL:   dnf install pcre2-devel
#   macOS:         brew install pcre2（头文件/库在 SDK 与 brew 前缀下，pkg-config 可查）
#   Windows(MSYS2/MinGW): pacman -S mingw-w64-x86_64-pcre2 → 产出 libqkregex.dll
# 产物：bin/libqkregex.so（Linux）/ .dylib（macOS）/ .dll（Windows）
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
out="$(CDPATH= cd -- "$here/.." && pwd)/bin"
mkdir -p "$out"

CFLAGS="$(pkg-config --cflags libpcre2-8 2>/dev/null || true)"
LIBS="$(pkg-config --libs libpcre2-8 2>/dev/null || echo -lpcre2-8)"
CC="${CC:-cc}"

case "$(uname -s)" in
  Darwin)                  ext="dylib"; flags="-dynamiclib" ;;
  MINGW*|MSYS*|CYGWIN*)    ext="dll";   flags="-shared" ;;
  *)                       ext="so";    flags="-shared" ;;
esac

# shellcheck disable=SC2086
"$CC" -O2 -fPIC $flags -o "$out/libqkregex.$ext" "$here/qkregex.c" $CFLAGS $LIBS
echo "✓ 构建: $out/libqkregex.$ext（PCRE2 后端）"
