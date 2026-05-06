#!/bin/bash
# Micro-benchmark: Runs the actual performance tests

echo "Starting Kyber/Dilithium Benchmark..."
echo "Timestamp: $(date)"

# Run your compiled program (assuming the Dockerfile names it 'app')
# If your program outputs CSV-formatted data, it will be captured here
./app

echo "Benchmark Complete."