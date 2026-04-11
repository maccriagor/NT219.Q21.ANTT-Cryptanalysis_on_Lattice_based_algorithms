#!/bin/bash
# ECDSA P-521 Full Benchmark Runner
set -euo pipefail

echo "⚙️  Setup: CPU pinning + performance governor"
sudo cpufreq-set -g performance 2>/dev/null || true
sudo sysctl kernel.perf_event_paranoid=-1 2>/dev/null || true

echo "🔥 Pinning to CPU core 0"
sudo taskset -c 0 numactl --physcpubind=0 time -v ./benchmark_p521

echo ""
echo "📊 RESULTS SUMMARY:"
echo "=================="
cat ecdsa_p521_sizes.txt 2>/dev/null || echo "No sizes.txt"
echo ""
echo "💾 Artifacts:"
ls -la *.txt *.csv 2>/dev/null || echo "No additional files"
echo ""
echo "✅ BENCHMARK COMPLETE - Ready for submission!"