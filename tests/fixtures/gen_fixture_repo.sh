#!/usr/bin/env bash
# 生成测试仓库 fixtures（测试/CI 用，产品外）：
#   - ed25519 密钥对（keys/key.pem、keys/pubkey.pem）
#   - 已签名索引 repo.db + repo.db.sig
#   - mock-proton 模板（urls 指向 file:// 绝对路径，第二个镜像故意 404）
#   - 源 tarball + Archive blobs（时光机回退用）
#   - bad-hash 模板（sha256 对不上，负例）
# 用法: tests/fixtures/gen_fixture_repo.sh <fixtures_dir>
set -euo pipefail

FIX="$(cd "$1" && pwd)"
BUILDENV_DIGEST="${2:-sha256:0000000000000000000000000000000000000000000000000000000000000000}"
REPO="$FIX/repo"
mkdir -p "$REPO/templates/mock-proton" "$REPO/src" "$REPO/blobs" "$FIX/keys"

# ---------- 密钥 ----------
openssl genpkey -algorithm ED25519 -out "$FIX/keys/key.pem" 2>/dev/null
openssl pkey -in "$FIX/keys/key.pem" -pubout -out "$FIX/keys/pubkey.pem" 2>/dev/null

# ---------- 源 tarball（内容固定，哈希确定） ----------
PAYLOAD_DIR="$FIX/mock_payload"
mkdir -p "$PAYLOAD_DIR/files/bin"
echo '#!/bin/sh
echo "mock wine payload"
' > "$PAYLOAD_DIR/files/bin/wine64"
chmod +x "$PAYLOAD_DIR/files/bin/wine64"
tar -C "$PAYLOAD_DIR" -czf "$REPO/src/mock-proton-1.0.tar.gz" files

TARBALL_SHA="$(shasum -a 256 "$REPO/src/mock-proton-1.0.tar.gz" | cut -d' ' -f1)"

# Archive blob（时光机）：内容寻址
mkdir -p "$REPO/blobs"
cp "$REPO/src/mock-proton-1.0.tar.gz" "$REPO/blobs/$TARBALL_SHA"

# ---------- mock-proton 模板 ----------
TPL="$REPO/templates/mock-proton/template.toml"
cat > "$TPL" <<EOF
[template]
format = "0.1"
id = "mock-proton"
name = "Mock Proton (assembly fixture)"
version = "1.0"
kind = "assembly"
upstream = "https://example.invalid/mock-proton"

[meta]
maintainer = "w2ucem tests <tests@w2ucem.invalid>"
license = "MIT"
channels = ["semi-rolling"]
gpu_variants = ["generic"]
distro_families = ["arch", "deb"]

[buildenv]
image = "w2ucem/buildenv-alpine"
digest = "$BUILDENV_DIGEST"

[sources."mock-proton-1.0"]
urls = [
  "file://$REPO/src/mock-proton-1.0.tar.gz",
  "file://$REPO/src/broken/mock-proton-1.0.tar.gz",
]
sha256 = "$TARBALL_SHA"
archive_id = "sha256:$TARBALL_SHA"

[build_depends]
default = []
arch = ["libx11"]
deb = ["libx11-dev"]

[[assemble.steps]]
type = "extract"
source = "mock-proton-1.0"
to = "\$build/src"

[[assemble.steps]]
type = "dll-override"
rules = ["xaudio2_7=native,builtin"]
output = "dll-overrides.json"

[[assemble.steps]]
type = "copy"
from = "\$build/src/files"
to = "\$prefix"

[[assemble.steps]]
type = "dep-scan"
elfs = ["\$prefix/files/bin/wine64"]
output = "dep-manifest.json"

[[assemble.steps]]
type = "manifest"
required = ["\$prefix/files/bin/wine64", "dll-overrides.json", "dep-manifest.json"]

[manifest]
[[manifest.files]]
path = "files/bin/wine64"
sha256 = ""
[[manifest.files]]
path = "dll-overrides.json"
sha256 = ""
[[manifest.files]]
path = "dep-manifest.json"
sha256 = ""
EOF

TPL_SHA="$(shasum -a 256 "$TPL" | cut -d' ' -f1)"

# ---------- bad-hash 模板（负例：声明 sha256 与实际 tarball 不符） ----------
BAD_DIR="$REPO/templates/bad-hash"
mkdir -p "$BAD_DIR"
BAD_TPL="$BAD_DIR/template.toml"
cat > "$BAD_TPL" <<EOF
[template]
format = "0.1"
id = "bad-hash"
name = "Bad hash (negative fixture)"
version = "1.0"
kind = "assembly"

[meta]
maintainer = "w2ucem tests"
license = "MIT"
channels = ["semi-rolling"]
gpu_variants = ["generic"]
distro_families = ["deb"]

[buildenv]
image = "w2ucem/buildenv-alpine"
digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000"

[sources."broken-1.0"]
urls = ["file://$REPO/src/mock-proton-1.0.tar.gz"]
sha256 = "0000000000000000000000000000000000000000000000000000000000000000"

[manifest]
[[manifest.files]]
path = "files/bin/wine64"
sha256 = "0000000000000000000000000000000000000000000000000000000000000000"
EOF

BAD_SHA="$(shasum -a 256 "$BAD_TPL" | cut -d' ' -f1)"

# ---------- 索引 ----------
cat > "$REPO/repo.db" <<EOF
{
  "schema_version": 1,
  "generated_at": "2026-10-28T00:00:00Z",
  "templates": [
    {
      "id": "mock-proton",
      "version": "1.0",
      "sha256": "$TPL_SHA",
      "path": "templates/mock-proton/template.toml",
      "review_status": "reviewed",
      "channels": ["semi-rolling", "stable"]
    },
    {
      "id": "bad-hash",
      "version": "1.0",
      "sha256": "$BAD_SHA",
      "path": "templates/bad-hash/template.toml",
      "review_status": "experimental",
      "channels": ["semi-rolling"]
    }
  ]
}
EOF

openssl pkeyutl -sign -inkey "$FIX/keys/key.pem" -rawin -in "$REPO/repo.db" \
  -out "$REPO/repo.db.sig" 2>/dev/null

echo "fixtures 已生成于 $FIX"
echo "  tarball sha256: $TARBALL_SHA"
echo "  pubkey: $FIX/keys/pubkey.pem"
