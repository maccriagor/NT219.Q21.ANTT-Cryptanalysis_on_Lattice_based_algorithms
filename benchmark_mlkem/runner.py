#!/usr/bin/env python3
# ML-KEM runner — quét 3 mức NIST (512/768/1024) × N lần (input.txt) -> data/micro/<arch>/
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.exit(subprocess.call([
    sys.executable, os.path.join(ROOT, "scripts", "micro_runner.py"),
    "--name", "mlkem", "--bin", os.path.join(HERE, "benchmark_mlkem"),
    "--levels", "512:1,768:3,1024:5",
    "--cols", "Keygen_Cyc,Keygen_ns,Encaps_Cyc,Encaps_ns,Decaps_Cyc,Decaps_ns",
    "--input", os.path.join(HERE, "input.txt"),
]))
