#!/usr/bin/env bash
set -euo pipefail
OUT="docs/env-$(hostname)-$(date -u +%Y%m%dT%H%M%SZ).txt"
mkdir -p docs
{
  echo "=== HOST ==="; uname -a; cat /etc/os-release | head -2
  echo; echo "=== CPU ==="; lscpu
  echo; echo "=== COMPILERS ==="
  gcc --version | head -1; g++ --version | head -1
  clang --version 2>/dev/null | head -1 || echo "clang: N/A"
  cmake --version | head -1; ninja --version 2>/dev/null || true
  echo; echo "=== OPENSSL (self-built) ==="
  /opt/openssl-3.6.2/bin/openssl version -a
  echo; echo "=== PINNED VERSIONS / COMMITS ==="
  cat scripts/versions.env
  for f in docs/openssl.commit docs/liboqs.commit docs/oqsprovider.commit; do
    [[ -f $f ]] && echo "$f: $(cat $f)"
  done
  echo; echo "=== GOVERNOR / THROTTLE ==="
  cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null | sort -u
  command -v vcgencmd >/dev/null && { vcgencmd measure_temp; vcgencmd get_throttled; vcgencmd measure_clock arm; }
} | tee "$OUT"
echo "Saved -> $OUT"