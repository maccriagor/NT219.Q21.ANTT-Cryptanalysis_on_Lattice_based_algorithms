import subprocess
import csv
import os

# --- Configuration ---
EXECUTABLE = "./benchmark_p521"
INPUT_FILE = "input.txt"
OUTPUT_CSV = "benchmark_results.csv"

def run_benchmark():
    # 1. Đọc số lần lặp N từ input.txt
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
            print("❌ Error: First line of input.txt must be an integer.")
            return

    # 2. Kiểm tra file thực thi
    if not os.path.exists(EXECUTABLE):
        print(f"❌ Error: {EXECUTABLE} not found. Please compile first.")
        return

    # 3. Cấu hình hệ thống (Chạy 1 lần duy nhất)
    print("⚙️ System Initialization: Enforcing performance governor...")
    subprocess.run("sudo cpufreq-set -g performance 2>/dev/null || true", shell=True)
    subprocess.run("sudo sysctl kernel.perf_event_paranoid=-1 2>/dev/null || true", shell=True)
    print("-" * 72)

    # 4. Trích xuất thông tin tĩnh của Binary
    try:
        exact_bytes = os.path.getsize(EXECUTABLE)
        hash_sha256 = subprocess.check_output(["sha256sum", EXECUTABLE]).decode().split()[0]
        print("📊 Static Binary Profiling:")
        print(f"  • Exact Byte Count : {exact_bytes} bytes")
        print(f"  • SHA-256 Hash     : {hash_sha256}")
        print("-" * 72)
    except Exception as e:
        print(f"⚠️ Could not profile binary: {e}")

    # 5. Khởi tạo CSV
    header = ["Iter", "Keygen_Cyc", "Keygen_ns", "Sign_Cyc", "Sign_ns", "Verify_Cyc", "Verify_ns"]
    print(f"🚀 Running {n_iterations} iterations isolated to CPU Core 0...")

    with open(OUTPUT_CSV, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)

        for i in range(n_iterations):
            # Lệnh chạy nguyên bản từ file Bash của bạn
            cmd = ["sudo", "taskset", "-c", "0", "numactl", "--physcpubind=0", "time", "-v", EXECUTABLE]
            
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, # time -v ghi log vào stderr
                text=True
            )

            stdout, stderr = process.communicate()
            
            # Gộp chung stdout và stderr để đảm bảo không bỏ sót dòng DATA
            full_output = stdout + "\n" + stderr

            # Trích xuất dòng DATA
            found_data = False
            for line in full_output.splitlines():
                if line.startswith("DATA:"):
                    # Xóa chữ "DATA:", xóa khoảng trắng thừa và tách bằng dấu phẩy
                    raw_values = line.replace("DATA:", "").replace(" ", "").strip().split(",")
                    writer.writerow([i + 1] + raw_values)
                    found_data = True
                    break

            if not found_data:
                print(f"⚠️ No DATA found in iteration {i+1}")
                if stderr:
                    print(f"Stderr: {stderr.strip()}")

            # Hiển thị tiến độ (cập nhật mỗi 10%)
            if (i + 1) % max(1, n_iterations // 10) == 0 or (i + 1) == n_iterations:
                percent = int(((i + 1) / n_iterations) * 100)
                print(f"⏳ {percent}% ({i+1}/{n_iterations})")

    print("-" * 72)
    print(f"✅ Execution Concluded: Analytical results saved to → {os.path.abspath(OUTPUT_CSV)}")

if __name__ == "__main__":
    run_benchmark()
