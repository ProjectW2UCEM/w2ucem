# W2UCEM 发行版矩阵镜像（Arch 系，dep-scan 专用）。
# 需包含 readelf（binutils）——依赖清单扫描的执行环境。
FROM archlinux:latest

# archlinux 官方镜像的经典坑：密钥环未初始化/过期，先初始化再装包。
# --disable-sandbox：新版 pacman 的 seccomp 沙箱在模拟环境（Apple Silicon 上
# 跑 amd64 镜像）下报 "error restricting syscalls via seccomp"；CI 原生环境
# 亦无害（镜像构建本身即隔离环境）。
RUN pacman-key --init \
    && pacman-key --populate archlinux \
    && pacman -Syu --noconfirm --disable-sandbox binutils \
    && rm -rf /var/cache/pacman/pkg/*
