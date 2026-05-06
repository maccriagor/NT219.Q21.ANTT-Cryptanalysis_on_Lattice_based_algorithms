#!/bin/bash
# Orchestrator: Sets up the environment and runs the container

# 1. Ensure the directory structure exists for the build
mkdir -p common sign kem

# 2. Build the image for the ARM64 runner
docker build -t pqc-app .

# 3. Execute the micro-benchmark script inside the container
# We pass file.txt as input as specified in your README
docker run --rm -i pqc-app < file.txt > benchmark_output.log

# 4. Extract the binary for your project artifacts
docker create --name temp pqc-app
docker cp temp:/app/app ./app_arm_binary
docker rm temp

# 5. Output results to the runner so they are captured by results_arm64.csv
cat benchmark_output.log