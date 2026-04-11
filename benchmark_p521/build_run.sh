#!/bin/bash 
# ECDSA P-521 Single-Run Runner (Size & CSV Extraction)
set -euo pipefail

# --- Configuration ---
EXECUTABLE="./benchmark_p521"
OUTPUT_CSV="benchmark_results.csv"

echo "⚙️  Setup: System Performance Governor"
sudo cpufreq-set -g performance 2>/dev/null || true
sudo sysctl kernel.perf_event_paranoid=-1 2>/dev/null || true
echo "------------------------------------------------"


# 1. Check executable
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ Error: $EXECUTABLE not found. Please run ./build.sh first."
    exit 1

fi


# 2. Measure Binary Size
echo "📏 Measuring Binary Size..."
echo "--- Memory sections (text, data, bss) ---"
size "$EXECUTABLE" 2>/dev/null || echo "⚠️ 'size' command not available."
echo "--- Storage size ---"
ls -lh "$EXECUTABLE" | awk '{print "Total size on disk: " $5}'
echo "------------------------------------------------"
echo ""


# 3. Initialize CSV Header
HEADER="Iter,Keygen_Cyc,Keygen_ns,Sign_Cyc,Sign_ns,Verify_Cyc,Verify_ns"
echo "$HEADER" > "$OUTPUT_CSV"

echo "🚀 Running Benchmark (Single Execution)..."


# 4. Execute and Extract Data
# The C++ code handles CPU pinning internally
OUTPUT=$($EXECUTABLE)

# Extract the DATA line
DATA_LINE=$(echo "$OUTPUT" | grep "^DATA:" || true)

if [ -n "$DATA_LINE" ]; then
    # Clean "DATA:" prefix and spaces
    RAW_VALUES=$(echo "$DATA_LINE" | sed 's/DATA://' | tr -d ' ')
    # Write to CSV as Iteration 1
    echo "1,$RAW_VALUES" >> "$OUTPUT_CSV"
    echo "✅ Data captured: $RAW_VALUES"

else
    echo "⚠️ Error: No DATA line found in program output."

fi


echo "------------------------------------------------"

echo "✅ Done! Results saved to → $OUTPUT_CSV" 
