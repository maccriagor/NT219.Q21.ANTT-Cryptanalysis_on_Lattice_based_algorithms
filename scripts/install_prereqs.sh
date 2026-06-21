#!/bin/bash
# =============================================================================
# Usage:
#   bash ./scripts/install_prereqs.sh
# =============================================================================

set -e
export DEBIAN_FRONTEND=noninteractive

# Docker images run as root and often have no sudo; only prefix it when not root.
SUDO=sudo
if [ "$(id -u)" = 0 ]; then SUDO=; fi

# a broken third-party PPA shouldn't kill the whole run
$SUDO apt-get update || echo "apt-get update had errors (broken PPA?), continuing"

# toolchain to build OpenSSL, liboqs, oqs-provider and the harnesses
$SUDO apt-get install -y \
  build-essential \
  git \
  perl \
  pkg-config \
  cmake \
  ninja-build \
  ca-certificates

# measurement: /usr/bin/time for peak RSS, tshark for bytes-on-wire
echo "wireshark-common wireshark-common/install-setuid boolean true" | $SUDO debconf-set-selections
$SUDO apt-get install -y \
  time \
  tshark

# wrk drives the TLS handshake load (Ubuntu universe; source build on Debian/Pi)
$SUDO apt-get install -y wrk || echo "wrk not packaged here - build from github.com/wg/wrk"

# analysis + plots
$SUDO apt-get install -y \
  python3 \
  python3-pandas \
  python3-matplotlib

# add the invoking user to the wireshark group (skipped for root / Docker / CI)
user="${SUDO_USER:-$USER}"
if [ -n "$user" ] && [ "$user" != root ]; then
  $SUDO usermod -aG wireshark "$user"
fi

echo "Done - log out and back in before capturing with tshark."