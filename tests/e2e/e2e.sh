#!/usr/bin/env bash
# 容器端到端测试（Linux + docker，CI 用）：
#   1. 本地构建 buildenv 与 matrix 镜像，记录 digest（SDK 层锁定的真实来源）
#   2. 用真实 digest 生成 fixtures（digest 锁校验生效）
#   3. 完整链路：sync（验签）→ build（digest 锁 + 容器装配 + dep-scan）→ verify
#   4. 负例：篡改 tarball → HashMismatch
#   5. 回退例：上游 URL 全 404 → archive-check 走时光机命中
# 用法: tests/e2e/e2e.sh <w2ucem-cli 可执行文件> [fixtures_dir]
set -euo pipefail

# CI 诊断：完整输出 tee 进 step summary（Actions 页面无需展开日志即可见）
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  exec > >(tee -a "$GITHUB_STEP_SUMMARY") 2>&1
fi

CLI="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
FIX="${2:-$ROOT_DIR/tests/fixtures}"

fail() {
  echo "FAIL: $1" >&2
  # CI 诊断：失败断言写进 step summary（Actions 页面直接可见）
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    echo "### e2e FAIL: $1" >> "$GITHUB_STEP_SUMMARY"
  fi
  exit 1
}
step() {
  echo "== $1"
}

# ---------- 1. 构建本地镜像（真实 digest） ----------
step "构建 buildenv 镜像（Alpine + tar）"
docker build --platform linux/amd64 -q -t w2ucem/buildenv-alpine "$ROOT_DIR/ci/buildenv" >/dev/null
# 镜像内容 ID（sha256:...）：本地构建即有；RepoDigests 仅 push 后填充
# （Linux 原生 docker 对本地镜像返回空数组）
BUILDENV_DIGEST="$(docker image inspect w2ucem/buildenv-alpine --format '{{.Id}}')"
[ -n "$BUILDENV_DIGEST" ] || fail "无法获取 buildenv digest"
echo "  buildenv digest: $BUILDENV_DIGEST"

step "构建矩阵镜像（四族 + binutils）"
declare -A MATRIX_IMAGE MATRIX_DIGEST
MATRIX_IMAGE[deb]="w2ucem/matrix-debian"
MATRIX_IMAGE[rpm]="w2ucem/matrix-fedora"
MATRIX_IMAGE[arch]="w2ucem/matrix-archlinux"
MATRIX_IMAGE[alpine]="w2ucem/matrix-alpine"
for fam in deb rpm arch alpine; do
  docker build --platform linux/amd64 -q -t "${MATRIX_IMAGE[$fam]}" "$ROOT_DIR/ci/matrix-$fam" >/dev/null
  MATRIX_DIGEST[$fam]="$(docker image inspect "${MATRIX_IMAGE[$fam]}" --format '{{.Id}}')"
  [ -n "${MATRIX_DIGEST[$fam]}" ] || fail "无法获取 $fam digest"
  echo "  matrix $fam digest: ${MATRIX_DIGEST[$fam]}"
done

# ---------- 2. fixtures（digest 注入 + mockelf） ----------
step "生成 fixtures（真实 digest 注入 + mockelf）"
if [ -x "$ROOT_DIR/tests/fixtures/elf/gen_elf.sh" ] && command -v readelf >/dev/null; then
  # 仅 Linux（有 readelf）重新生成：macOS 的 clang 会产出 Mach-O 覆盖 ELF
  "$ROOT_DIR/tests/fixtures/elf/gen_elf.sh" >/dev/null
fi
rm -rf "$FIX/repo" "$FIX/keys"
"$ROOT_DIR/tests/fixtures/gen_fixture_repo.sh" "$FIX" "$BUILDENV_DIGEST" >/dev/null

# 动态矩阵表（W2UCEM_MATRIX_FILE 覆盖编译默认值）
MATRIX_FILE="$(mktemp)"
{
  echo "[deb]"
  echo "image = \"${MATRIX_IMAGE[deb]}\""
  echo "digest = \"${MATRIX_DIGEST[deb]}\""
  echo "[rpm]"
  echo "image = \"${MATRIX_IMAGE[rpm]}\""
  echo "digest = \"${MATRIX_DIGEST[rpm]}\""
  echo "[arch]"
  echo "image = \"${MATRIX_IMAGE[arch]}\""
  echo "digest = \"${MATRIX_DIGEST[arch]}\""
  echo "[alpine]"
  echo "image = \"${MATRIX_IMAGE[alpine]}\""
  echo "digest = \"${MATRIX_DIGEST[alpine]}\""
} > "$MATRIX_FILE"
export W2UCEM_MATRIX_FILE="$MATRIX_FILE"

# ---------- 3. 完整链路 ----------
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK" "$MATRIX_FILE"' EXIT
mkdir -p "$WORK/trust"
cp "$FIX/keys/pubkey.pem" "$WORK/trust/pubkey.pem"

step "sync（验签）"
"$CLI" --root "$WORK" sync --repo "file://$FIX/repo" | grep -q "2 个模板" || fail "sync"

step "build（digest 锁 + 容器装配 + 四族 dep-scan）"
"$CLI" --root "$WORK" build mock-proton --target semi-rolling:generic:deb:stable --distro-matrix \
  | grep -q "构建完成" || fail "build 失败"

# 产物与依赖清单
step "断言产物树与 dep-manifest"
[ -f "$WORK/runtime/mock-proton/semi-rolling/deb/generic/files/bin/wine64" ] \
  || fail "缺少装配产物 wine64"
[ -f "$WORK/runtime/mock-proton/semi-rolling/deb/generic/dll-overrides.json" ] \
  || fail "缺少 dll-overrides.json"
# 四族 dep-manifest + 跨族确定性（busybox/alpine 兼容的直接证据）
step "断言四族 dep-manifest 与跨族一致性"
for fam in deb rpm arch alpine; do
  DM="$WORK/runtime/mock-proton/semi-rolling/deb/generic/dep-manifests/$fam.json"
  [ -f "$DM" ] || fail "缺少 $fam dep-manifest"
  python3 - "$DM" "$fam" <<'PYEOF'
import json, sys
path, fam = sys.argv[1], sys.argv[2]
with open(path) as f:
    data = json.load(f)
assert data["schema"] == 1, "schema != 1"
assert data["family"] == fam, "family 不匹配"
assert data["elfs"], "elfs 为空"
mockelf = [e for e in data["elfs"] if "mockelf" in e["path"]]
assert mockelf, "缺 mockelf 条目"
assert "libc.so.6" in mockelf[0]["needed"], "mockelf needed 缺 libc.so.6"
PYEOF
done
python3 - "$WORK/runtime/mock-proton/semi-rolling/deb/generic/dep-manifests" <<'PYEOF'
import json, sys, os
d = sys.argv[1]
def elfs(fam):
    with open(os.path.join(d, fam + ".json")) as f:
        return json.load(f)["elfs"]
assert elfs("deb") == elfs("alpine"), "deb/alpine 依赖清单不一致（busybox 兼容性失败）"
assert elfs("deb") == elfs("rpm"), "deb/rpm 依赖清单不一致"
assert elfs("deb") == elfs("arch"), "deb/arch 依赖清单不一致"
PYEOF

# symtab 快照（符号表校验链）
step "断言 symtab 快照与账本记录"
SNAP="$WORK/runtime/mock-proton/semi-rolling/deb/generic/.w2ucem/symtab/0.snap"
[ -f "$SNAP" ] || fail "缺少 symtab 快照"
head -1 "$SNAP" | grep -q "w2ucem-symtab v1" || fail "symtab 快照头非法"
grep -q "mock_export" "$SNAP" || fail "快照缺 mock_export 符号"
grep -q "puts@" "$SNAP" || fail "快照缺 puts 导入符号（UND）"
LEDGER_JSON="$WORK/runtime/mock-proton/semi-rolling/deb/generic/ledger/index.json"
LEDGER_FILE="$WORK/runtime/mock-proton/semi-rolling/deb/generic/ledger/$(cat "$LEDGER_JSON" | python3 -c 'import json,sys; print(json.load(sys.stdin)[-1])').json"
SNAP_SHA="$(shasum -a 256 "$SNAP" | cut -d' ' -f1)"
LEDGER_SHA="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['symtabs']['files/bin/mockelf'])" "$LEDGER_FILE")"
[ "$SNAP_SHA" = "$LEDGER_SHA" ] || fail "账本 symtabs 与快照哈希不一致"

step "verify（对账本）"
"$CLI" --root "$WORK" verify mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "record 校验" || fail "verify 失败"
"$CLI" --root "$WORK" verify mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "符号表" || fail "verify 缺符号表检查"

step "篡改产物 → verify 拦截"
echo tampered >> "$WORK/runtime/mock-proton/semi-rolling/deb/generic/files/bin/wine64"
"$CLI" --root "$WORK" verify mock-proton --target semi-rolling:generic:deb:stable 2>&1 \
  | grep -q "verification-failed" || fail "篡改未被拦截"

step "篡改账本 symtabs → 符号表校验拦截"
# 重建产物与账本（干净基线），再篡改最新账本的快照哈希：
# 产物是原版（record 检查通过），符号表复验将比对假哈希 → 拦截
"$CLI" --root "$WORK" build mock-proton --target semi-rolling:generic:deb:stable >/dev/null 2>&1 \
  || fail "重建失败"
LEDGER_FILE="$WORK/runtime/mock-proton/semi-rolling/deb/generic/ledger/$(cat "$LEDGER_JSON" | python3 -c 'import json,sys; print(json.load(sys.stdin)[-1])').json"
python3 - "$LEDGER_FILE" <<'PYEOF'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
data["symtabs"]["files/bin/mockelf"] = "0" * 64
with open(sys.argv[1], "w") as f:
    json.dump(data, f, indent=2)
PYEOF
"$CLI" --root "$WORK" verify mock-proton --target semi-rolling:generic:deb:stable 2>&1 \
  | grep -q "verification-failed" || fail "symtab 篡改未被拦截"
# 恢复：重建一次（好账本）
"$CLI" --root "$WORK" build mock-proton --target semi-rolling:generic:deb:stable >/dev/null 2>&1

# ---------- 4. 篡改官方源：时光机回退拯救（正例） + 坏哈希（负例） ----------
step "篡改官方源 → 时光机回退拯救"
WORK2="$(mktemp -d)"
mkdir -p "$WORK2/trust"
cp "$FIX/keys/pubkey.pem" "$WORK2/trust/pubkey.pem"
"$CLI" --root "$WORK2" sync --repo "file://$FIX/repo" >/dev/null
# Archive 回退源指向本地 blob 目录（alpha 无真实 Archive 服务端）
cat >> "$WORK2/config.toml" <<CFGEOF
archive_urls = ["file://$FIX/repo"]
CFGEOF
cp "$FIX/repo/src/mock-proton-1.0.tar.gz" "$FIX/repo/src/mock-proton-1.0.tar.gz.bak"
printf 'tamper' >> "$FIX/repo/src/mock-proton-1.0.tar.gz"
# 官方源已坏 → Archive blob（原版）自动接管 → fetch 成功
"$CLI" --root "$WORK2" fetch mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "已校验" || fail "时光机回退未接管"
mv "$FIX/repo/src/mock-proton-1.0.tar.gz.bak" "$FIX/repo/src/mock-proton-1.0.tar.gz"

step "坏哈希源（无 Archive 回退）→ HashMismatch"
"$CLI" --root "$WORK2" fetch bad-hash --target semi-rolling:generic:deb:stable 2>&1 \
  | grep -q "hash-mismatch" || fail "坏哈希源未被拦截"
rm -rf "$WORK2"

# ---------- 5. 回退例：上游全 404 → 时光机 ----------
step "上游 URL 全 404 → archive-check 走时光机"
mkdir -p "$FIX/repo/src/gone"
# 临时把 urls 全部指向不存在的路径（blob 仍在）：用独立坏源模板太重，
# 这里直接验证 archive-check 强制路径（跳过 urls 强制走 Archive）
"$CLI" --root "$WORK" archive-check mock-proton --target semi-rolling:generic:deb:stable \
  | grep -q "时光机回退验证通过" || fail "archive-check 失败"

# ---------- 6. 跳过语义负例：辅助族镜像锁定失败 → 跳过留痕 ----------
step "跳过语义：arch cell 假 digest → build 成功且留痕"
SKIP_MATRIX="$(mktemp)"
{
  echo "[deb]"
  echo "image = \"${MATRIX_IMAGE[deb]}\""
  echo "digest = \"${MATRIX_DIGEST[deb]}\""
  echo "[arch]"
  echo "image = \"${MATRIX_IMAGE[arch]}\""
  echo "digest = \"sha256:1111111111111111111111111111111111111111111111111111111111111111\""
} > "$SKIP_MATRIX"
WORK3="$(mktemp -d)"
mkdir -p "$WORK3/trust"
cp "$FIX/keys/pubkey.pem" "$WORK3/trust/pubkey.pem"
"$CLI" --root "$WORK3" sync --repo "file://$FIX/repo" >/dev/null
W2UCEM_MATRIX_FILE="$SKIP_MATRIX" "$CLI" --root "$WORK3" \
  build mock-proton --target semi-rolling:generic:deb:stable --distro-matrix > /tmp/skip-build.out 2>&1 \
  || fail "辅助族跳过不应导致 build 失败"
grep -q "构建完成" /tmp/skip-build.out || fail "跳过场景 build 未完成"
grep -q "跳过(镜像锁定失败):  arch" /tmp/skip-build.out || fail "跳过留痕缺失"
[ ! -f "$WORK3/runtime/mock-proton/semi-rolling/deb/generic/dep-manifests/arch.json" ] \
  || fail "arch dep-manifest 不应存在"
[ -f "$WORK3/runtime/mock-proton/semi-rolling/deb/generic/dep-manifests/deb.json" ] \
  || fail "deb dep-manifest 应存在"
SKIP_LEDGER="$WORK3/runtime/mock-proton/semi-rolling/deb/generic/ledger/$(cat "$WORK3/runtime/mock-proton/semi-rolling/deb/generic/ledger/index.json" | python3 -c 'import json,sys; print(json.load(sys.stdin)[-1])').json"
python3 - "$SKIP_LEDGER" <<'PYEOF'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
assert data["dep_scan_skipped"] == ["arch"], "dep_scan_skipped 不等于 [arch]"
PYEOF
rm -rf "$WORK3" "$SKIP_MATRIX" /tmp/skip-build.out

echo "e2e 全部通过"
