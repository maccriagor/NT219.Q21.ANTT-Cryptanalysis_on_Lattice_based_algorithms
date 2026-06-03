# CMakeLists.txt — dùng Crypto++ qua abdes/cryptopp-cmake (CMake >= 3.21) bằng FetchContent.
# Build native:   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
# Cross aarch64:  cmake -S . -B build-arm64 -DCMAKE_BUILD_TYPE=Release \
#                   -DCMAKE_TOOLCHAIN_FILE=$PWD/aarch64-linux-gnu.toolchain.cmake && cmake --build build-arm64 -j
cmake_minimum_required(VERSION 3.21)
project(cryptopp_bench CXX)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(cryptopp-cmake
  GIT_REPOSITORY https://github.com/abdes/cryptopp-cmake.git
  GIT_TAG        master)          # pin 1 tag/commit để tái lập (xem Releases)
set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cryptopp-cmake)

# Tối ưu (tùy chọn): bỏ comment để bật -O3 / -march=native cho CHÍNH máy build (không portable)
# add_compile_options(-O3)
# add_compile_options(-march=native)

add_executable(cryptopp_bench bench.cpp)
target_link_libraries(cryptopp_bench PRIVATE cryptopp::cryptopp)
