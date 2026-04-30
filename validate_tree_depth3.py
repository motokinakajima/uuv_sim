#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate a learned depth-3 decision tree by running many independent batch simulations."
    )

    parser.add_argument("--runs", type=int, default=4500)
    parser.add_argument("--field-seed", type=int, default=42)
    parser.add_argument("--tree-file", type=str, default="best_tree_depth3.txt")

    parser.add_argument("--agent-count", type=int, default=3)
    parser.add_argument("--max-steps", type=int, default=7500)
    parser.add_argument("--window", type=int, default=150)
    parser.add_argument("--hold", type=int, default=150)
    parser.add_argument("--num-gaussians", type=int, default=20)
    parser.add_argument("--eps-v", type=float, default=0.03)
    parser.add_argument("--eps-f", type=float, default=5e-4)
    parser.add_argument("--use-speed-check", action="store_true")
    parser.add_argument("--init-pos-range", type=float, default=20.0)
    parser.add_argument("--init-vel-range", type=float, default=1.0)

    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--output-csv", type=str, default="tree_validation_runs.csv")
    parser.add_argument("--overwrite-log", action="store_true")
    parser.add_argument("--quiet", action="store_true")

    return parser.parse_args()


def run_command(cmd: list[str], cwd: Path) -> None:
    proc = subprocess.run(cmd, cwd=str(cwd), text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}")


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


def ensure_build(root: Path) -> Path:
    run_command(["cmake", "-S", ".", "-B", "build"], root)
    run_command(["cmake", "--build", "build"], root)

    exe = detect_executable(root)
    if exe is None:
        raise FileNotFoundError("Built executable was not found under build/")
    return exe


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


def load_tree_specs(tree_file: Path) -> list[str]:
    specs: list[str] = []
    for line in tree_file.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        specs.append(stripped)
    if not specs:
        raise ValueError(f"Tree file is empty: {tree_file}")
    return specs


def ensure_csv_writer(path: Path, overwrite: bool) -> tuple[csv.DictWriter, object]:
    fieldnames = [
        "run_id",
        "timestamp",
        "seed",
        "field_seed",
        "tree_file",
        "agent_count",
        "max_steps",
        "window",
        "hold",
        "num_gaussians",
        "eps_v",
        "eps_f",
        "use_speed_check",
        "init_pos_range",
        "init_vel_range",
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
        "run_seconds",
    ]

    mode = "w" if overwrite else "a"
    file_obj = path.open(mode, newline="", encoding="utf-8")
    writer = csv.DictWriter(file_obj, fieldnames=fieldnames)
    if overwrite or path.stat().st_size == 0:
        writer.writeheader()
        file_obj.flush()
    return writer, file_obj


def build_batch_command(exe_path: Path, args: argparse.Namespace, tree_specs: list[str], seed: int) -> list[str]:
    cmd = [
        str(exe_path),
        "--batch-run",
        "--seed",
        str(seed),
        "--field-seed",
        str(args.field_seed),
        "--agent-count",
        str(args.agent_count),
        "--max-steps",
        str(args.max_steps),
        "--window",
        str(args.window),
        "--hold",
        str(args.hold),
        "--num-gaussians",
        str(args.num_gaussians),
        "--eps-v",
        str(args.eps_v),
        "--eps-f",
        str(args.eps_f),
        "--init-pos-range",
        str(args.init_pos_range),
        "--init-vel-range",
        str(args.init_vel_range),
    ]

    if args.use_speed_check:
        cmd.append("--use-speed-check")

    for spec in tree_specs:
        cmd.extend(["--tree-node", spec])
    cmd.extend(["--tree-root", "0"])
    return cmd


def run_one_validation_run(
    exe_path: Path,
    args: argparse.Namespace,
    tree_specs: list[str],
    run_id: int,
) -> dict:
    seed = 42 + run_id - 1
    cmd = build_batch_command(exe_path, args, tree_specs, seed)

    started = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    run_seconds = time.time() - started

    if proc.returncode != 0:
        stderr_tail = proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else ""
        raise RuntimeError(f"run_id={run_id} failed with code={proc.returncode}, stderr_tail={stderr_tail}")

    summary = parse_summary_json(proc.stdout)
    summary["run_id"] = run_id
    summary["timestamp"] = datetime.now().isoformat(timespec="seconds")
    summary["seed"] = seed
    summary["field_seed"] = args.field_seed
    summary["tree_file"] = args.tree_file
    summary["run_seconds"] = run_seconds
    summary["window"] = args.window
    summary["hold"] = args.hold
    summary["num_gaussians"] = args.num_gaussians
    summary["eps_v"] = args.eps_v
    summary["eps_f"] = args.eps_f
    summary["use_speed_check"] = args.use_speed_check
    summary["init_pos_range"] = args.init_pos_range
    summary["init_vel_range"] = args.init_vel_range
    return summary


def summarize_runs(rows: list[dict]) -> dict:
    if not rows:
        return {}

    total = len(rows)
    converged_count = sum(int(row.get("converged", 0)) for row in rows)
    return {
        "runs": total,
        "converged_runs": converged_count,
        "convergence_rate": converged_count / total,
        "mean_best_field_final": sum(float(row.get("best_field_final", 0.0)) for row in rows) / total,
        "mean_improvement_from_start": sum(float(row.get("improvement_from_start", 0.0)) for row in rows) / total,
        "mean_runtime_ms": sum(float(row.get("runtime_ms", 0.0)) for row in rows) / total,
    }


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parent

    if args.runs <= 0:
        raise ValueError("--runs must be > 0")

    tree_path = (root / args.tree_file).resolve()
    if not tree_path.exists():
        raise FileNotFoundError(f"Tree file not found: {tree_path}")

    tree_specs = load_tree_specs(tree_path)

    exe_path = detect_executable(root)
    if exe_path is None or not args.skip_build:
        print("[setup] configuring and building project...")
        exe_path = ensure_build(root)

    print(f"[setup] executable: {exe_path}")
    print(f"[setup] tree_file: {tree_path}")
    print(f"[setup] runs: {args.runs}")
    print("[setup] seed: 42..N (varies per run)")
    print(f"[setup] field_seed: {args.field_seed}")

    csv_path = (root / args.output_csv).resolve()
    writer, csv_file = ensure_csv_writer(csv_path, overwrite=bool(args.overwrite_log))

    started = time.time()
    completed_rows: list[dict] = []
    failed = 0

    try:
        if args.workers <= 1:
            for run_id in range(1, args.runs + 1):
                if not args.quiet:
                    print(f"[run {run_id}/{args.runs}] seed={42 + run_id - 1} field_seed={args.field_seed}")
                try:
                    row = run_one_validation_run(exe_path, args, tree_specs, run_id)
                    completed_rows.append(row)
                    writer.writerow({key: row.get(key, "") for key in writer.fieldnames})
                except Exception as exc:
                    failed += 1
                    print(f"[error] run_id={run_id}: {exc}")

                if (len(completed_rows) + failed) % 50 == 0 or (len(completed_rows) + failed) == args.runs:
                    elapsed = time.time() - started
                    rate = (len(completed_rows) + failed) / elapsed if elapsed > 0 else 0.0
                    print(
                        f"[progress] done={len(completed_rows) + failed}/{args.runs} "
                        f"ok={len(completed_rows)} fail={failed} rate={rate:.2f} runs/s"
                    )

                if (len(completed_rows) + failed) % 20 == 0:
                    csv_file.flush()
        else:
            jobs = [(exe_path, args, tree_specs, run_id) for run_id in range(1, args.runs + 1)]
            with ThreadPoolExecutor(max_workers=args.workers) as executor:
                for row in executor.map(lambda job: run_one_validation_run(*job), jobs):
                    completed_rows.append(row)
                    writer.writerow({key: row.get(key, "") for key in writer.fieldnames})
                    if len(completed_rows) % 50 == 0 or len(completed_rows) == args.runs:
                        elapsed = time.time() - started
                        rate = len(completed_rows) / elapsed if elapsed > 0 else 0.0
                        print(
                            f"[progress] done={len(completed_rows)}/{args.runs} "
                            f"ok={len(completed_rows)} fail={failed} rate={rate:.2f} runs/s"
                        )
                    if len(completed_rows) % 20 == 0:
                        csv_file.flush()

    finally:
        csv_file.flush()
        csv_file.close()

    elapsed = time.time() - started
    summary = summarize_runs(completed_rows)

    print(f"[done] completed={len(completed_rows)}, failed={failed}, elapsed_sec={elapsed:.1f}")
    print(f"[done] csv={csv_path}")
    print(json.dumps(summary, ensure_ascii=False))

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("[fatal] interrupted")
        raise SystemExit(130)
    except Exception as exc:
        print(f"[fatal] {exc}")
        raise SystemExit(1)