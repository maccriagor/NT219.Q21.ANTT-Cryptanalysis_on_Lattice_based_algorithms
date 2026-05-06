#!/bin/bash
# Root Orchestrator: /run_all.sh

# 1. Ensure directories exist
mkdir -p common sign kem

# 2. Build the image with an explicit 'latest' tag
echo "Building Docker image for ARM64..."
docker build -t pqc-app:latest .

# 3. Run Benchmark - Force local image usage
echo "Running Benchmark..."
# We remove -i and -t for the CI environment to avoid stream confusion
# We redirect both stdout and stderr (2>&1)
docker run --rm pqc-app:latest < file.txt > benchmark_output.log 2>&1

# 4. Diagnostic: Print the log to the Action terminal
echo "--- START LOG CONTENT ---"
cat benchmark_output.log
echo "--- END LOG CONTENT ---"

# 5. Extract Binary
echo "Extracting ARM64 binary..."
docker create --name temp pqc-app:latest
docker cp temp:/app/app ./app_arm_binary
docker rm temp

# 6. Final verification
if [ -s benchmark_output.log ]; then
    echo "Success: Data captured."
else
    echo "Error: benchmark_output.log is empty."
    exit 1
fi
