#!/usr/bin/env python3
import argparse
import os
import shutil
from pathlib import Path
from typing import Dict, Set, Tuple


def read_fasta(path: Path) -> Dict[str, str]:
    records: Dict[str, str] = {}
    cur_id = None
    cur_seq = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith(">"):
                if cur_id is not None:
                    records[cur_id] = "".join(cur_seq)
                cur_id = line[1:].strip()
                cur_seq = []
            else:
                cur_seq.append(line)
        if cur_id is not None:
            records[cur_id] = "".join(cur_seq)
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description="Merge trajectory level folders into a single dataset.")
    parser.add_argument("--levels-dir", default="trajectory_levels", help="Directory with level_* folders")
    parser.add_argument("--out-dir", default="trajectory_merged", help="Output directory")
    args = parser.parse_args()

    levels_dir = Path(args.levels_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    out_pdb = out_dir / "pdbs"
    out_pdb.mkdir(parents=True, exist_ok=True)

    merged_fasta_path = out_dir / "sequences.fasta"
    merged_manifest_path = out_dir / "manifest.tsv"

    all_records: Dict[str, str] = {}
    kept: Set[str] = set()

    for level_path in sorted(levels_dir.glob("level_*")):
        fasta_path = level_path / "sequences.fasta"
        if not fasta_path.exists():
            continue
        records = read_fasta(fasta_path)
        kept_path = level_path / "kept.tsv"
        if kept_path.exists():
            with kept_path.open("r", encoding="utf-8") as f:
                for line in f:
                    if line.strip():
                        kept.add(line.split("\t")[0])
        all_records.update(records)

        pdb_dir = level_path / "pdbs"
        if pdb_dir.exists():
            for pdb_file in pdb_dir.glob("*.pdb"):
                shutil.copy2(pdb_file, out_pdb / pdb_file.name)

    with merged_fasta_path.open("w", encoding="utf-8") as fasta_f, merged_manifest_path.open("w", encoding="utf-8") as man_f:
        man_f.write("seq_id\tkept\n")
        for seq_id, seq in all_records.items():
            is_kept = (seq_id in kept) if kept else True
            if is_kept:
                fasta_f.write(f">{seq_id}\n{seq}\n")
            man_f.write(f"{seq_id}\t{1 if is_kept else 0}\n")

    print(f"[merge_trajectory_layers] done. Output: {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
