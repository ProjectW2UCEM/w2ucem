# W2UCEM 开发容器镜像 —— Ubuntu 26.04 LTS。
# 兼容三种宿主运行时：Docker / Docker Rootless / Podman。
# 关键约定：全程非 root 用户（ubuntu），适配 rootless 运行时的 UID 映射与文件属主。

FROM ubuntu:26.04

# 避免 apt 交互提示
ENV DEBIAN_FRONTEND=noninteractive

# --- 国内镜像源（加速 apt；可用 --build-arg APT_MIRROR=... 覆盖） ---
# 默认阿里云（HTTP）。注意：ubuntu:26.04 基础镜像不带 ca-certificates（/etc/ssl/certs 为空），
# 若用 HTTPS 镜像源，会在安装 ca-certificates 之前就因证书校验失败而 apt update 失败。
# 故镜像源走 HTTP；ca-certificates 在下方的 apt install 中一并安装，
# 装好后容器内 git/curl 拉取 https（如 FetchContent）不受影响。
ARG APT_MIRROR=http://mirrors.aliyun.com/ubuntu/
ARG APT_MIRROR_PORTS=http://mirrors.aliyun.com/ubuntu-ports/
# Ubuntu 24.04+ 用 deb822 格式（/etc/apt/sources.list.d/ubuntu.sources）；旧格式兼容处理。
RUN set -eux; \
    for f in /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list; do \
      if [ -f "$f" ]; then \
        sed -i \
          -e "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
          -e "s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
          -e "s|http://ports.ubuntu.com/ubuntu-ports/|${APT_MIRROR_PORTS}|g" \
          "$f"; \
      fi; \
    done

# 工具链 + 构建期系统依赖 + 测试依赖：
#   build-essential         gcc/g++/make（C++20）
#   cmake / ninja-build     构建系统（项目要求 CMake ≥3.20）
#   git / ca-certificates   FetchContent 拉取锁定依赖（HTTPS）
#   pkg-config              库探测
#   libcurl4-openssl-dev    libcurl（下载）
#   libssl-dev              OpenSSL（sha256 / ed25519 验签）
#   zlib1g-dev              zlib
#   binutils                readelf（符号表快照 / dep-scan）
#   gdb                     调试（CLion 远程调试）
#   python3                 单测 mock 服务器 + e2e 断言（纯 stdlib）
#   podman / podman-docker  容器引擎 CLI（rootless；podman-docker 提供 docker 别名）
#   fuse-overlayfs          rootless overlay 存储驱动
#   slirp4netns             rootless 网络
#   sudo / curl / jq        开发便利
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ca-certificates \
    libcurl4-openssl-dev \
    libssl-dev \
    zlib1g-dev \
    binutils \
    gdb \
    python3 \
    podman \
    podman-docker \
    fuse-overlayfs \
    slirp4netns \
    sudo \
    curl \
    jq \
    && rm -rf /var/lib/apt/lists/*

# 非 root 用户：复用 Ubuntu 官方镜像内置的 ubuntu 用户（UID 1000，与多数宿主一致）。
# rootless 运行时下必须以非 root 用户写挂载卷，否则宿主机文件属主会变成 root。
# ubuntu:26.04 镜像自带 UID/GID 1000 的 ubuntu 用户（无 sudo），此处仅补免密 sudo 权限。
ARG USERNAME=ubuntu
RUN echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/"$USERNAME" \
    && chmod 0440 /etc/sudoers.d/"$USERNAME"

# rootless podman 的从属 UID/GID 映射（容器内再跑容器的前提）。
# 注意：容器内能否真正起容器还取决于宿主的嵌套 user-ns 与 /dev/fuse 支持；
# 不满足时 w2ucem 会自动回退 `--engine fake`（宿主直跑），或改为挂载宿主
# docker/podman socket（见 devcontainer.json 注释）。
RUN echo "$USERNAME:100000:65536" >> /etc/subuid \
    && echo "$USERNAME:100000:65536" >> /etc/subgid \
    && mkdir -p "/home/$USERNAME/.config/containers" \
    && chown -R "$USERNAME:$USERNAME" "/home/$USERNAME"

USER $USERNAME
WORKDIR /workspaces/w2ucem
