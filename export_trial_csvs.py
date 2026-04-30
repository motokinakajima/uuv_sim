#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import time
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run one detailed trial and export per-step CSV logs.")
    parser.add_argument("--tree-file", type=str, default="best_tree_depth3.txt")
    parser.add_argument("--prefix", type=str, default="one_trial_5_agents")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--field-seed", type=int, default=42)
    parser.add_argument("--agent-count", type=int, default=5)
    parser.add_argument("--max-steps", type=int, default=2000)
    parser.add_argument("--window", type=int, default=150)
    parser.add_argument("--hold", type=int, default=150)
    parser.add_argument("--num-gaussians", type=int, default=20)
    parser.add_argument("--eps-v", type=float, default=0.03)
    parser.add_argument("--eps-f", type=float, default=5e-4)
    parser.add_argument("--use-speed-check", action="store_true")
    parser.add_argument("--init-pos-range", type=float, default=20.0)
    parser.add_argument("--init-vel-range", type=float, default=1.0)
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


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
    subprocess.run(["cmake", "-S", ".", "-B", "build"], cwd=root, check=True)
    subprocess.run(["cmake", "--build", "build"], cwd=root, check=True)

    exe = detect_executable(root)
    if exe is None:
        raise FileNotFoundError("Built executable was not found under build/")
    return exe


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


def parse_summary_json(stdout: str) -> dict:
    for line in reversed(stdout.splitlines()):
        line = line.strip()
        if not line:
            continue
        if line.startswith("{") and line.endswith("}"):
            return json.loads(line)
    raise ValueError("Could not find JSON summary in process output")


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parent
    tree_path = (root / args.tree_file).resolve()
    if not tree_path.exists():
        raise FileNotFoundError(f"Tree file not found: {tree_path}")

    tree_specs = load_tree_specs(tree_path)

    exe_path = detect_executable(root)
    if exe_path is None or not args.skip_build:
        print("[setup] configuring and building project...")
        exe_path = ensure_build(root)

    cmd = [
        str(exe_path),
        "--batch-run",
        "--seed",
        str(args.seed),
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
        "--csv-prefix",
        args.prefix,
    ]

    if args.use_speed_check:
        cmd.append("--use-speed-check")

    for spec in tree_specs:
        cmd.extend(["--tree-node", spec])
    cmd.extend(["--tree-root", "0"])

    print(f"[setup] executable: {exe_path}")
    print(f"[setup] tree_file: {tree_path}")
    print(f"[setup] prefix: {args.prefix}")
    print(f"[setup] agent_count: {args.agent_count}")
    print(f"[setup] seed: {args.seed}")
    print(f"[setup] field_seed: {args.field_seed}")
    print(f"[setup] max_steps: {args.max_steps}")

    started = time.time()
    proc = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    elapsed = time.time() - started

    if proc.stdout.strip():
        print(proc.stdout.rstrip())
    if proc.returncode != 0:
        if proc.stderr.strip():
            print(proc.stderr.rstrip())
        raise RuntimeError(f"Detailed trial run failed with code {proc.returncode}")

    summary = parse_summary_json(proc.stdout)
    print(f"[done] elapsed_sec={elapsed:.1f}")
    print(f"[done] summary={json.dumps(summary, ensure_ascii=False)}")
    print(f"[done] csv_prefix={args.prefix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())