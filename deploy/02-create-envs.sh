#!/usr/bin/env bash
set -euo pipefail

MAMBA_PREFIX=${MAMBA_PREFIX:-"$HOME/mambaforge"}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ESMFOLD_TORCH_VERSION=${ESMFOLD_TORCH_VERSION:-"2.1.2"}
ESMFOLD_TORCHVISION_VERSION=${ESMFOLD_TORCHVISION_VERSION:-"0.16.2"}
ESMFOLD_TORCHAUDIO_VERSION=${ESMFOLD_TORCHAUDIO_VERSION:-"2.1.2"}
ESMFOLD_FAIR_ESM_VERSION=${ESMFOLD_FAIR_ESM_VERSION:-"2.0.0"}

if [[ ! -f "$MAMBA_PREFIX/etc/profile.d/conda.sh" ]]; then
  echo "conda.sh not found. Install Mambaforge first (deploy/01-install-mambaforge.sh)." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$MAMBA_PREFIX/etc/profile.d/conda.sh"

mamba env create -f "$ROOT_DIR/deploy/env_ssg.yml"

mamba env create -f "$ROOT_DIR/deploy/env_esmfold.yml"

if [[ "${ESMFOLD_CUDA:-0}" == "1" ]]; then
  CUDA_VERSION=${CUDA_VERSION:-"12.1"}
  mamba install -n esmfold -c pytorch -c nvidia \
    "pytorch=${ESMFOLD_TORCH_VERSION}" \
    "torchvision=${ESMFOLD_TORCHVISION_VERSION}" \
    "torchaudio=${ESMFOLD_TORCHAUDIO_VERSION}" \
    "pytorch-cuda=${CUDA_VERSION}"
else
  mamba install -n esmfold -c pytorch \
    "pytorch=${ESMFOLD_TORCH_VERSION}" \
    "torchvision=${ESMFOLD_TORCHVISION_VERSION}" \
    "torchaudio=${ESMFOLD_TORCHAUDIO_VERSION}"
fi

conda run -n esmfold python -m pip install --upgrade pip
conda run -n esmfold python -m pip install "fair-esm==${ESMFOLD_FAIR_ESM_VERSION}"

conda run -n ssg python -m pip install --upgrade pip setuptools wheel

cat <<EOM
Environments created:
  - ssg
  - esmfold
EOM
