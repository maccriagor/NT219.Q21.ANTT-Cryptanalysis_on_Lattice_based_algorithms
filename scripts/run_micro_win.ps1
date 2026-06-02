# run_micro_win.ps1 — Build + chạy microbenchmark bằng MinGW + OpenSSL 3.6.2 trên Windows.
# Dùng:
#   powershell -ExecutionPolicy Bypass -File scripts\run_micro_win.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\run_micro_win.ps1 --suite kem --level 5 --iters 2000
$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)

$GPP  = "C:\msys64\mingw64\bin\g++.exe"   # MinGW g++
$OSSL = "F:\cryptolibrary"                # OpenSSL 3.6.2 cho MinGW (đổi nếu máy khác)

if (-not (Test-Path $GPP))  { throw "Khong thay g++: $GPP" }
if (-not (Test-Path "$OSSL\include\openssl\evp.h")) { throw "Khong thay OpenSSL: $OSSL\include" }

New-Item -ItemType Directory -Force -Path "data\results" | Out-Null

Write-Host ">>> Compile (MinGW g++ + OpenSSL 3.6.2)..."
& $GPP -O2 -std=c++17 "benchmarks\micro\pqc_bench.cpp" -o "benchmarks\micro\pqc_bench.exe" `
    "-I$OSSL\include" "-L$OSSL\lib64" "-l:libssl.a" "-l:libcrypto.a" -lws2_32 -lcrypt32
if ($LASTEXITCODE -ne 0) { throw "Compile failed" }

$benchArgs = if ($args.Count -gt 0) { $args } else { @("--level","all","--iters","1000","--warmup","100","--batches","10") }
Write-Host ">>> Run: $($benchArgs -join ' ')"
& ".\benchmarks\micro\pqc_bench.exe" @benchArgs --csv "data\results\micro_win.csv"
Write-Host ">>> CSV: data\results\micro_win.csv"
Get-Content "data\results\micro_win.csv"
