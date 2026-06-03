# ============================================================================
# cmake/toolchain-aarch64.cmake
# Toolchain file để CROSS-COMPILE từ x86_64 (PC Ubuntu) sang arm64 (Pi 4).
# CMake ĐỌC file này (không "chạy" nó) khi bạn truyền -DCMAKE_TOOLCHAIN_FILE=...
# LƯU Ý: output là ARTIFACT arm64, KHÔNG dùng để đo. Số NEON phải build native trên Pi.
# ============================================================================
 
# Khai báo đang build cho hệ khác (cross-compile): target = Linux/aarch64
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)            # liboqs nhận diện aarch64|arm64|arm64v8
 
# Cross-compiler đã cài qua: apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
 
# Tìm chương trình ở HỆ THỐNG host (vd cmake/ninja), nhưng thư viện/header thì
# CHỈ tìm trong sysroot của target (tránh lỡ link nhầm lib x86 của máy host).
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
 
# (TÙY CHỌN) nhắm đúng Cortex-A72 của Pi 4. Bỏ comment nếu artifact CHỈ cho Pi 4.
# set(CMAKE_C_FLAGS   "-mcpu=cortex-a72")
# set(CMAKE_CXX_FLAGS "-mcpu=cortex-a72")