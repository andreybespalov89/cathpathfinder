# datasetgradualstep_c

C implementation of algorithms from `datasetgradualstep` as a Python extension module.

## Build (in-place)

```bash
python -m pip install -e .
```

Or:

```bash
python setup.py build_ext --inplace
```

## Usage

```python
import datasetgradualstep_c as dsc

steps, ops, dist = dsc.algo_seq_dynamic_run("kitten", "sitting")

steps_v, ops_v, dist_v = dsc.algo_seq_dynamic_with_validation_run(
    "kitten",
    "sitting",
    validator=lambda s: True,
)
```
