#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname -- "$0")"
[ -x ./benchmark_mldsa ] || bash ./build.sh
python3 runner.py
