#!/usr/bin/env python3
import argparse
import os
import random

import datasetgradualstep_c as dsc
from iupred3 import iupred3_lib as iupred3


def read_fasta(path: str):
    seqs = []
    with open(path, "r", encoding="utf-8") as f:
        cur = None
        for line in f:
            if line.startswith(">"):
                if cur is not None:
                    seqs.append(cur)
                cur = ""
            else:
                cur += line.strip()
        if cur is not None:
            seqs.append(cur)
    return seqs


def find_max_lev_pair(pairs_path: str, max_lev: int):
    best = None
    with open(pairs_path, "r", encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            cols = line.rstrip("\n").split("\t")
            if len(cols) < 7:
                continue
            i1 = int(cols[2])
            i2 = int(cols[3])
            lev = int(cols[6])
            if max_lev > 0 and lev > max_lev:
                continue
            if best is None or lev > best[2]:
                best = (i1, i2, lev)
    if best is None:
        raise RuntimeError("pairs file is empty")
    return best


def iupred_validator_factory(mode: str, smoothing: str, disorder_thr: float, max_frac: float, max_run: int, max_mean: float):
    cache = {}

    def metrics(seq: str):
        if seq in cache:
            return cache[seq]
        scores, _ = iupred3.iupred(seq, mode=mode, smoothing=smoothing)
        if not scores:
            res = (0.0, 0.0, 0)
            cache[seq] = res
            return res
        total = len(scores)
        dis = 0
        run = 0
        maxr = 0
        for v in scores:
            if v >= disorder_thr:
                dis += 1
                run += 1
                if run > maxr:
                    maxr = run
            else:
                run = 0
        frac = dis / total
        mean = sum(scores) / total
        res = (frac, mean, maxr)
        cache[seq] = res
        return res

    def validator(chain: str) -> bool:
        frac, mean, maxr = metrics(chain)
        if frac > max_frac:
            return False
        if maxr >= max_run:
            return False
        if mean > max_mean:
            return False
        return True

    return validator


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pairs", default="layer_outputs/pairs_1_layer.tsv")
    parser.add_argument("--fasta", default="layer_outputs/pdb_sequences_layer_1.fasta")
    parser.add_argument("--log-every", type=int, default=10000)
    parser.add_argument("--log-fails", type=int, default=5)
    parser.add_argument("--iupred-mode", choices=["long", "short", "glob"], default="long")
    parser.add_argument("--iupred-smoothing", choices=["no", "medium", "strong"], default="no")
    parser.add_argument("--disorder-threshold", type=float, default=0.5)
    parser.add_argument("--max-disordered-fraction", type=float, default=0.5)
    parser.add_argument("--max-disordered-run", type=int, default=60)
    parser.add_argument("--max-mean-score", type=float, default=0.5)
    parser.add_argument("--seed", type=int, default=123)
    parser.add_argument("--max-lev", type=int, default=0, help="Limit to pairs with lev <= max-lev (0 = no limit)")
    parser.add_argument("--no-validator", action="store_true", help="Run without validator")
    args = parser.parse_args()

    os.environ["DSC_VERBOSE"] = "1"
    os.environ["DSC_LOG_EVERY"] = str(args.log_every)
    os.environ["DSC_LOG_FAILS"] = str(args.log_fails)

    seqs = read_fasta(args.fasta)
    idx1, idx2, lev = find_max_lev_pair(args.pairs, args.max_lev)
    a_seq = seqs[idx1]
    b_seq = seqs[idx2]

    print(f"[debug] selected max lev pair idx1={idx1} idx2={idx2} lev={lev}")
    print(f"[debug] len(a)={len(a_seq)} len(b)={len(b_seq)}")

    if args.no_validator:
        validator = None
    else:
        iupred_validator = iupred_validator_factory(
            args.iupred_mode,
            args.iupred_smoothing,
            args.disorder_threshold,
            args.max_disordered_fraction,
            args.max_disordered_run,
            args.max_mean_score,
        )
        validator = iupred_validator

    steps, operations, distance = dsc.algo_seq_dynamic_with_validation_run(
        a_seq,
        b_seq,
        validator=validator,
    )

    print("[debug] completed")
    if steps is None:
        print("Не найден валидный путь преобразования")
    else:
        print(f"Найден путь! Расстояние: {distance}")
        print(f"Операции: {operations}")
        print(f"Шаги преобразования: {steps}")
        print("Последовательность преобразования:")
        for i, step in enumerate(steps):
            print(f"{i}: {step}")


if __name__ == "__main__":
    main()
