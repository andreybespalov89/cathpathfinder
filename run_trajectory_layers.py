#!/usr/bin/env python3
import argparse
import io
import os
import queue
import re
import sys
from dataclasses import dataclass
from multiprocessing import get_context
from typing import Dict, Iterable, List, Tuple


def read_fasta(path: str) -> List[Tuple[str, str]]:
    records: List[Tuple[str, str]] = []
    cur_id = None
    cur_seq: List[str] = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith(">"):
                if cur_id is not None:
                    records.append((cur_id, "".join(cur_seq)))
                cur_id = line[1:].strip()
                cur_seq = []
            else:
                cur_seq.append(line)
        if cur_id is not None:
            records.append((cur_id, "".join(cur_seq)))
    return records


def parse_header(header: str, fallback_idx: int) -> Tuple[str, int, int]:
    layer = 0
    pair = fallback_idx
    step = fallback_idx
    m = re.search(r"layer(\d+)", header)
    if m:
        layer = int(m.group(1))
    m = re.search(r"pair(\d+)", header)
    if m:
        pair = int(m.group(1))
    m = re.search(r"step=(\d+)", header)
    if m:
        step = int(m.group(1))
    return f"layer{layer}|pair{pair}", step, pair


def build_levels(steps_count: int) -> List[List[int]]:
    if steps_count <= 2:
        return []
    intervals = [(0, steps_count - 1)]
    levels: List[List[int]] = []
    while intervals:
        level_indices: List[int] = []
        next_intervals: List[Tuple[int, int]] = []
        for start, end in intervals:
            if end - start <= 1:
                continue
            mid = (start + end) // 2
            level_indices.append(mid)
            if mid - start > 1:
                next_intervals.append((start, mid))
            if end - mid > 1:
                next_intervals.append((mid, end))
        if not level_indices:
            break
        levels.append(level_indices)
        intervals = next_intervals
    return levels


def load_esmfold_model():
    import torch
    import esm

    try:
        return esm.pretrained.esmfold_v1()
    except RuntimeError as exc:
        msg = str(exc)
        if "missing" not in msg:
            raise

    weights_path = os.environ.get("ESMFOLD_WEIGHTS")
    if not weights_path:
        torch_home = os.environ.get("TORCH_HOME", os.path.expanduser("~/.cache/torch"))
        weights_path = os.path.join(torch_home, "hub", "checkpoints", "esmfold_3B_v1.pt")
    if not os.path.isfile(weights_path):
        raise FileNotFoundError(f"ESMFold weights not found: {weights_path}")

    model_data = torch.load(weights_path, map_location="cpu")
    cfg = model_data["cfg"]["model"]
    model_state = model_data["model"]

    def remap_key(key: str) -> str:
        prefix = "trunk.structure_module.ipa."
        for name in ("linear_q_points", "linear_kv_points"):
            marker = prefix + name + "."
            if key.startswith(marker) and ".linear." not in key:
                return marker + "linear." + key[len(marker):]
        return key

    remapped_state = {remap_key(k): v for k, v in model_state.items()}

    from esm.esmfold.v1.esmfold import ESMFold

    model = ESMFold(esmfold_config=cfg)
    model.load_state_dict(remapped_state, strict=False)
    return model


def rmsd_ca(pdb_a: str, pdb_b: str) -> float:
    from Bio.PDB import PDBParser, Superimposer

    parser = PDBParser(QUIET=True)
    struct_a = parser.get_structure("a", io.StringIO(pdb_a))
    struct_b = parser.get_structure("b", io.StringIO(pdb_b))

    atoms_a = []
    atoms_b = []

    for model_a, model_b in zip(struct_a, struct_b):
        for chain_a, chain_b in zip(model_a, model_b):
            for res_a, res_b in zip(chain_a, chain_b):
                if "CA" in res_a and "CA" in res_b:
                    atoms_a.append(res_a["CA"])
                    atoms_b.append(res_b["CA"])

    if not atoms_a or len(atoms_a) != len(atoms_b):
        return float("inf")

    sup = Superimposer()
    sup.set_atoms(atoms_a, atoms_b)
    return float(sup.rms)


@dataclass
class Task:
    seq_id: str
    seq: str


@dataclass
class Result:
    seq_id: str
    status: str
    rmsd_max: float
    details: str


def worker_main(task_q, result_q, device: str, out_dir: str, num_samples: int, rmsd_thr: float, seed_base: int, stochastic: bool):
    try:
        import torch
        import esm
    except Exception as exc:  # pragma: no cover
        result_q.put(Result("", "fatal", float("inf"), f"import error: {exc}"))
        return

    torch.set_grad_enabled(False)
    model = load_esmfold_model().eval().to(device)
    if device == "cpu":
        model = model.float()

    while True:
        try:
            task = task_q.get(timeout=0.5)
        except queue.Empty:
            continue
        if task is None:
            break

        seq_id = task.seq_id
        seq = task.seq
        seq_dir = os.path.join(out_dir, "pdbs")
        os.makedirs(seq_dir, exist_ok=True)

        pdbs: List[str] = []
        try:
            for i in range(num_samples):
                seed = seed_base + i
                torch.manual_seed(seed)
                if stochastic:
                    model.train()
                else:
                    model.eval()
                with torch.no_grad():
                    pdb = model.infer_pdbs([seq])[0]
                pdbs.append(pdb)
        except Exception as exc:
            result_q.put(Result(seq_id, "error", float("inf"), str(exc)))
            continue

        rmsd_max = 0.0
        if len(pdbs) > 1:
            ref = pdbs[0]
            for other in pdbs[1:]:
                r = rmsd_ca(ref, other)
                if r > rmsd_max:
                    rmsd_max = r
        ok = rmsd_max <= rmsd_thr

        if ok:
            out_path = os.path.join(seq_dir, f"{seq_id}.pdb")
            with open(out_path, "w", encoding="utf-8") as f:
                f.write(pdbs[0])
            result_q.put(Result(seq_id, "ok", rmsd_max, ""))
        else:
            result_q.put(Result(seq_id, "bad_rmsd", rmsd_max, ""))


def main() -> int:
    parser = argparse.ArgumentParser(description="Build trajectory midpoints iteratively and generate structures with RMSD filtering.")
    parser.add_argument("--fasta", required=True, help="Input FASTA with trajectory steps (e.g. validation_outputs/new_sequences.fasta)")
    parser.add_argument("--out-dir", default="trajectory_levels", help="Output directory")
    parser.add_argument("--gpus", default="0", help="Comma-separated GPU ids (or empty for CPU)")
    parser.add_argument("--num-samples", type=int, default=5, help="Number of structures per sequence")
    parser.add_argument("--rmsd-threshold", type=float, default=2.0, help="Max RMSD (A) across samples to keep")
    parser.add_argument("--max-levels", type=int, default=0, help="Limit levels (0 = all)")
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--stochastic", action="store_true", help="Enable stochastic sampling (train mode) for diversity")
    parser.add_argument("--skip-existing", action="store_true")
    args = parser.parse_args()

    records = read_fasta(args.fasta)
    if not records:
        print("No sequences found.", file=sys.stderr)
        return 1

    traj_map: Dict[str, List[Tuple[int, str, str]]] = {}
    for i, (header, seq) in enumerate(records):
        traj_key, step, _pair = parse_header(header, i)
        traj_map.setdefault(traj_key, []).append((step, seq, header))

    for key in traj_map:
        traj_map[key].sort(key=lambda x: x[0])

    # Build level assignments
    level_to_entries: Dict[int, List[Tuple[str, str]]] = {}
    for traj_key, steps in traj_map.items():
        seqs = [s for _step, s, _hdr in steps]
        levels = build_levels(len(seqs))
        for level_idx, indices in enumerate(levels, start=1):
            for pos in indices:
                step_val = steps[pos][0]
                seq = steps[pos][1]
                seq_id = f"{traj_key}|step={step_val}|pos={pos}|level={level_idx}"
                level_to_entries.setdefault(level_idx, []).append((seq_id, seq))

    if not level_to_entries:
        print("Nothing to generate: trajectories too short.", file=sys.stderr)
        return 1

    out_root = args.out_dir
    os.makedirs(out_root, exist_ok=True)

    max_level = max(level_to_entries.keys())
    if args.max_levels > 0:
        max_level = min(max_level, args.max_levels)

    gpu_ids = [g.strip() for g in args.gpus.split(",") if g.strip()]
    devices = [f"cuda:{g}" for g in gpu_ids] if gpu_ids else ["cpu"]

    for level in range(1, max_level + 1):
        entries = level_to_entries.get(level, [])
        if not entries:
            continue
        level_dir = os.path.join(out_root, f"level_{level}")
        os.makedirs(level_dir, exist_ok=True)
        fasta_path = os.path.join(level_dir, "sequences.fasta")
        kept_path = os.path.join(level_dir, "kept.tsv")
        failed_path = os.path.join(level_dir, "failed.tsv")

        if not os.path.exists(fasta_path):
            with open(fasta_path, "w", encoding="utf-8") as f:
                for seq_id, seq in entries:
                    f.write(f">{seq_id}\n{seq}\n")

        # Resume support
        done = set()
        pdb_dir = os.path.join(level_dir, "pdbs")
        if args.skip_existing and os.path.isdir(pdb_dir):
            for name in os.listdir(pdb_dir):
                if name.endswith(".pdb"):
                    done.add(name[:-4])
        if args.skip_existing and os.path.exists(kept_path):
            with open(kept_path, "r", encoding="utf-8") as f:
                for line in f:
                    if line.strip():
                        done.add(line.split("\t")[0])
        if args.skip_existing and os.path.exists(failed_path):
            with open(failed_path, "r", encoding="utf-8") as f:
                for line in f:
                    if line.strip():
                        done.add(line.split("\t")[0])

        tasks = [Task(seq_id, seq) for seq_id, seq in entries if seq_id not in done]
        if not tasks:
            continue

        ctx = get_context("spawn")
        task_q = ctx.Queue(maxsize=2048)
        result_q = ctx.Queue()
        workers = []

        for device in devices:
            p = ctx.Process(
                target=worker_main,
                args=(task_q, result_q, device, level_dir, args.num_samples, args.rmsd_threshold, args.seed, args.stochastic),
            )
            p.daemon = True
            p.start()
            workers.append(p)

        for task in tasks:
            task_q.put(task)
        for _ in workers:
            task_q.put(None)

        processed = 0
        total = len(tasks)
        with open(kept_path, "a", encoding="utf-8") as kept_f, open(failed_path, "a", encoding="utf-8") as failed_f:
            while processed < total:
                try:
                    res: Result = result_q.get(timeout=1.0)
                except queue.Empty:
                    continue
                if res.status == "fatal":
                    print(f"[level {level}] worker fatal: {res.details}", file=sys.stderr)
                    return 2
                if res.status == "ok":
                    kept_f.write(f"{res.seq_id}\t{res.rmsd_max:.4f}\n")
                else:
                    failed_f.write(f"{res.seq_id}\t{res.status}\t{res.rmsd_max:.4f}\t{res.details}\n")
                processed += 1
                if processed % 100 == 0 or processed == total:
                    print(f"[level {level}] {processed}/{total} done", flush=True)

        for p in workers:
            p.join(timeout=1.0)

    print(f"[run_trajectory_layers] done. Output: {out_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
