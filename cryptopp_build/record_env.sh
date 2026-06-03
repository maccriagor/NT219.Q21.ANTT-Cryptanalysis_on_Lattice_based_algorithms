#!/usr/bin/env bash
# record_env.sh — Ghi version compiler + cờ + CPU governor (yêu cầu tái lập §7.3).
#   bash cryptopp_build/record_env.sh [out.txt]
set -uo pipefail
OUT="${1:-build_env.txt}"
{
  echo "== timestamp =="; date -u '+%Y-%m-%dT%H:%M:%SZ'
  echo "== OS / arch =="; uname -srvmo; (. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME")
  echo "== compilers / tools =="
  g++  --version 2>/dev/null | head -1
  gcc  --version 2>/dev/null | head -1
  command -v clang++ >/dev/null && clang++ --version | head -1
  cmake --version 2>/dev/null | head -1
  make  --version 2>/dev/null | head -1
  ld    --version 2>/dev/null | head -1
  command -v aarch64-linux-gnu-g++ >/dev/null && aarch64-linux-gnu-g++ --version | head -1
  echo "== CPU =="; lscpu 2>/dev/null | grep -iE 'architecture|^model name|cpu max mhz|^flags'
  echo "== governor =="; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/a
  echo "== turbo/boost (x86) =="; cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo n/a
  echo "== throttled (Pi) =="; vcgencmd get_throttled 2>/dev/null || echo n/a
} | tee "$OUT"

cat <<'EOF'

# --- CỐ ĐỊNH MÔI TRƯỜNG TRƯỚC KHI ĐO (chạy với sudo) ---
#   sudo cpupower frequency-set -g performance          # governor = performance
#   echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost   # tắt turbo (x86)
#   echo 0 | sudo tee /proc/sys/kernel/randomize_va_space     # tắt ASLR (khi đếm cycle)
#   taskset -c 2 ./cryptest.exe b 3                      # ghim core 2
#
# --- CỜ BIÊN DỊCH (tài liệu hóa) ---
#   -O2            : ổn định, dễ so sánh (mặc định nên dùng cho benchmark công bằng)
#   -O3            : tối ưu mạnh hơn (có thể vectorize thêm)
#   -march=native  : tối ưu cho CPU build-host — NHANH NHẤT nhưng KHÔNG portable, KHÔNG dùng khi cross
#   x86 SIMD       : -maes -mavx2 -msse4.1   |   ARM: -march=armv8-a[+crypto]
#   Crypto++ tự dò CPU lúc chạy (CPU dispatch) nên -march ít quan trọng hơn; cross thì BẮT BUỘC đặt -march đích
EOF
