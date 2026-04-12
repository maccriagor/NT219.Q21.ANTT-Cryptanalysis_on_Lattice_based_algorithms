#!/bin/bash
# 1-Click: Build + Run RSA-15360
set -euo pipefail

echo "🎯 RSA-15360 BENCHMARK - 1 CLICK (OpenSSL 3.0)"
echo "============================================="

./build.sh
echo ""
./run.sh

echo ""
echo "🎉 SUCCESS! Results ready for Kyber/Dilithium comparison:"
echo "📦 tar czf rsa15360_benchmark.tar.gz *"
