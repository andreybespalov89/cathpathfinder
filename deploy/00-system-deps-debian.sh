#!/usr/bin/env bash
set -euo pipefail

if [[ $(id -u) -ne 0 ]]; then
  echo "This script must be run as root (use sudo)." >&2
  exit 1
fi

if [[ -r /etc/os-release ]]; then
  . /etc/os-release
else
  echo "/etc/os-release not found; cannot verify OS." >&2
  exit 1
fi

if [[ "${ID:-}" != "debian" ]]; then
  echo "Unsupported OS: ${ID:-unknown}. Expected debian." >&2
  exit 1
fi

case "${VERSION_ID:-}" in
  12|13) : ;;
  *)
    echo "Unsupported Debian version: ${VERSION_ID:-unknown}. Expected 12 or 13." >&2
    exit 1
    ;;
 esac

export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  curl \
  git \
  bzip2 \
  xz-utils \
  pkg-config

apt-get clean
rm -rf /var/lib/apt/lists/*

echo "System dependencies installed."
