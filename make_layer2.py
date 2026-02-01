#!/usr/bin/env python3
import sys


def read_idx1_set(pairs_path: str) -> set[int]:
    idxs: set[int] = set()
    total_lines = 0
    with open(pairs_path, "r", encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            cols = line.rstrip("\n").split("\t")
            if len(cols) < 4:
                continue
            idxs.add(int(cols[2]))
            total_lines += 1
    print(f"[make_layer2] pairs lines read: {total_lines}")
    print(f"[make_layer2] unique idx1: {len(idxs)}")
    return idxs


def filter_fasta_by_indices(fasta_in: str, fasta_out: str, keep: set[int]) -> None:
    out = open(fasta_out, "w", encoding="utf-8")
    try:
        cur_idx = -1
        write_block = False
        written = 0
        with open(fasta_in, "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith(">"):
                    cur_idx += 1
                    write_block = cur_idx in keep
                    if write_block:
                        written += 1
                if write_block:
                    out.write(line)
        print(f"[make_layer2] fasta records written: {written}")
    finally:
        out.close()


def main() -> int:
    if len(sys.argv) != 4:
        print("Usage: python make_layer2.py pairs_1_layer.tsv pdb_sequences.fasta pdb_sequences_layer_2.fasta")
        return 1
    pairs_path, fasta_in, fasta_out = sys.argv[1], sys.argv[2], sys.argv[3]
    idxs = read_idx1_set(pairs_path)
    filter_fasta_by_indices(fasta_in, fasta_out, idxs)
    print("[make_layer2] done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
