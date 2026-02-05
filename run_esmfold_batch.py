#!/usr/bin/env python3
import argparse
import os
import queue
import re
import sys
import time
from dataclasses import dataclass
from multiprocessing import get_context
from typing import Iterable, List, Tuple


def read_fasta(path: str) -> List[Tuple[str, str]]:
    records: List[Tuple[str, str]] = []
    cur_id = None
    cur_seq = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith(">"):
                if cur_id is not None:
                    records.append((cur_id, "".join(cur_seq)))
                header = line[1:].strip()
                cur_id = header.split()[0] if header else "seq"
                cur_seq = []
            else:
                cur_seq.append(line)
        if cur_id is not None:
            records.append((cur_id, "".join(cur_seq)))
    return records


def sanitize_id(seq_id: str) -> str:
    safe = re.sub(r"[^A-Za-z0-9._-]+", "_", seq_id)
    return safe[:200] if safe else "seq"


@dataclass
class Task:
    idx: int
    seq_id: str
    seq: str


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


def worker_main(
    task_q,
    done_q,
    device: str,
    out_dir: str,
    batch_size: int,
    chunk_size: int,
):
    try:
        import torch
        import esm
    except Exception as exc:  # pragma: no cover
        done_q.put(("fatal", device, f"import error: {exc}"))
        return

    print(f"[esmfold] loading model on {device}...", flush=True)
    torch.set_grad_enabled(False)
    model = load_esmfold_model()
    model = model.eval().to(device)
    if device == "cpu":
        model = model.float()
    if chunk_size > 0:
        model.set_chunk_size(chunk_size)
    print(f"[esmfold] model ready on {device}", flush=True)
    done_q.put(("ready", device))

    batch: List[Task] = []

    def flush_batch():
        if not batch:
            return
        seqs = [t.seq for t in batch]
        try:
            with torch.no_grad():
                pdbs = model.infer_pdbs(seqs)
        except Exception as exc:
            for t in batch:
                done_q.put(("error", t.idx, t.seq_id, str(exc)))
            batch.clear()
            return
        for t, pdb in zip(batch, pdbs):
            filename = f"{t.idx:08d}_{sanitize_id(t.seq_id)}.pdb"
            out_path = os.path.join(out_dir, filename)
            try:
                with open(out_path, "w", encoding="utf-8") as f:
                    f.write(pdb)
                done_q.put(("ok", t.idx, t.seq_id))
            except Exception as exc:
                done_q.put(("error", t.idx, t.seq_id, str(exc)))
        batch.clear()

    while True:
        try:
            item = task_q.get(timeout=0.5)
        except queue.Empty:
            flush_batch()
            continue
        if item is None:
            flush_batch()
            break
        batch.append(item)
        if len(batch) >= batch_size:
            flush_batch()


def format_eta(seconds: float) -> str:
    if seconds < 0:
        return "?"
    m, s = divmod(int(seconds), 60)
    h, m = divmod(m, 60)
    if h:
        return f"{h}h{m:02d}m"
    if m:
        return f"{m}m{s:02d}s"
    return f"{s}s"


def iter_tasks(records: Iterable[Tuple[str, str]]) -> Iterable[Task]:
    for idx, (seq_id, seq) in enumerate(records):
        yield Task(idx=idx, seq_id=seq_id, seq=seq)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run ESMFold on sequences from FASTA.")
    parser.add_argument("--fasta", default="validation_outputs/new_sequences.fasta", help="Input FASTA")
    parser.add_argument("--out-dir", default="esmfold_outputs", help="Output directory for PDBs")
    parser.add_argument("--gpus", default="0,1", help="Comma-separated GPU ids")
    parser.add_argument("--batch-size", type=int, default=1, help="Batch size per GPU")
    parser.add_argument("--chunk-size", type=int, default=0, help="ESMFold chunk size (0 = default)")
    parser.add_argument("--skip-existing", action="store_true", help="Skip sequences with existing PDB outputs")
    parser.add_argument("--no-preload", action="store_true", help="Do not preload model weights in main process")
    args = parser.parse_args()

    records = read_fasta(args.fasta)
    if not records:
        print("No sequences found.", file=sys.stderr)
        return 1

    os.makedirs(args.out_dir, exist_ok=True)

    use_cpu = os.environ.get("ESMFOLD_CPU", "0") == "1"
    gpu_ids = [g.strip() for g in args.gpus.split(",") if g.strip() != ""]
    devices = [f"cuda:{g}" for g in gpu_ids] if gpu_ids else ["cuda:0"]
    if use_cpu:
        devices = ["cpu"]
    else:
        try:
            import torch

            if not torch.cuda.is_available():
                print("[esmfold] CUDA not available, falling back to CPU", file=sys.stderr)
                devices = ["cpu"]
        except Exception:
            pass

    if not args.no_preload:
        try:
            import esm
            import torch
            print("[esmfold] preloading weights in main process...", flush=True)
            model = load_esmfold_model()
            model.eval()
            del model
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
            print("[esmfold] preload complete", flush=True)
        except Exception as exc:
            print(f"[esmfold] preload failed: {exc}", file=sys.stderr)

    ctx = get_context("spawn")
    task_q = ctx.Queue(maxsize=1024)
    done_q = ctx.Queue()

    workers = []
    for dev in devices:
        p = ctx.Process(
            target=worker_main,
            args=(task_q, done_q, dev, args.out_dir, args.batch_size, args.chunk_size),
        )
        p.daemon = True
        p.start()
        workers.append(p)

    total = len(records)
    start = time.time()
    submitted = 0
    processed = 0
    errors = 0
    ready = 0

    last_report = 0.0
    while ready < len(workers):
        try:
            msg = done_q.get(timeout=0.5)
        except queue.Empty:
            msg = None
        now = time.time()
        if msg and msg[0] == "ready":
            ready += 1
            print(f"[esmfold] worker ready: {msg[1]}", flush=True)
        if now - last_report >= 0.5:
            print(
                f"\r[esmfold] waiting for workers {ready}/{len(workers)}...",
                end="",
                flush=True,
            )
            last_report = now
    print()

    if args.skip_existing:
        existing = set(os.listdir(args.out_dir))
    else:
        existing = set()

    for task in iter_tasks(records):
        filename = f"{task.idx:08d}_{sanitize_id(task.seq_id)}.pdb"
        if filename in existing:
            processed += 1
            continue
        while True:
            try:
                task_q.put(task, timeout=0.1)
                submitted += 1
                break
            except queue.Full:
                now = time.time()
                if now - last_report >= 0.5:
                    print(
                        f"\r[esmfold] queue full, queued {submitted}/{total}...",
                        end="",
                        flush=True,
                    )
                    last_report = now

    for _ in workers:
        task_q.put(None)

    while processed < total:
        try:
            msg = done_q.get(timeout=0.5)
        except queue.Empty:
            msg = None

        now = time.time()
        if msg:
            if msg[0] == "fatal":
                print(f"[esmfold] worker error on {msg[1]}: {msg[2]}", file=sys.stderr)
                return 2
            if msg[0] == "ready":
                continue
            if msg[0] == "ok":
                processed += 1
            elif msg[0] == "error":
                processed += 1
                errors += 1

        if now - last_report >= 0.5 or processed == total:
            elapsed = now - start
            rate = processed / elapsed if elapsed > 0 else 0.0
            remaining = total - processed
            eta = remaining / rate if rate > 0 else -1.0
            print(
                f"\r[esmfold] {processed}/{total} done | {submitted}/{total} queued | {rate:.2f} seq/s | ETA {format_eta(eta)} | errors {errors}",
                end="",
                flush=True,
            )
            last_report = now

    print()
    for p in workers:
        p.join(timeout=1.0)

    print(f"[esmfold] done. outputs: {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
