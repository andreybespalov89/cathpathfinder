#!/usr/bin/env bash
set -euo pipefail

FASTA=${ESMFOLD_FASTA:-/app/validation_outputs/new_sequences.fasta}
OUT_DIR=${ESMFOLD_OUT_DIR:-/app/esmfold_outputs}
GPUS=${ESMFOLD_GPUS:-0}
BATCH_SIZE=${ESMFOLD_BATCH_SIZE:-1}
CHUNK_SIZE=${ESMFOLD_CHUNK_SIZE:-0}
SKIP_EXISTING=${ESMFOLD_SKIP_EXISTING:-0}

if [[ ! -f "$FASTA" ]]; then
  echo "Input FASTA not found: $FASTA" >&2
  exit 1
fi

source /opt/mambaforge/etc/profile.d/conda.sh
conda activate esmfold

args=(--fasta "$FASTA" --out-dir "$OUT_DIR" --gpus "$GPUS" --batch-size "$BATCH_SIZE" --chunk-size "$CHUNK_SIZE")
if [[ "$SKIP_EXISTING" == "1" ]]; then
  args+=(--skip-existing)
fi

python /app/run_esmfold_batch.py "${args[@]}"
