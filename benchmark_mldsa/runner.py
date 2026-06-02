#!/usr/bin/env python3
# ML-DSA runner — quét 3 mức NIST (44/65/87) × N lần (input.txt) -> data/micro/<arch>/
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.exit(subprocess.call([
    sys.executable, os.path.join(ROOT, "scripts", "micro_runner.py"),
    "--name", "mldsa", "--bin", os.path.join(HERE, "benchmark_mldsa"),
    "--levels", "44:2,65:3,87:5",
    "--cols", "Keygen_Cyc,Keygen_ns,Sign_Cyc,Sign_ns,Verify_Cyc,Verify_ns",
    "--input", os.path.join(HERE, "input.txt"),
]))
