# aarch64-linux-gnu.toolchain.cmake — CMake toolchain cross-compile ARM64.
# Cài: sudo apt-get install -y g++-aarch64-linux-gnu gcc-aarch64-linux-gnu
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# NEON sẵn ở ARMv8; thêm '+crypto' nếu CPU đích có ARMv8 AES (Pi4/BCM2711 KHÔNG có)
set(CMAKE_C_FLAGS_INIT   "-O2 -march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-O2 -march=armv8-a")

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
