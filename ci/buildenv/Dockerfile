# W2UCEM buildenv 构建镜像（不可变，Alpine 基板）。
# 含装配所需基础工具 + binutils（readelf：符号表提取）。
# 容器零网络，源由宿主注入（只读挂载）。
FROM alpine:3.20

RUN apk add --no-cache tar binutils
