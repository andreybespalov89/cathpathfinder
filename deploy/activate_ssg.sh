#!/usr/bin/env bash
set -euo pipefail

MAMBA_PREFIX=${MAMBA_PREFIX:-"$HOME/mambaforge"}

# shellcheck disable=SC1091
source "$MAMBA_PREFIX/etc/profile.d/conda.sh"
conda activate ssg
