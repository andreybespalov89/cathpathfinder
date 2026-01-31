#!/usr/bin/env python3
"""Extract amino-acid sequences from PDB files in ./dompdb and write FASTA + length stats."""
from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
from statistics import mean, median, pstdev
from typing import Dict, List, Tuple

AA3_TO_1 = {
    "ALA": "A", "ARG": "R", "ASN": "N", "ASP": "D", "CYS": "C",
    "GLU": "E", "GLN": "Q", "GLY": "G", "HIS": "H", "ILE": "I",
    "LEU": "L", "LYS": "K", "MET": "M", "PHE": "F", "PRO": "P",
    "SER": "S", "THR": "T", "TRP": "W", "TYR": "Y", "VAL": "V",
    # Common ambiguous/modified residues
    "ASX": "B", "GLX": "Z", "SEC": "U", "PYL": "O", "UNK": "X",
    "MSE": "M",  # Selenomethionine -> Methionine
}


def parse_seqres(lines: List[str]) -> Dict[str, str]:
    seqs: Dict[str, List[str]] = {}
    for line in lines:
        if not line.startswith("SEQRES"):
            continue
        chain = line[11].strip() or "_"
        residues = line[19:70].split()
        seqs.setdefault(chain, [])
        for res in residues:
            seqs[chain].append(AA3_TO_1.get(res.upper(), "X"))
    return {chain: "".join(seq) for chain, seq in seqs.items()}


def parse_atom(lines: List[str]) -> Dict[str, str]:
    # Build sequence by residue order per chain
    seqs: Dict[str, List[str]] = {}
    seen: Dict[Tuple[str, str, str], None] = {}
    for line in lines:
        if not (line.startswith("ATOM") or line.startswith("HETATM")):
            continue
        resname = line[17:20].strip()
        chain = line[21].strip() or "_"
        resseq = line[22:26].strip()
        icode = line[26].strip()
        key = (chain, resseq, icode)
        if key in seen:
            continue
        seen[key] = None
        seqs.setdefault(chain, [])
        seqs[chain].append(AA3_TO_1.get(resname.upper(), "X"))
    return {chain: "".join(seq) for chain, seq in seqs.items()}


def percentile(values: List[int], pct: float) -> float:
    if not values:
        return float("nan")
    if pct <= 0:
        return float(min(values))
    if pct >= 100:
        return float(max(values))
    xs = sorted(values)
    k = (len(xs) - 1) * (pct / 100.0)
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return float(xs[int(k)])
    d0 = xs[int(f)] * (c - k)
    d1 = xs[int(c)] * (k - f)
    return float(d0 + d1)


def extract_sequences(pdb_path: Path) -> Dict[str, str]:
    lines = pdb_path.read_text(errors="ignore").splitlines()
    seqres = parse_seqres(lines)
    if seqres:
        return seqres
    return parse_atom(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract amino-acid sequences from PDB files.")
    parser.add_argument("--input", default="./dompdb", help="Directory with PDB files")
    parser.add_argument("--output", default="./pdb_sequences.fasta", help="Output FASTA file")
    args = parser.parse_args()

    input_dir = Path(args.input)
    if not input_dir.is_dir():
        raise SystemExit(f"Input directory not found: {input_dir}")

    pdb_files = sorted([p for p in input_dir.iterdir() if p.is_file()])
    all_seqs: List[Tuple[str, str]] = []

    for pdb_file in pdb_files:
        seqs = extract_sequences(pdb_file)
        if not seqs:
            continue
        for chain, seq in seqs.items():
            if not seq:
                continue
            header = f">{pdb_file.stem}|chain:{chain}"
            all_seqs.append((header, seq))

    if not all_seqs:
        raise SystemExit("No sequences found.")

    out_path = Path(args.output)
    with out_path.open("w") as f:
        for header, seq in all_seqs:
            f.write(header + "\n")
            f.write(seq + "\n")

    lengths = [len(seq) for _, seq in all_seqs]
    stats = {
        "count": len(lengths),
        "min": min(lengths),
        "max": max(lengths),
        "mean": mean(lengths),
        "median": median(lengths),
        "std": pstdev(lengths) if len(lengths) > 1 else 0.0,
        "p90": percentile(lengths, 90),
        "p80": percentile(lengths, 80),
        "p70": percentile(lengths, 70),
    }

    print(f"Wrote {len(all_seqs)} sequences to {out_path}")
    print("Length statistics:")
    print(f"  count: {stats['count']}")
    print(f"  min: {stats['min']}")
    print(f"  max: {stats['max']}")
    print(f"  mean: {stats['mean']:.2f}")
    print(f"  median: {stats['median']}")
    print(f"  std: {stats['std']:.2f}")
    print(f"  p90: {stats['p90']:.2f}")
    print(f"  p80: {stats['p80']:.2f}")
    print(f"  p70: {stats['p70']:.2f}")


if __name__ == "__main__":
    main()
