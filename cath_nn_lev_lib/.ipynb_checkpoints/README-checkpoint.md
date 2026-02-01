# cath_nn_lev

C extension for nearest-neighbor search over polyfasta sequences using a k-mer prefilter (top-M) and Levenshtein distance.

## Python usage

```python
import cath_nn_lev

stats = cath_nn_lev.find_pairs(
    "cath_s40.fasta",
    "pairs.tsv",
    k=4,
    M=500,
    threads=16,
    strict=0,
    write_directed_path="directed.tsv",
)
print(stats)
```

## Build

```bash
python -m pip install -e .
```
