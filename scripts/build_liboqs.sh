#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/versions.env

ARCH="$(uname -m)"           # x86_64 hoặc aarch64
WORK="$(pwd)/.build/liboqs"
mkdir -p "$WORK"; cd "$WORK"
[[ -d liboqs/.git ]] || git clone https://github.com/open-quantum-safe/liboqs.git
cd liboqs
git fetch --tags
git checkout "$LIBOQS_TAG"
git rev-parse HEAD > "$OLDPWD/../../docs/liboqs.commit"

# Chọn OPT_TARGET theo kiến trúc
if [[ "$ARCH" == "aarch64" ]]; then OPT="cortex-a72"; else OPT="native"; fi

build_tree () {  # $1=prefix  $2=DIST_BUILD(ON/OFF)  $3=label
  rm -rf "build-$3"
  cmake -GNinja -S . -B "build-$3" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DOQS_ALGS_ENABLED=STD \
    -DOQS_DIST_BUILD="$2" \
    -DOQS_OPT_TARGET="$OPT" \
    -DOPENSSL_ROOT_DIR="$OSSL_PREFIX" \
    -DCMAKE_INSTALL_PREFIX="$1" \
    $( [[ "$ARCH" == "aarch64" ]] && echo "-DOQS_SPEED_USE_ARM_PMU=ON" )
  cmake --build "build-$3" -j"$JOBS"
  cmake --install "build-$3"
}

# Cây 1: NEON-optimized (DIST_BUILD=OFF → tối ưu đúng micro-arch, dùng để LẤY SỐ ĐO)
build_tree "$LIBOQS_PREFIX_OPT" OFF neon
# Cây 2: portable/reference (DIST_BUILD=ON → runtime dispatch, để đối chứng)
build_tree "$LIBOQS_PREFIX_REF" ON ref