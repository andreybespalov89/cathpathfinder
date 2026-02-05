#!/usr/bin/env bash
set -euo pipefail

MAMBA_PREFIX=${MAMBA_PREFIX:-"$HOME/mambaforge"}
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ESMFOLD_TORCH_VERSION=${ESMFOLD_TORCH_VERSION:-"2.1.2"}
ESMFOLD_TORCHVISION_VERSION=${ESMFOLD_TORCHVISION_VERSION:-"0.16.2"}
ESMFOLD_TORCHAUDIO_VERSION=${ESMFOLD_TORCHAUDIO_VERSION:-"2.1.2"}
ESMFOLD_FAIR_ESM_VERSION=${ESMFOLD_FAIR_ESM_VERSION:-"2.0.0"}
ESMFOLD_OPENFOLD_REF=${ESMFOLD_OPENFOLD_REF:-""}
export MAMBA_NO_PROMPT=1

if [[ ! -f "$MAMBA_PREFIX/etc/profile.d/conda.sh" ]]; then
  echo "conda.sh not found. Install Mambaforge first (deploy/01-install-mambaforge.sh)." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$MAMBA_PREFIX/etc/profile.d/conda.sh"

mamba env create -f "$ROOT_DIR/deploy/env_ssg.yml" -y

mamba env create -f "$ROOT_DIR/deploy/env_esmfold.yml" -y

conda run -n esmfold python -m pip install --upgrade pip
if [[ "${ESMFOLD_CUDA:-0}" == "1" ]]; then
  CUDA_VERSION=${CUDA_VERSION:-"12.1"}
  conda run -n esmfold python -m pip install \
    --index-url "https://download.pytorch.org/whl/cu${CUDA_VERSION//./}" \
    "torch==${ESMFOLD_TORCH_VERSION}+cu${CUDA_VERSION//./}" \
    "torchvision==${ESMFOLD_TORCHVISION_VERSION}+cu${CUDA_VERSION//./}" \
    "torchaudio==${ESMFOLD_TORCHAUDIO_VERSION}+cu${CUDA_VERSION//./}"
else
  conda run -n esmfold python -m pip install \
    "torch==${ESMFOLD_TORCH_VERSION}" \
    "torchvision==${ESMFOLD_TORCHVISION_VERSION}" \
    "torchaudio==${ESMFOLD_TORCHAUDIO_VERSION}"
fi
conda run -n esmfold python -m pip install "numpy<2"
conda run -n esmfold python -m pip install "fair-esm==${ESMFOLD_FAIR_ESM_VERSION}"
conda run -n esmfold python -m pip install omegaconf dm-tree biopython modelcif einops ml-collections scipy
conda run -n esmfold python -m pip install \
  "dllogger @ git+https://github.com/NVIDIA/dllogger.git"
if [[ -n "${ESMFOLD_OPENFOLD_REF}" ]]; then
  conda run -n esmfold python -m pip install --no-build-isolation \
    "openfold @ git+https://github.com/aqlaboratory/openfold.git@${ESMFOLD_OPENFOLD_REF}"
else
  conda run -n esmfold python -m pip install --no-build-isolation \
    "openfold @ git+https://github.com/aqlaboratory/openfold.git"
fi

conda run -n ssg python -m pip install --upgrade pip setuptools wheel

cat <<EOM
Environments created:
  - ssg
  - esmfold
EOM
