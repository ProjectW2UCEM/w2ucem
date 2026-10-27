# W2UCEM 发行版矩阵镜像（Debian 系，dep-scan 专用）。
# 需包含 readelf（binutils）——依赖清单扫描的执行环境。
FROM debian:stable-slim

RUN apt-get update && apt-get install -y --no-install-recommends binutils \
    && rm -rf /var/lib/apt/lists/*
