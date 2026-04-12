#!/bin/bash
# ECDSA P-521 Single-Run Runner (Academic Profiling & CSV Extraction)
set -euo pipefail

# --- Configuration ---
EXECUTABLE="./benchmark_p521"
OUTPUT_CSV="$(pwd)/benchmark_results.csv"  # Đảm bảo đường dẫn tuyệt đối

echo "⚙️ System Initialization: Enforcing performance governor..."
sudo cpufreq-set -g performance 2>/dev/null || true
sudo sysctl kernel.perf_event_paranoid=-1 2>/dev/null || true
echo "------------------------------------------------------------------------"

# 1. Check executable
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ Error: $EXECUTABLE not found. Please run ./build.sh first."
    exit 1
fi

# 2. Static Binary Profiling
echo "📊 Static Binary Profiling (Post-Compilation):"
EXACT_BYTES=$(stat -c "%s" "$EXECUTABLE" 2>/dev/null || stat -f "%z" "$EXECUTABLE")
DISK_USAGE=$(du -sh "$EXECUTABLE" | awk '{print $1}')
HASH_SHA256=$(sha256sum "$EXECUTABLE" | awk '{print $1}')

echo "  • Exact Byte Count (Binary Size)    : $EXACT_BYTES bytes"
echo "  • Storage Allocation (Disk Usage)   : $DISK_USAGE"
echo "  • Cryptographic Hash (SHA-256)      : $HASH_SHA256"
echo "------------------------------------------------------------------------"

# 3. Initialize CSV Header (Reset file mới mỗi lần chạy)
HEADER="Iter,Keygen_Cyc,Keygen_ns,Sign_Cyc,Sign_ns,Verify_Cyc,Verify_ns"
echo "$HEADER" > "$OUTPUT_CSV"

echo "🚀 Running Benchmark: Isolating process to CPU Core 0..."

# 4. Execute and Extract Data
# Sử dụng 'sudo' để đảm bảo taskset và numactl hoạt động
# Ghi output ra biến để xử lý và đồng thời hiện lên màn hình
OUTPUT=$(sudo taskset -c 0 numactl --physcpubind=0 time -v "$EXECUTABLE" 2>&1 | tee /dev/tty)

# Extract the DATA line
DATA_LINE=$(echo "$OUTPUT" | grep "^DATA:" || true)

if [ -n "$DATA_LINE" ]; then
    RAW_VALUES=$(echo "$DATA_LINE" | sed 's/DATA://' | tr -d ' ')
    echo "1,$RAW_VALUES" >> "$OUTPUT_CSV"
    # Đảm bảo user hiện tại có quyền đọc file này sau khi sudo tạo ra
    sudo chmod 666 "$OUTPUT_CSV"
    echo "------------------------------------------------------------------------"
    echo "✅ Execution Concluded: Analytical results saved to → benchmark_results.csv"
else
    echo "⚠️ Error: No DATA line found in program output."
fi
