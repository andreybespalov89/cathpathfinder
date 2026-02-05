#!/usr/bin/env bash
set -euo pipefail

MAMBA_PREFIX=${MAMBA_PREFIX:-"$HOME/mambaforge"}
# Mambaforge installers were removed from latest releases; use Miniforge3 instead.
INSTALLER_URL=${INSTALLER_URL:-"https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh"}

if [[ -x "$MAMBA_PREFIX/bin/mamba" ]]; then
  echo "Mambaforge already installed at $MAMBA_PREFIX"
  exit 0
fi

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

INSTALLER="$TMP_DIR/Miniforge3-Linux-x86_64.sh"

curl -fsSL "$INSTALLER_URL" -o "$INSTALLER"

INSTALL_ARGS=(-b -p "$MAMBA_PREFIX")
if [[ -d "$MAMBA_PREFIX" ]]; then
  INSTALL_ARGS+=(-u)
fi

bash "$INSTALLER" "${INSTALL_ARGS[@]}"

"$MAMBA_PREFIX/bin/conda" config --set auto_activate_base false

cat <<EOM
Mambaforge installed to: $MAMBA_PREFIX
Add this to your shell (or run in scripts):
  source "$MAMBA_PREFIX/etc/profile.d/conda.sh"
EOM
