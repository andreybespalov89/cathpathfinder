#!/usr/bin/env bash
set -euo pipefail

MAMBA_PREFIX=${MAMBA_PREFIX:-"$HOME/mambaforge"}
INSTALLER_URL=${INSTALLER_URL:-"https://github.com/conda-forge/miniforge/releases/latest/download/Mambaforge-Linux-x86_64.sh"}

if [[ -x "$MAMBA_PREFIX/bin/mamba" ]]; then
  echo "Mambaforge already installed at $MAMBA_PREFIX"
  exit 0
fi

mkdir -p "$MAMBA_PREFIX"

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

INSTALLER="$TMP_DIR/Mambaforge-Linux-x86_64.sh"

curl -fsSL "$INSTALLER_URL" -o "$INSTALLER"

bash "$INSTALLER" -b -p "$MAMBA_PREFIX"

"$MAMBA_PREFIX/bin/conda" config --set auto_activate_base false

cat <<EOM
Mambaforge installed to: $MAMBA_PREFIX
Add this to your shell (or run in scripts):
  source "$MAMBA_PREFIX/etc/profile.d/conda.sh"
EOM
