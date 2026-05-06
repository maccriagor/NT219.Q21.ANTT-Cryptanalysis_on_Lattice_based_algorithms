#!/bin/bash
# Root Orchestrator: /run_all.sh

# 1. Ensure directories exist
mkdir -p common sign kem

# 2. Build the image and FORCE failure if build fails
echo "Building Docker image for ARM64..."
# We add '|| exit 1' to stop immediately if the build fails
docker build -t pqc-app:latest . || { echo "ERROR: Docker build failed!"; exit 1; }

# 3. VERIFY: Show images to confirm pqc-app exists
echo "Verifying local images..."
docker images

# 4. Run Benchmark
echo "Running Benchmark..."
# Removed the < file.txt redirect for a moment to see if the container even starts
docker run --rm pqc-app:latest > benchmark_output.log 2>&1 || echo "Docker run returned non-zero exit code"

# 5. Diagnostic: Print the log
echo "--- START LOG CONTENT ---"
cat benchmark_output.log
echo "--- END LOG CONTENT ---"

# 6. Extract Binary
echo "Extracting ARM64 binary..."
docker create --name temp pqc-app:latest
docker cp temp:/app/app ./app_arm_binary || echo "Binary extraction failed"
docker rm temp

# 7. Final verification
if grep -q "DATA:" benchmark_output.log; then
    echo "Success: Benchmark data found in log."
else
    echo "Error: No benchmark data found. Check the log above for errors."
    exit 1
fi
