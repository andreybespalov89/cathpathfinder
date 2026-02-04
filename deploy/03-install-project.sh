#!/usr/bin/env bash
set -euo pipefail

MAMBA_PREFIX=${MAMBA_PREFIX:-"$HOME/mambaforge"}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

if [[ ! -f "$MAMBA_PREFIX/etc/profile.d/conda.sh" ]]; then
  echo "conda.sh not found. Install Mambaforge first (deploy/01-install-mambaforge.sh)." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$MAMBA_PREFIX/etc/profile.d/conda.sh"

conda activate ssg

python -m pip install -e "$ROOT_DIR/datasetgradualstep_c"
python -m pip install -e "$ROOT_DIR/cath_nn_lev_lib"

cat <<EOM
Project packages installed into the ssg environment.
Note: run scripts from the repo root so Python can import the local iupred3 package.
EOM
