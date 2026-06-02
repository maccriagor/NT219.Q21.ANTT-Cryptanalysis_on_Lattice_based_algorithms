#!/usr/bin/env bash
# capture_env.sh — Ghi cấu hình máy vào /data/env/, tên file theo nền tảng.
#   x86_64  -> env_linux-x86.txt
#   aarch64 -> env_linux-arm.txt
# Dùng:  capture_env.sh [OUT_DIR]      (mặc định OUT_DIR=/data/env)
# Chạy trong container (env khớp lúc đo) hoặc trên host đều được.
set -euo pipefail

OUT_DIR="${1:-/data/env}"
mkdir -p "$OUT_DIR"

case "$(uname -m)" in
  x86_64|amd64)  PLAT="linux-x86" ;;
  aarch64|arm64) PLAT="linux-arm" ;;
  *)             PLAT="linux-$(uname -m)" ;;
esac
FILE="$OUT_DIR/env_${PLAT}.txt"

{
  echo "================ ENV CAPTURE ================"
  echo "platform   : $PLAT   (uname -m = $(uname -m))"
  echo "timestamp  : $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "hostname   : $(hostname)"
  echo "in_docker  : $([ -f /.dockerenv ] && echo yes || echo no)"
  echo
  echo "---- OS / Kernel ----"
  ( . /etc/os-release 2>/dev/null && echo "$PRETTY_NAME" ) || true
  uname -srvmo
  echo
  echo "---- CPU ----"
  lscpu 2>/dev/null | grep -iE 'architecture|^model name|^cpu\(s\)|cpu max mhz|^flags' || true
  printf "SIMD       : "; grep -ioE 'asimd|neon|avx2|avx512[a-z]*' /proc/cpuinfo 2>/dev/null | sort -u | tr '\n' ' '; echo
  echo
  echo "---- Memory ----"
  free -h 2>/dev/null | head -2 || true
  echo
  echo "---- Frequency / Throttle ----"
  printf "governor   : "; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "n/a"
  printf "throttled  : "; vcgencmd get_throttled 2>/dev/null || echo "n/a (chỉ có trên Pi host, không có trong container)"
  echo
  echo "---- Toolchain ----"
  echo "openssl    : $(openssl version 2>/dev/null || echo n/a)"
  echo "gcc        : $(gcc --version 2>/dev/null | head -1 || echo n/a)"
  echo "cmake      : $(cmake --version 2>/dev/null | head -1 || echo n/a)"
  if [ -f /opt/build/versions.txt ]; then
    echo "---- Pinned versions (image) ----"; cat /opt/build/versions.txt
  fi
} | tee "$FILE"

echo ">>> Saved: $FILE"
