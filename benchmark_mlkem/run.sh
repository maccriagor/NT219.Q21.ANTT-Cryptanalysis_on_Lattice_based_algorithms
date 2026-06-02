#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname -- "$0")"
[ -x ./benchmark_mlkem ] || bash ./build.sh
python3 runner.py
