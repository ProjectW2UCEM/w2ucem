# W2UCEM 发行版矩阵镜像（Fedora/rpm 系，dep-scan 专用）。
# 需包含 readelf（binutils）——依赖清单扫描的执行环境。
FROM fedora:latest

RUN dnf install -y binutils && dnf clean all
