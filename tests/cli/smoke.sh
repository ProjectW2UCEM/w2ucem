#!/usr/bin/env bash
# CLI 黑盒冒烟测试：sync(验签) → search/info/list → fetch → archive-check + 负例。
# 用法: tests/cli/smoke.sh <w2ucem-cli 可执行文件> [fixtures_dir]
set -euo pipefail

CLI="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
FIX="${2:-$ROOT_DIR/tests/fixtures}"

if [ ! -d "$FIX/repo" ]; then
  "$ROOT_DIR/tests/fixtures/gen_fixture_repo.sh" "$FIX"
fi

# 每个测试用独立 root（隔离）
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail() {
  echo "FAIL: $1" >&2
  exit 1
}
step() {
  echo "== $1"
}

# ---------- 0. 版本 ----------
step "version"
"$CLI" version | grep -q "w2ucem 0.1.0" || fail "version 输出不含 0.1.0"
"$CLI" version --json | grep -q '"evaluator"' || fail "version --json 缺字段"

# ---------- 1. doctor ----------
step "doctor"
"$CLI" --root "$WORK" doctor | grep -q "容器引擎" || fail "doctor 无容器引擎检查"

# ---------- 2. sync（验签） ----------
step "sync (ed25519 验签)"
mkdir -p "$WORK/trust"
cp "$FIX/keys/pubkey.pem" "$WORK/trust/pubkey.pem"
"$CLI" --root "$WORK" sync --repo "file://$FIX/repo" | grep -q "2 个模板" || fail "sync 失败"

# 坏签名负例：篡改 repo.db 后签名失效
step "sync 负例：篡改索引被签名校验拦截"
TAMPER="$(mktemp -d)"
cp -r "$FIX/repo" "$TAMPER/repo"
printf 'x' >> "$TAMPER/repo/repo.db"
"$CLI" --root "$WORK" sync --repo "file://$TAMPER/repo" 2>&1 | grep -q "签名校验失败" \
  || fail "篡改索引未被拦截"
rm -rf "$TAMPER"

# ---------- 3. search / list / info ----------
step "search / list"
"$CLI" --root "$WORK" search mock | grep -q "mock-proton@1.0" || fail "search 未命中"
"$CLI" --root "$WORK" list | grep -q "bad-hash@1.0" && fail "实验性模板默认应被隐藏"
"$CLI" --root "$WORK" list --include-experimental | grep -q "bad-hash@1.0" || fail "实验性模板未列出"

step "info（含求值预览）"
"$CLI" --root "$WORK" info mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "libx11-dev" || fail "info 求值预览缺 build_depends"

# ---------- 4. fetch ----------
step "fetch（下载 + sha256 校验）"
"$CLI" --root "$WORK" fetch mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "已校验" || fail "fetch 失败"

# ---------- 5. archive-check（强制时光机回退） ----------
step "archive-check（时光机回退）"
"$CLI" --root "$WORK" archive-check mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "时光机回退验证通过" || fail "archive-check 失败"

# ---------- 6. 负例：坏哈希源 ----------
step "负例：坏哈希源被拦截"
"$CLI" --root "$WORK" fetch bad-hash --target semi-rolling:generic:deb:stable 2>&1 \
  | grep -q "hash-mismatch" || fail "坏哈希源未被拦截"

echo "smoke 全部通过"
