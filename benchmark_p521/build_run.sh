#!/bin/bash
# 1-Click: Build + Run + Results
set -euo pipefail

echo "🎯 ECDSA P-521 FULL BENCHMARK - 1 CLICK"
echo "======================================="

./build.sh
echo ""
./run.sh

echo ""
echo "🎉 SUCCESS! Package ready for submission:"
echo "📦 tar czf ecdsa_p521_complete.tar.gz ."
echo "📸 Screenshot terminal + ecdsa_p521_sizes.txt"