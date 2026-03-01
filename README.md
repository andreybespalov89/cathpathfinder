# datasetgradualstep

A pipeline for iterative protein-sequence pairing, edit-path validation, and optional structure-oriented trajectory generation.

The repository combines:
- Python orchestration scripts
- C extensions for performance-critical computations
- Local IUPred3 sources for disorder scoring
- Optional ESMFold-based structure generation with RMSD filtering
- Precomputed layer artifacts for immediate runs

## Overview

Main workflow:
1. Build nearest-neighbor sequence pairs for a FASTA dataset (`k`-mer prefilter + Levenshtein distance).
2. Build the next FASTA layer from selected indices in the pairs file.
3. Repeat layer generation until only a small number of pairs remains.
4. Optionally validate each pair transformation path with a dynamic search + validator.
5. Optionally generate midpoint trajectory sequences and run ESMFold + RMSD filtering.

Main entry points:
- `extract_pdb_sequences.py`: extract amino-acid sequences from PDB files into FASTA
- `run_layers.py`: iterative layer construction
- `run_pair_validations.py`: batch pair validation and transformed-sequence export
- `debug_max_lev.py`: debug a difficult pair
- `run_trajectory_layers.py`: trajectory midpoint generation + ESMFold + RMSD filter
- `merge_trajectory_layers.py`: merge all trajectory levels into one dataset
- `run_esmfold_batch.py`: direct batch ESMFold inference for FASTA input

## Algorithm Notes

The edit-path method in `datasetgradualstep_c` is hybrid:
- Dynamic programming computes the base edit-distance landscape.
- Greedy path reconstruction checks intermediate chains with an external validator.
- If validation fails, search switches to a best-first/A*-style recovery guided by DP-derived distance values.

This keeps exact distance computation fast while supporting constrained path search.

## Repository Structure

- `datasetgradualstep_c/`
  C extension `datasetgradualstep_c._core` with:
  - `algo_seq_dynamic_run`
  - `algo_seq_dynamic_with_validation_run`
- `cath_nn_lev_lib/`
  C extension `cath_nn_lev.cath_nn_lev` with:
  - `find_pairs`
  - `_levenshtein_bounded`
- `iupred3/`
  Local IUPred3 code and data files
- `layer_outputs/`
  Precomputed layer artifacts (`pairs_*`, `directed*`, `pdb_sequences_layer_*`)
- `deploy/`
  Debian-oriented environment/bootstrap scripts
- `tests/`
  Tests for `datasetgradualstep_c`

## Requirements

Recommended environment:
- Linux or macOS
- Python 3.10+
- C compiler toolchain (`gcc` or `clang`)
- `pip`

Python packages:
- Required for tests: `pytest`
- Required for IUPred medium smoothing: `scipy`
- Required for trajectory/ESMFold path: `torch`, `fair-esm`, `biopython`
- Optional: `jupyter`

Notes:
- `run_pair_validations.py` defaults to `--iupred-smoothing no`, so SciPy is not required in default mode.
- `run_trajectory_layers.py` and `run_esmfold_batch.py` are GPU-oriented, but can run on CPU with much lower throughput.

## Installation

From repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip setuptools wheel
```

Install local C extensions:

```bash
python -m pip install -e ./datasetgradualstep_c
python -m pip install -e ./cath_nn_lev_lib
```

Install base dependencies:

```bash
python -m pip install pytest
# Optional / feature-specific:
python -m pip install scipy biopython
```

If you plan to run ESMFold, install compatible PyTorch + `fair-esm` in the same environment.

Quick import check:

```bash
python - <<'PY'
import datasetgradualstep_c as dsc
import cath_nn_lev
from iupred3 import iupred3_lib
print('datasetgradualstep_c:', hasattr(dsc, 'algo_seq_dynamic_with_validation_run'))
print('cath_nn_lev:', hasattr(cath_nn_lev, 'find_pairs'))
print('iupred3:', hasattr(iupred3_lib, 'iupred'))
PY
```

## Running Tests

From repository root:

```bash
python -m pytest -q
```

`pytest.ini` currently sets:
- `pythonpath = .`
- `testpaths = tests`
- excludes recursive collection in `cath_nn_lev_lib`

To run tests under `cath_nn_lev_lib/tests` explicitly:

```bash
python -m pytest -q cath_nn_lev_lib/tests
```

## Input and Output Formats

### FASTA input

Expected multi-FASTA:
- Header lines start with `>`
- Sequence lines follow

### Pairs TSV output

Typical row format:

```text
id1\tid2\tidx1\tidx2\tlen1\tlen2\tlev\tmethod
```

`run_pair_validations.py` minimally requires:
- `id1`, `id2`, `idx1`, `idx2`
- `lev` is optional and used by `--max-lev`

## End-to-End Usage

### A. Extract sequences from PDB files

If PDB files are in `./dompdb`:

```bash
python extract_pdb_sequences.py --input ./dompdb --output ./pdb_sequences.fasta
```

Produces:
- FASTA records like `>filename|chain:X`
- Sequence-length statistics on stdout

### B. Build nearest-neighbor pairs (Layer 1)

```bash
python - <<'PY'
import cath_nn_lev
stats = cath_nn_lev.find_pairs(
    fasta_path='pdb_sequences.fasta',
    out_pairs_path='pairs_1_layer.tsv',
    k=12,
    M=500,
    threads=24,
    strict=0,
    write_directed_path='directed1.tsv',
)
print(stats)
PY
```

Produces:
- `pairs_1_layer.tsv`
- `directed1.tsv`

### C. Build Layer 2 FASTA from `idx1`

```bash
python make_layer2.py pairs_1_layer.tsv pdb_sequences.fasta pdb_sequences_layer_2.fasta
```

### D. Build layers iteratively

`run_layers.py` expects in current directory:
- `pairs_<n>_layer.tsv`
- `pdb_sequences_layer_<n>.fasta`

Example:

```bash
python run_layers.py --start 4 --k 12 --M 500 --threads 24 --strict 0
```

For each next layer it writes:
- `pdb_sequences_layer_<n+1>.fasta`
- `pairs_<n+1>_layer.tsv`
- `directed<n+1>.tsv`

Stop conditions:
- Next pairs file has `<= 1` non-empty line
- Or next FASTA has `< 2` sequences

### E. Validate pairs with dynamic path + validator

Default run against precomputed layers:

```bash
python run_pair_validations.py \
  --layers-dir layer_outputs \
  --out-dir validation_outputs \
  --backend processes \
  --threads 0 \
  --iupred-mode long \
  --iupred-smoothing no
```

Main outputs:
- `validation_outputs/results.tsv`
- `validation_outputs/new_sequences.fasta`

`results.tsv` columns:
- `layer, line, idx1, idx2, id1, id2, status, distance, operations_json, steps_count`

Status values:
- `found`
- `not_found`
- `timeout`
- `invalid_index`

## Trajectory Midpoint Generation (ESMFold + RMSD)

Build midpoint layers from transformation trajectories and keep structurally stable sequences:

```bash
python run_trajectory_layers.py \
  --fasta validation_outputs/new_sequences.fasta \
  --out-dir trajectory_levels \
  --gpus 0,1,2,3 \
  --num-samples 5 \
  --rmsd-threshold 2.0 \
  --skip-existing
```

Merge all levels into one dataset:

```bash
python merge_trajectory_layers.py \
  --levels-dir trajectory_levels \
  --out-dir trajectory_merged
```

Merged outputs:
- `trajectory_merged/sequences.fasta`
- `trajectory_merged/manifest.tsv`
- `trajectory_merged/pdbs/`

## Direct ESMFold Batch Inference

```bash
python run_esmfold_batch.py \
  --fasta validation_outputs/new_sequences.fasta \
  --out-dir esmfold_outputs \
  --gpus 0,1 \
  --batch-size 1 \
  --skip-existing
```

Use `ESMFOLD_CPU=1` to force CPU mode.

## Key Runtime Options

### `run_pair_validations.py`

Performance and concurrency:
- `--backend {threads,processes}` (default: `processes`)
- `--threads 0` uses CPU count
- `--progress-interval` controls progress logging frequency

Validation behavior:
- `--no-validator` disables chain validation (fastest mode)
- `--validator-prob` controls random acceptance in custom validator path
- `--max-lev` skips pairs above Levenshtein threshold
- `--pair-timeout` sets per-pair timeout in seconds

IUPred thresholds:
- `--disorder-threshold`
- `--max-disordered-fraction`
- `--max-disordered-run`
- `--max-mean-score`

### `datasetgradualstep_c` environment variables

`algo_seq_dynamic_with_validation_run` uses:
- `DSC_VERBOSE=1`
- `DSC_LOG_EVERY=<int>`
- `DSC_LOG_FAILS=<int>`
- `DSC_MAX_NODES=<int>`
- `DSC_BEAM_SIZE=<int>`
- `DSC_MAX_UNVALIDATED_STEPS=<int>`
- `DSC_MAX_SECONDS=<float>`

Example:

```bash
DSC_VERBOSE=1 DSC_MAX_SECONDS=30 python debug_max_lev.py --max-lev 15
```

## Deployment Helpers (Debian)

The `deploy/` folder contains scripts to provision Debian 12/13 environments (system deps, Mambaforge, env creation, and project install).

Typical flow:

```bash
sudo bash deploy/00-system-deps-debian.sh
bash deploy/01-install-mambaforge.sh
bash deploy/02-create-envs.sh
bash deploy/03-install-project.sh
```

Then activate:

```bash
source deploy/activate_ssg.sh
source deploy/activate_esmfold.sh
```

## Troubleshooting

### `ModuleNotFoundError` for `datasetgradualstep_c` or `cath_nn_lev`

```bash
python -m pip install -e ./datasetgradualstep_c
python -m pip install -e ./cath_nn_lev_lib
```

### `No module named pytest`

```bash
python -m pip install pytest
```

### SciPy-related IUPred error

If using medium smoothing:

```bash
python -m pip install scipy
```

Or run with:

```bash
--iupred-smoothing no
```

### C extension build failures

Verify toolchain:
- Linux: install build essentials (`gcc`, headers)
- macOS: install command-line tools (`xcode-select --install`)

Then reinstall local extensions.

### ESMFold weights/model issues

- Ensure compatible `torch` and `fair-esm` versions.
- Set `ESMFOLD_WEIGHTS` if auto-download/loading fails.
- Ensure GPU visibility (`CUDA_VISIBLE_DEVICES`) or force CPU with `ESMFOLD_CPU=1`.

### Long runtime / high CPU usage

- Decrease `M`
- Reduce `--threads`
- Use `--max-lev`
- Use `--pair-timeout`
- Set C-level guards (`DSC_MAX_NODES`, `DSC_MAX_SECONDS`)

## Minimal API Reference

### `datasetgradualstep_c`

```python
import datasetgradualstep_c as dsc

steps, ops, dist = dsc.algo_seq_dynamic_run(a_seq, b_seq)
steps_v, ops_v, dist_v = dsc.algo_seq_dynamic_with_validation_run(
    a_seq,
    b_seq,
    validator=my_callable_or_none,
)
```

Returns:
- `steps`: list of intermediate sequences
- `ops`: list of operation tuples
- `dist`: weighted operation count

### `cath_nn_lev`

```python
import cath_nn_lev

stats = cath_nn_lev.find_pairs(
    fasta_path,
    out_pairs_path,
    k=4,
    M=500,
    df_max=None,
    threads=0,
    strict=0,
    write_directed_path=None,
)
```

Returns a dictionary with dataset, index, and timing metrics.

## Reproducibility

- Use `--seed` in `run_pair_validations.py` for deterministic random-validator behavior.
- Keep command logs and output directories for repeatable runs.
- Python/compiler and CUDA stack versions can affect runtime/performance.

## Licensing

- `cath_nn_lev_lib` includes an MIT `LICENSE`.
- `iupred3` includes its own `LICENSE`.

If you redistribute code or publish results, verify obligations for each bundled component.
