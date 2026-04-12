#!/bin/bash
# RSA-15360 Benchmark Runner (Enhanced)
set -euo pipefail

echo "⚙️  CPU Setup: pinning + performance governor"
sudo cpufreq-set -g performance 2>/dev/null || true
sudo sysctl kernel.perf_event_paranoid=-1 2>/dev/null || true

echo "🔥 Pinning to CPU core 0 + Monitoring"
sudo taskset -c 0 numactl --physcpubind=0 time -v ./rsa15360_full

echo ""
echo "📊 RESULTS SUMMARY:"
echo "=================="
echo "✅ BENCHMARK COMPLETE - Ready for RQ1 comparison!"
