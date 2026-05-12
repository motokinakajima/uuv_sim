#!/usr/bin/env python3
import argparse
import csv
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run parallel fixed-field experiments and append per-run summaries to CSV."
    )
    parser.add_argument("--total-runs", type=int, default=25000)
    parser.add_argument("--agent-counts", type=str, default="3,5,8,10,15,20,30")
    parser.add_argument("--field-seed", type=int, default=20260407)
    parser.add_argument("--base-seed", type=int, default=1)

    parser.add_argument("--max-steps", type=int, default=7500)
    parser.add_argument("--eps-v", type=float, default=0.03)
    parser.add_argument("--eps-f", type=float, default=5e-4)
    parser.add_argument("--window", type=int, default=150)
    parser.add_argument("--hold", type=int, default=150)
    parser.add_argument("--num-gaussians", type=int, default=20)

    parser.add_argument("--init-pos-range", type=float, default=20.0)
    parser.add_argument("--init-vel-range", type=float, default=1.0)

    parser.add_argument("--worker-ratio", type=float, default=0.7)
    parser.add_argument("--workers", type=int, default=0)

    parser.add_argument("--csv-path", type=str, default="experiment_results_summary.csv")
    parser.add_argument("--append", action="store_true", default=True)
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def parse_agent_counts(raw: str) -> list[int]:
    values = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        values.append(int(token))

    unique_sorted = sorted(set(values))
    if not unique_sorted:
        raise ValueError("agent-counts is empty")

    for value in unique_sorted:
        if value <= 0:
            raise ValueError(f"agent-count must be > 0: {value}")

    return unique_sorted


def detect_executable(root: Path) -> Path | None:
    candidates = [
        root / "build" / "Debug" / "uuv_sim.exe",
        root / "build" / "uuv_sim.exe",
        root / "build" / "Debug" / "uuv_sim",
        root / "build" / "uuv_sim",
    ]
    for path in candidates:
        if path.exists():
            return path
    return None


def run_command(cmd: list[str], cwd: Path) -> None:
    proc = subprocess.run(cmd, cwd=str(cwd), text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}")


def ensure_build(root: Path) -> Path:
    run_command(["cmake", "-S", ".", "-B", "build"], root)
    run_command(["cmake", "--build", "build", "--config", "Debug"], root)

    exe = detect_executable(root)
    if exe is None:
        raise FileNotFoundError("Built executable was not found under build/")
    return exe


def distribute_runs(total_runs: int, agent_counts: list[int]) -> list[tuple[int, int]]:
    pairs = []
    base = total_runs // len(agent_counts)
    remainder = total_runs % len(agent_counts)

    for idx, agent_count in enumerate(agent_counts):
        runs_for_count = base + (1 if idx < remainder else 0)
        if runs_for_count > 0:
            pairs.append((agent_count, runs_for_count))

    return pairs


def parse_summary_json(stdout: str) -> dict:
    for line in reversed(stdout.splitlines()):
        line = line.strip()
        if not line:
            continue
        if not (line.startswith("{") and line.endswith("}")):
            continue
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            continue
    raise ValueError("Could not find JSON summary in process output")


def run_one(
    exe_path: Path,
    run_id: int,
    agent_count: int,
    seed: int,
    field_seed: int,
    max_steps: int,
    eps_v: float,
    eps_f: float,
    window: int,
    hold: int,
    num_gaussians: int,
    init_pos_range: float,
    init_vel_range: float,
) -> dict:
    cmd = [
        str(exe_path),
        "--batch-run",
        "--agent-count",
        str(agent_count),
        "--seed",
        str(seed),
        "--field-seed",
        str(field_seed),
        "--max-steps",
        str(max_steps),
        "--eps-v",
        str(eps_v),
        "--eps-f",
        str(eps_f),
        "--window",
        str(window),
        "--hold",
        str(hold),
        "--num-gaussians",
        str(num_gaussians),
        "--init-pos-range",
        str(init_pos_range),
        "--init-vel-range",
        str(init_vel_range),
    ]

    proc = subprocess.run(cmd, text=True, capture_output=True)
    if proc.returncode != 0:
        stderr_tail = proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else ""
        raise RuntimeError(
            f"run_id={run_id} failed with code={proc.returncode}, stderr_tail={stderr_tail}"
        )

    summary = parse_summary_json(proc.stdout)
    summary["run_id"] = run_id
    summary["timestamp"] = datetime.now().isoformat(timespec="seconds")
    return summary


def default_workers(worker_ratio: float) -> int:
    logical = os.cpu_count() or 1
    workers = int(logical * worker_ratio)
    return max(1, workers)


def ensure_csv_header(csv_path: Path, overwrite: bool) -> tuple[csv.DictWriter, object]:
    fieldnames = [
        "run_id",
        "timestamp",
        "agent_count",
        "seed",
        "field_seed",
        "max_steps",
        "steps_executed",
        "converged",
        "converge_step",
        "best_field_final",
        "mean_field_final",
        "best_field_min_over_run",
        "improvement_from_start",
        "swarm_radius_final",
        "avg_speed_final",
        "runtime_ms",
    ]

    mode = "w" if overwrite else "a"
    file_obj = csv_path.open(mode, newline="", encoding="utf-8")
    writer = csv.DictWriter(file_obj, fieldnames=fieldnames)

    should_write_header = overwrite or csv_path.stat().st_size == 0
    if should_write_header:
        writer.writeheader()
        file_obj.flush()

    return writer, file_obj


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parent

    if args.total_runs <= 0:
        raise ValueError("--total-runs must be > 0")

    agent_counts = parse_agent_counts(args.agent_counts)

    workers = args.workers if args.workers > 0 else default_workers(args.worker_ratio)

    exe_path = detect_executable(root)
    if exe_path is None or not args.skip_build:
        print("[setup] configuring and building project...")
        exe_path = ensure_build(root)

    print(f"[setup] executable: {exe_path}")
    print(f"[setup] workers: {workers}")
    print(f"[setup] agent_counts: {agent_counts}")

    run_plan = []
    seed = args.base_seed
    distribution = distribute_runs(args.total_runs, agent_counts)
    run_id = 1

    for agent_count, count in distribution:
        for _ in range(count):
            run_plan.append((run_id, agent_count, seed))
            run_id += 1
            seed += 1

    print(f"[setup] total runs planned: {len(run_plan)}")

    csv_path = (root / args.csv_path).resolve()
    overwrite = bool(args.overwrite)
    writer, csv_file = ensure_csv_header(csv_path, overwrite)

    started = time.time()
    completed = 0
    failed = 0

    future_to_meta = {}

    try:
        with ThreadPoolExecutor(max_workers=workers) as executor:
            for run_id, agent_count, seed in run_plan:
                fut = executor.submit(
                    run_one,
                    exe_path,
                    run_id,
                    agent_count,
                    seed,
                    args.field_seed,
                    args.max_steps,
                    args.eps_v,
                    args.eps_f,
                    args.window,
                    args.hold,
                    args.num_gaussians,
                    args.init_pos_range,
                    args.init_vel_range,
                )
                future_to_meta[fut] = (run_id, agent_count, seed)

            for fut in as_completed(future_to_meta):
                run_id, agent_count, seed = future_to_meta[fut]
                try:
                    row = fut.result()
                    writer.writerow(row)
                    completed += 1
                except Exception as ex:
                    failed += 1
                    print(f"[error] run_id={run_id}, agent_count={agent_count}, seed={seed}: {ex}")

                if (completed + failed) % 100 == 0 or (completed + failed) == len(run_plan):
                    elapsed = time.time() - started
                    rate = (completed + failed) / elapsed if elapsed > 0 else 0.0
                    print(
                        f"[progress] done={completed + failed}/{len(run_plan)} "
                        f"ok={completed} fail={failed} rate={rate:.2f} runs/s"
                    )

                if (completed + failed) % 20 == 0:
                    csv_file.flush()

    finally:
        csv_file.flush()
        csv_file.close()

    elapsed = time.time() - started
    print(f"[done] completed={completed}, failed={failed}, elapsed_sec={elapsed:.1f}")
    print(f"[done] csv={csv_path}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[fatal] {exc}")
        raise SystemExit(1)
