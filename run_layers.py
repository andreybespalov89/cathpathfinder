#!/usr/bin/env python3
import argparse
import pathlib
import sys

import cath_nn_lev

from make_layer2 import read_idx1_set, filter_fasta_by_indices


def count_pairs(pairs_path: pathlib.Path) -> int:
    count = 0
    with pairs_path.open("r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                count += 1
    return count


def count_fasta_records(fasta_path: pathlib.Path) -> int:
    count = 0
    with fasta_path.open("r", encoding="utf-8") as f:
        for line in f:
            if line.startswith(">"):
                count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description="Iteratively build layers until pairs file has a single pair.")
    parser.add_argument("--start", type=int, default=4, help="Current layer n (expects pairs_n_layer.tsv and pdb_sequences_layer_n.fasta).")
    parser.add_argument("--k", type=int, default=12)
    parser.add_argument("--M", type=int, default=500)
    parser.add_argument("--threads", type=int, default=24)
    parser.add_argument("--strict", type=int, default=0)
    args = parser.parse_args()

    n = args.start

    while True:
        pairs_in = pathlib.Path(f"pairs_{n}_layer.tsv")
        fasta_in = pathlib.Path(f"pdb_sequences_layer_{n}.fasta")
        if not pairs_in.exists() or not fasta_in.exists():
            print(f"[run_layers] missing {pairs_in} or {fasta_in}")
            return 1

        next_n = n + 1
        fasta_out = pathlib.Path(f"pdb_sequences_layer_{next_n}.fasta")
        pairs_out = pathlib.Path(f"pairs_{next_n}_layer.tsv")
        directed_out = pathlib.Path(f"directed{next_n}.tsv")

        print(f"[run_layers] building layer {next_n} from {pairs_in} -> {fasta_out}")
        idxs = read_idx1_set(str(pairs_in))
        filter_fasta_by_indices(str(fasta_in), str(fasta_out), idxs)

        n_records = count_fasta_records(fasta_out)
        print(f"[run_layers] layer {next_n} fasta records: {n_records}")
        if n_records < 2:
            print(f"[run_layers] stop: less than 2 sequences at layer {next_n}")
            return 0

        print("[run_layers] запуск алгоритма")
        stats = cath_nn_lev.find_pairs(
            str(fasta_out),
            str(pairs_out),
            k=args.k,
            M=args.M,
            threads=args.threads,
            strict=args.strict,
            write_directed_path=str(directed_out),
        )
        print(stats)

        n_pairs = count_pairs(pairs_out)
        print(f"[run_layers] pairs in layer {next_n}: {n_pairs}")
        if n_pairs <= 1:
            print(f"[run_layers] stop: pairs_{next_n}_layer.tsv has {n_pairs} line(s)")
            return 0

        n = next_n


if __name__ == "__main__":
    raise SystemExit(main())
