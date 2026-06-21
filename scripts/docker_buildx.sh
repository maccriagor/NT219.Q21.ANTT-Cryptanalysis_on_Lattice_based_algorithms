#!/usr/bin/env bash
# =============================================================================
# docker_buildx.sh - Build the reproducible image for BOTH amd64 and arm64
# with docker buildx (brief 7.3: "docker buildx multiarch").
# Docs: https://docs.docker.com/build/building/multi-platform/
# Note: cross-arch emulation may need: docker run --privileged --rm tonistiigi/binfmt --install all
# =============================================================================
# DESIGN NOTES
#   - buildx builds ONE Dockerfile for MANY platforms; the arm64 pass runs
#     inside QEMU emulation (docs.docker.com/build/building/multi-platform).
#   - One-time prerequisite: docker run --privileged --rm tonistiigi/binfmt
#     --install all   (teaches the host to execute foreign binaries).
#   - Role = reproducibility evidence (brief 7.3 "docker buildx multiarch").
#     NEVER benchmark inside QEMU: numbers measure the emulator, not a CPU.
#   - Known caveat: --load may reject multi-platform manifests -> enable the
#     containerd image store or build one --platform at a time.
set -eu
docker buildx create --name nt219builder --use 2>/dev/null || docker buildx use nt219builder
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -f docker/Dockerfile.x86_64 \
  -t nt219-pqc:multi \
  --load .
