#!/bin/bash
# 1. Ensure directories exist
mkdir -p common sign kem

# 2. Build and check for success
docker build -t pqc-app . || exit 1

# 3. Run and ensure output is captured
# Use 'tee' so you can see it in logs AND save it
docker run --rm -i pqc-app < file.txt | tee benchmark_output.log

# 4. Extract binary - check if source exists first
docker create --name temp pqc-app
docker cp temp:/app/app ./app_arm_binary || echo "Extraction failed!"
docker rm temp
