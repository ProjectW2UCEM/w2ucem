#!/usr/bin/env bash
# 在 Linux 上重新编译 mockelf（e2e 用）。
# 注意：tests/fixtures/elf/mockelf.readelf-Ws.txt 与 mockelf.canonical.txt 是
# 规范化器单测的 golden 文件（语义样本，不随 gcc 版本漂移），本脚本只负责
# 产出真实 ELF 二进制供 e2e 装配/扫描/校验闭环使用，不覆盖 golden。
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
gcc -Os -o mockelf mockelf.c
echo "mockelf 已重新生成（$(gcc --version | head -1)）"
