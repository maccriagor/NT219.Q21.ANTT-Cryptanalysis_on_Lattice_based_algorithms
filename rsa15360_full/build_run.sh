#!/bin/bash
# 1-Click: Build + Run + Results
set -euo pipefail

echo "🎯 RSA-15360 FULL BENCHMARK - 1 CLICK"
echo "====================================="

./build.sh
echo ""
./run.sh

echo ""
echo "🎉 SUCCESS! Package ready for submission:"
echo "📦 tar czf rsa15360_complete.tar.gz ."
echo "📸 Screenshot terminal + rsa15360_sizes.txt"