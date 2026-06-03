#!/usr/bin/env bash
set -euo pipefail
echo "[*] Đặt governor = performance"
if command -v cpupower >/dev/null; then
  sudo cpupower frequency-set -g performance
else
  for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee "$g" >/dev/null
  done
fi

echo "[*] Tắt turbo boost (x86)"
if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
  echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo   # 1 = turbo OFF
elif [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
  echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost           # 0 = boost OFF (AMD/ARM)
fi

echo "[*] Kiểm tra tần số ổn định"
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq | sort -u

# --- Raspberry Pi: chống thermal throttle ---
if command -v vcgencmd >/dev/null; then
  echo "[*] Pi: nhiệt độ / throttle / clock"
  vcgencmd measure_temp
  vcgencmd measure_clock arm
  TH=$(vcgencmd get_throttled)
  echo "throttle = $TH"
  [[ "$TH" == "throttled=0x0" ]] || echo "!!! CẢNH BÁO: đang/đã bị throttle hoặc undervoltage — KHÔNG đo lúc này !!!"
fi