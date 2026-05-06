import subprocess
import csv
import os

# --- Configuration ---
DOCKER_IMAGE = "my-app"
INPUT_FILE = "file.txt"
OUTPUT_CSV = "benchmark_results.csv"

def run_benchmark():
    # 1. Check input file
    if not os.path.exists(INPUT_FILE):
        print(f"❌ Error: {INPUT_FILE} not found.")
        return

    with open(INPUT_FILE, "r") as f:
        lines = f.readlines()
        if not lines:
            print("❌ Error: input.txt is empty.")
            return

        try:
            n_iterations = int(lines[0].strip())
        except ValueError:
            print("❌ Error: First line must be integer.")
            return

        message_content = "".join(lines[1:])

    print(f"🚀 Running {n_iterations} iterations via Docker...")
      
    
    header = ["Iter", "K_KEM_Cyc", "K_KEM_ns", "K_SIG_Cyc", "K_SIG_ns", "Sign_Cyc", "Sign_ns", "Verify_Cyc", "Verify_ns", "E_Cyc", "E_ns", "D_Cyc", "D_ns"]

    with open(OUTPUT_CSV, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)

        for i in range(n_iterations):
            # Run Docker container with stdin
            process = subprocess.Popen(
                ["./app"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            stdout, stderr = process.communicate(input=message_content)

            # Extract DATA line
            found_data = False
            for line in stdout.splitlines():
                if line.startswith("DATA:"):
                    raw_values = line.replace("DATA:", "").strip().split(",")
                    writer.writerow([i + 1] + raw_values)
                    found_data = True
                    break

            if not found_data:
                print(f"⚠️ No DATA in iteration {i+1}")
                if stderr:
                    print(stderr)

            # Progress
            if (i + 1) % max(1, n_iterations // 20) == 0:
                percent = int(((i + 1) / n_iterations) * 100)
                print(f"⏳ {percent}% ({i+1}/{n_iterations})")

    print(f"✅ Done → {OUTPUT_CSV}")

if __name__ == "__main__":
    run_benchmark()
