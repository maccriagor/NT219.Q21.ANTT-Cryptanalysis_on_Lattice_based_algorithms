#!/bin/bash
# Root Orchestrator: /run_all.sh

# 1. Ensure directories exist
mkdir -p common sign kem

# 2. Build the image
echo "Building Docker image for ARM64..."
docker build -t pqc-app .

# 3. Run Benchmark - Capture stdout AND stderr
echo "Running Benchmark..."
# Removed -t. Added 2>&1 to merge error messages into the log.
docker run --rm -i pqc-app < file.txt > benchmark_output.log 2>&1

# 4. Extract Binary
echo "Extracting ARM64 binary..."
docker create --name temp pqc-app
docker cp temp:/app/app ./app_arm_binary
docker rm temp

# 5. Diagnostic: Print the log to the Action terminal
echo "--- START LOG CONTENT ---"
cat benchmark_output.log
echo "--- END LOG CONTENT ---"

# 6. Final verification for GitHub Artifacts
if [ -s benchmark_output.log ]; then
    echo "Success: Data captured."
else
    echo "Error: benchmark_output.log is empty."
    exit 1 # Force the Action to fail so you know it's empty
fi
