#!/usr/bin/env python3
# AES-GCM runner — quét 3 mức (128/192/256) × N lần (input.txt) -> data/micro/<arch>/
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.exit(subprocess.call([
    sys.executable, os.path.join(ROOT, "scripts", "micro_runner.py"),
    "--name", "aes", "--bin", os.path.join(HERE, "benchmark_aes"),
    "--levels", "128:1,192:3,256:5",
    "--cols", "Encrypt_Cyc,Encrypt_ns,Decrypt_Cyc,Decrypt_ns",
    "--input", os.path.join(HERE, "input.txt"),
]))
