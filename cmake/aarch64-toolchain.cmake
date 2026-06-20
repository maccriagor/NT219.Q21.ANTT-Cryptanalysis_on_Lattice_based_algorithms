# =============================================================================
# aarch64-toolchain.cmake - CMake cross toolchain: x86_64 host -> aarch64 target
# Per liboqs wiki "Cross-compiling on Linux for ARM": supply CMake an
# appropriate toolchain file (CMake cross-compiling convention).
# Requires: apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
# =============================================================================
# DESIGN NOTES (cmake-toolchains(7))
#   - Mirrors the example liboqs SHIPS (.CMake/toolchain_rasppi.cmake):
#     theirs targets 32-bit armhf/gcc-8 Raspberry Pi OS; ours is the modern
#     aarch64 analogue (Pi 4 64-bit / Ampere) -> SYSTEM_PROCESSOR aarch64,
#     aarch64-linux-gnu-gcc, and no Pi-specific defines.
#   - FIND_ROOT_PATH_MODE quartet (PROGRAM=NEVER, LIBRARY/INCLUDE/PACKAGE=
#     ONLY) keeps host tools usable while never linking host libraries;
#     the PACKAGE line matches the upstream example one-for-one.
#   - Never run directly: consumed via -DCMAKE_TOOLCHAIN_FILE by
#     cross_build_liboqs.sh.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
# Search libs/headers only in the target environment, programs on the host.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
