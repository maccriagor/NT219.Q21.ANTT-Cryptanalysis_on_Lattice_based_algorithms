#!/bin/bash
# Root Orchestrator: /run_all.sh

# 1. Create structure (even though Dockerfile clones PQClean, 
# main.cpp might expect these locally for headers)
mkdir -p common sign kem

# 2. Build the image
echo "Building Docker image for ARM64..."
docker build -t pqc-app .

# 3. Run Benchmark and Capture CSV
# We use 'tee' so we can see progress in the GitHub logs
echo "Running Benchmark..."
docker run --rm -i pqc-app < file.txt | tee benchmark_output.log

# 4. Extract Binary for Stage 3 Analysis
echo "Extracting ARM64 binary..."
docker create --name temp pqc-app
docker cp temp:/app/app ./app_arm_binary
docker rm temp

# 5. Final verification for GitHub Artifacts
if [ -s benchmark_output.log ]; then
    echo "Success: Data captured."
    cat benchmark_output.log
else
    echo "Error: benchmark_output.log is empty. Check main.cpp output."
fi
