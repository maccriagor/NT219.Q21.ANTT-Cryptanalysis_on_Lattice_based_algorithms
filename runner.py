import subprocess
import csv
import os
import sys

# --- Configuration ---
# Inside the container, the binary is at /app/app
BINARY_PATH = "./app" 
INPUT_FILE = "file.txt"

def run_benchmark():
    # 1. Check input file
    if not os.path.exists(INPUT_FILE):
        print(f"❌ Error: {INPUT_FILE} not found.", file=sys.stderr)
        return

    with open(INPUT_FILE, "r") as f:
        lines = f.readlines()
        if not lines:
            return

        try:
            n_iterations = int(lines[0].strip())
        except ValueError:
            return
        
        message_content = "".join(lines[1:])

    # Print Header for the CSV output
    print("Iter,K_KEM_Cyc,K_KEM_ns,K_SIG_Cyc,K_SIG_ns,Sign_Cyc,Sign_ns,Verify_Cyc,Verify_ns,E_Cyc,E_ns,D_Cyc,D_ns")

    for i in range(n_iterations):
        process = subprocess.Popen(
            [BINARY_PATH],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        stdout, stderr = process.communicate(input=message_content)

        for line in stdout.splitlines():
            if line.startswith("DATA:"):
                raw_values = line.replace("DATA:", "").strip()
                # Print directly to stdout so run_all.sh captures it
                print(f"{i + 1},{raw_values}")

if __name__ == "__main__":
    run_benchmark()
