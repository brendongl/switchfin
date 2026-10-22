#!/bin/bash
# Build Switchfin.nro (deko3d) inside the devkitPro image upstream CI pins.
set -euo pipefail
SRC=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=$HOME/.cache/switchfin-nx; mkdir -p "$CACHE"
docker run --rm -v "$SRC":/src -v "$CACHE":/cache -w /src devkitpro/devkita64:20260219 bash -ec '
BASE_URL=https://github.com/dragonflylee/switchfin/releases/download/switch-portlibs
dl(){ f=/cache/$(basename $1); [ -s $f ] || curl -fsSL -o $f $1; echo $f; }
dkp-pacman --noconfirm -R switch-libmpv >/dev/null
dkp-pacman --noconfirm -U $(dl $BASE_URL/libuam-master-1-any.pkg.tar.zst) >/dev/null
dkp-pacman --noconfirm -U $(dl $BASE_URL/hacBrewPack-3.05-1-x86_64.pkg.tar.zst) >/dev/null
for pkg in mbedtls-3.6.7-1 libssh2-1.11.1-1 dav1d-1.5.4-1 curl-8.21.0-3 ffmpeg-7.1.5-5 mupdf-1.24.11-1 libmpv-deko3d-0.36.0-5 nspmini-main-1; do
  dkp-pacman --noconfirm -U $(dl $BASE_URL/switch-${pkg}-any.pkg.tar.zst) >/dev/null
done
git config --system --add safe.directory "*"
[ -d /cache/libusbhsfs ] || git clone -q https://github.com/DarkMatterCore/libusbhsfs.git -b v0.2.10 --depth=1 /cache/libusbhsfs
make -s -C /cache/libusbhsfs BUILD_TYPE=GPL install -j$(nproc) >/dev/null
cmake -B build-nx -DUSE_DEKO3D=ON -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_LIBUSBHSFS=ON -DUSE_MUPDF=ON -DBUILTIN_NSP=ON >/dev/null
make -C build-nx Switchfin.nro -j$(nproc) 2>&1 | grep -E "error|warning: unused|Built target|\.nro" | tail -20
chown -R '"$(id -u):$(id -g)"' build-nx
'
ls -la "$SRC/build-nx/Switchfin.nro"
