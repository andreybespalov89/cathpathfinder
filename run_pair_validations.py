#!/usr/bin/env python3
import argparse
import json
import pathlib
import random
import re
import sys
import os
import time
import multiprocessing as mp
import signal
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor, as_completed
from functools import lru_cache
from typing import List, Tuple

try:
    import datasetgradualstep_c as dsc
except Exception as exc:  # noqa: BLE001
    print(f"[run_pair_validations] failed to import datasetgradualstep_c: {exc}")
    sys.exit(1)

try:
    from iupred3 import iupred3_lib as iupred3
except Exception as exc:  # noqa: BLE001
    print(f"[run_pair_validations] failed to import iupred3: {exc}")
    sys.exit(1)

_GLOBAL_SEQS: List[str] = []
_GLOBAL_PROB: float = 0.5
_GLOBAL_SEED: int = 123
_GLOBAL_NO_VALIDATOR: bool = False
_GLOBAL_IUPRED_MODE: str = "long"
_GLOBAL_IUPRED_SMOOTH: str = "no"
_GLOBAL_DISORDER_THR: float = 0.5
_GLOBAL_MAX_FRAC: float = 0.5
_GLOBAL_MAX_RUN: int = 60
_GLOBAL_MAX_MEAN: float = 0.5
_GLOBAL_TIMEOUT_S: int = 0


def _pair_worker(pair):
    if _GLOBAL_TIMEOUT_S > 0:
        def _alarm_handler(_signum, _frame):
            raise TimeoutError("pair timeout")
        signal.signal(signal.SIGALRM, _alarm_handler)
        signal.alarm(_GLOBAL_TIMEOUT_S)
    line_no, id1, id2, idx1, idx2, lev = pair
    if idx1 >= len(_GLOBAL_SEQS) or idx2 >= len(_GLOBAL_SEQS):
        return (line_no, id1, id2, idx1, idx2, "invalid_index", None, None, None)
    a_seq = _GLOBAL_SEQS[idx1]
    b_seq = _GLOBAL_SEQS[idx2]
    seed = (_GLOBAL_SEED * 1315423911) ^ (idx1 * 2654435761) ^ idx2
    rng = random.Random(seed)

    def custom_validator(chain: str) -> bool:
        return rng.random() < _GLOBAL_PROB

    if _GLOBAL_NO_VALIDATOR:
        validator = None
    else:
        validator = custom_validator

    def iupred_validator(chain: str) -> bool:
        if _GLOBAL_NO_VALIDATOR:
            return True
        if not _is_foldable(chain):
            return False
        return validator(chain)

    try:
        steps, operations, distance = dsc.algo_seq_dynamic_with_validation_run(
            a_seq,
            b_seq,
            validator=iupred_validator,
        )
        return (line_no, id1, id2, idx1, idx2, "ok", steps, operations, distance)
    except TimeoutError:
        return (line_no, id1, id2, idx1, idx2, "timeout", None, None, None)
    finally:
        if _GLOBAL_TIMEOUT_S > 0:
            signal.alarm(0)


@lru_cache(maxsize=10000)
def _iupred_metrics(seq: str):
    scores, _glob = iupred3.iupred(seq, mode=_GLOBAL_IUPRED_MODE, smoothing=_GLOBAL_IUPRED_SMOOTH)
    if not scores:
        return 0.0, 0.0, 0
    total = len(scores)
    dis = 0
    max_run = 0
    run = 0
    for v in scores:
        if v >= _GLOBAL_DISORDER_THR:
            dis += 1
            run += 1
            if run > max_run:
                max_run = run
        else:
            run = 0
    frac = dis / total
    mean = sum(scores) / total
    return frac, mean, max_run


def _is_foldable(seq: str) -> bool:
    frac, mean, max_run = _iupred_metrics(seq)
    if frac > _GLOBAL_MAX_FRAC:
        return False
    if max_run >= _GLOBAL_MAX_RUN:
        return False
    if mean > _GLOBAL_MAX_MEAN:
        return False
    return True


def read_fasta(path: pathlib.Path) -> Tuple[List[str], List[str]]:
    ids: List[str] = []
    seqs: List[str] = []
    cur_id = None
    cur_seq = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            if line.startswith(">"):
                if cur_id is not None:
                    ids.append(cur_id)
                    seqs.append("".join(cur_seq))
                cur_id = line[1:].strip().split()[0]
                cur_seq = []
            else:
                cur_seq.append(line.strip())
        if cur_id is not None:
            ids.append(cur_id)
            seqs.append("".join(cur_seq))
    return ids, seqs


def iter_pairs(path: pathlib.Path):
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, start=1):
            if not line.strip():
                continue
            cols = line.rstrip("\n").split("\t")
            if len(cols) < 4:
                continue
            id1, id2 = cols[0], cols[1]
            idx1, idx2 = int(cols[2]), int(cols[3])
            lev = int(cols[6]) if len(cols) >= 7 else None
            yield line_no, id1, id2, idx1, idx2, lev


def count_pairs(path: pathlib.Path) -> int:
    count = 0
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description="Run validation algorithm for all layer pairs.")
    parser.add_argument("--layers-dir", default="layer_outputs", help="Directory with pairs_*_layer.tsv and pdb_sequences_layer_*.fasta")
    parser.add_argument("--out-dir", default="validation_outputs", help="Output directory for results")
    parser.add_argument("--validator-prob", type=float, default=0.5, help="Probability for random validator to accept")
    parser.add_argument("--seed", type=int, default=123, help="Random seed")
    parser.add_argument("--threads", type=int, default=0, help="Worker threads (0=cpu count)")
    parser.add_argument("--backend", choices=["threads", "processes"], default="processes", help="Concurrency backend")
    parser.add_argument("--progress-interval", type=float, default=2.0, help="Seconds between progress lines")
    parser.add_argument("--no-validator", action="store_true", help="Run without validator for speed")
    parser.add_argument("--iupred-mode", choices=["long", "short", "glob"], default="long")
    parser.add_argument("--iupred-smoothing", choices=["no", "medium", "strong"], default="no")
    parser.add_argument("--disorder-threshold", type=float, default=0.5, help="Residue disorder cutoff")
    parser.add_argument("--max-disordered-fraction", type=float, default=0.5, help="Reject if fraction of disordered residues is higher")
    parser.add_argument("--max-disordered-run", type=int, default=60, help="Reject if max consecutive disordered run is >= this")
    parser.add_argument("--max-mean-score", type=float, default=0.5, help="Reject if mean IUPred score is higher")
    parser.add_argument("--max-lev", type=int, default=0, help="Skip pairs with lev above this (0 = no limit)")
    parser.add_argument("--pair-timeout", type=int, default=0, help="Timeout per pair in seconds (0 = no limit)")
    args = parser.parse_args()

    layers_dir = pathlib.Path(args.layers_dir)
    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    pairs_files = sorted(layers_dir.glob("pairs_*_layer.tsv"))
    layer_re = re.compile(r"pairs_(\d+)_layer\.tsv$")

    results_path = out_dir / "results.tsv"
    new_fasta_path = out_dir / "new_sequences.fasta"

    with results_path.open("w", encoding="utf-8") as results_f, new_fasta_path.open("w", encoding="utf-8") as fasta_f:
        results_f.write("layer\tline\tidx1\tidx2\tid1\tid2\tstatus\tdistance\toperations_json\tsteps_count\n")

        for pairs_path in pairs_files:
            m = layer_re.search(pairs_path.name)
            if not m:
                continue
            layer = int(m.group(1))
            fasta_path = layers_dir / f"pdb_sequences_layer_{layer}.fasta"
            if not fasta_path.exists():
                print(f"[run_pair_validations] missing {fasta_path}, skipping layer {layer}")
                continue

            print(f"[run_pair_validations] layer {layer}: reading fasta {fasta_path}")
            ids, seqs = read_fasta(fasta_path)

            print(f"[run_pair_validations] layer {layer}: processing pairs {pairs_path}")
            pairs_to_run = []
            skipped = 0
            for pair in iter_pairs(pairs_path):
                line_no, id1, id2, idx1, idx2, lev = pair
                if args.max_lev > 0 and lev is not None and lev > args.max_lev:
                    skipped += 1
                    continue
                pairs_to_run.append(pair)

            total_pairs = len(pairs_to_run)
            processed = 0
            last_update = 0.0
            start_time = time.time()
            found = 0
            not_found = 0
            invalid_index = 0
            timeouts = 0

            global _GLOBAL_SEQS, _GLOBAL_PROB, _GLOBAL_SEED, _GLOBAL_NO_VALIDATOR, _GLOBAL_TIMEOUT_S
            global _GLOBAL_IUPRED_MODE, _GLOBAL_IUPRED_SMOOTH, _GLOBAL_DISORDER_THR
            global _GLOBAL_MAX_FRAC, _GLOBAL_MAX_RUN, _GLOBAL_MAX_MEAN
            _GLOBAL_SEQS = seqs
            _GLOBAL_PROB = args.validator_prob
            _GLOBAL_SEED = args.seed
            _GLOBAL_NO_VALIDATOR = args.no_validator
            _GLOBAL_TIMEOUT_S = args.pair_timeout
            _GLOBAL_IUPRED_MODE = args.iupred_mode
            _GLOBAL_IUPRED_SMOOTH = args.iupred_smoothing
            _GLOBAL_DISORDER_THR = args.disorder_threshold
            _GLOBAL_MAX_FRAC = args.max_disordered_fraction
            _GLOBAL_MAX_RUN = args.max_disordered_run
            _GLOBAL_MAX_MEAN = args.max_mean_score

            threads = args.threads if args.threads > 0 else (os.cpu_count() or 1)
            if args.backend == "processes":
                ctx = mp.get_context("fork")
                executor = ProcessPoolExecutor(max_workers=threads, mp_context=ctx)
            else:
                executor = ThreadPoolExecutor(max_workers=threads)

            with executor as ex:
                futures = []
                for pair in pairs_to_run:
                    futures.append(ex.submit(_pair_worker, pair))
                try:
                    for fut in as_completed(futures):
                        line_no, id1, id2, idx1, idx2, status, steps, operations, distance = fut.result()
                        processed += 1
                        now = time.time()
                        if now - last_update >= args.progress_interval or processed == total_pairs:
                            rate = processed / max(1e-9, now - start_time)
                            remaining = total_pairs - processed
                            eta = remaining / rate if rate > 0 else 0.0
                            print(f"[layer {layer}] {processed}/{total_pairs} | {rate:.2f} pairs/s | ETA {eta:.1f}s")
                            last_update = now

                        if status == "invalid_index":
                            results_f.write(f"{layer}\t{line_no}\t{idx1}\t{idx2}\t{id1}\t{id2}\tinvalid_index\t\t\t0\n")
                            invalid_index += 1
                            continue

                        if steps is None:
                            if status == "timeout":
                                results_f.write(f"{layer}\t{line_no}\t{idx1}\t{idx2}\t{id1}\t{id2}\ttimeout\t\t\t0\n")
                                timeouts += 1
                            else:
                                results_f.write(f"{layer}\t{line_no}\t{idx1}\t{idx2}\t{id1}\t{id2}\tnot_found\t\t\t0\n")
                            not_found += 1
                            continue

                        ops_json = json.dumps(operations, ensure_ascii=True)
                        steps_count = len(steps)
                        results_f.write(f"{layer}\t{line_no}\t{idx1}\t{idx2}\t{id1}\t{id2}\tfound\t{distance}\t{ops_json}\t{steps_count}\n")
                        found += 1

                        for i, step in enumerate(steps):
                            fasta_f.write(f">layer{layer}|pair{line_no}|idx1={idx1}|idx2={idx2}|step={i}|distance={distance}\n")
                            fasta_f.write(f"{step}\n")
                except KeyboardInterrupt:
                    print("[run_pair_validations] received Ctrl+C, cancelling outstanding tasks...")
                    for fut in futures:
                        fut.cancel()
                    ex.shutdown(wait=False, cancel_futures=True)
                    print("[run_pair_validations] shutdown complete")
                    return 1

            print(
                f"[run_pair_validations] layer {layer}: done "
                f"(processed={processed}, found={found}, not_found={not_found}, timeout={timeouts}, invalid_index={invalid_index}, skipped={skipped})",
                flush=True,
            )

    print(f"[run_pair_validations] done. Results: {results_path}")
    print(f"[run_pair_validations] new sequences: {new_fasta_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
