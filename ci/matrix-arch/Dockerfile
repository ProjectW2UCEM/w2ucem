# W2UCEM 发行版矩阵镜像（Arch 系，dep-scan 专用）。
# 需包含 readelf（binutils）——依赖清单扫描的执行环境。
FROM archlinux:latest

RUN pacman -Sy --noconfirm binutils && rm -rf /var/cache/pacman/pkg/*
