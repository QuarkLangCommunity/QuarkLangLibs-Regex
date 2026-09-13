#!/bin/sh
# 运行示例（自动把库拷到同目录）
# 用法：QUARK=/path/to/quark ./run.sh
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
cp "$DIR/../regex.qk" "$DIR/regex.qk"
cd "$DIR"
"${QUARK:-quark}" demo.qk
