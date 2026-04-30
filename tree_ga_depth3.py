#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import random
import subprocess
import sys
import time
from statistics import mean, median, pstdev
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence


FEATURES = [
    "speed",
    "accel",
    "field_delta",
    "neighbor_count",
    "nearest_neighbor_distance",
    "mean_neighbor_distance",
]

ACTION_VALUES = [-1, 0, 1]

FEATURE_THRESHOLD_RANGES = {
    "speed": (0.0, 5.0),
    "accel": (0.0, 8.0),
    "field_delta": (-50.0, 50.0),
    "neighbor_count": (0.0, 12.0),
    "nearest_neighbor_distance": (0.0, 50.0),
    "mean_neighbor_distance": (0.0, 50.0),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Genetic algorithm for a fixed-depth-3 decision tree used by uuv_sim."
    )

    parser.add_argument("--population", type=int, default=24)
    parser.add_argument("--generations", type=int, default=20)
    parser.add_argument("--elite", type=int, default=4)
    parser.add_argument("--tournament-size", type=int, default=3)
    parser.add_argument("--crossover-rate", type=float, default=0.9)
    parser.add_argument("--mutation-rate", type=float, default=0.35)

    parser.add_argument("--feature-mutation-rate", type=float, default=0.20)
    parser.add_argument("--threshold-mutation-rate", type=float, default=0.35)
    parser.add_argument("--threshold-resample-rate", type=float, default=0.15)
    parser.add_argument("--action-mutation-rate", type=float, default=0.30)
    parser.add_argument("--node-reset-rate", type=float, default=0.02)

    parser.add_argument("--threshold-jitter-scale", type=float, default=0.25)
    parser.add_argument("--max-depth", type=int, default=3)

    parser.add_argument("--base-seed", type=int, default=1)
    parser.add_argument("--field-seed", type=int, default=20260407)
    parser.add_argument("--field-count", type=int, default=10)
    parser.add_argument("--trials-per-field", type=int, default=10)
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

    parser.add_argument("--workers", type=int, default=0)
    parser.add_argument("--worker-ratio", type=float, default=0.75)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--output-best-tree", type=str, default="best_tree_depth3.txt")
    parser.add_argument("--output-best-tree-mermaid", type=str, default="best_tree_depth3.mmd")
    parser.add_argument("--log-csv", type=str, default="tree_ga_log.csv")
    parser.add_argument("--overwrite-log", action="store_true")

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


def cpu_workers(worker_ratio: float) -> int:
    logical = os.cpu_count() or 1
    return max(1, int(logical * worker_ratio))


def tree_size(max_depth: int) -> int:
    return (1 << (max_depth + 1)) - 1


def node_depth(index: int) -> int:
    return int(math.floor(math.log2(index + 1)))


def is_leaf_index(index: int, max_depth: int) -> bool:
    return node_depth(index) >= max_depth


def clamp(value: float, lower: float, upper: float) -> float:
    return max(lower, min(upper, value))


def feature_threshold_range(feature: str) -> tuple[float, float]:
    return FEATURE_THRESHOLD_RANGES.get(feature, (-10.0, 10.0))


def random_threshold(feature: str, rng: random.Random) -> float:
    lower, upper = feature_threshold_range(feature)
    return rng.uniform(lower, upper)


def pick_distinct_feature(current: str, rng: random.Random) -> str:
    choices = [feature for feature in FEATURES if feature != current]
    return rng.choice(choices) if choices else current


def mutate_action(current: int, rng: random.Random) -> int:
    choices = [value for value in ACTION_VALUES if value != current]
    return rng.choice(choices) if choices else current


def escape_mermaid_label(text: str) -> str:
    return text.replace('"', "&quot;")


GAIN_NAMES = [
    "avoidance",
    "quark",
    "gradient",
    "drag",
]


def format_gain_action(action: int, gain_name: str) -> str:
    if action > 0:
        prefix = "+"
    elif action < 0:
        prefix = "-"
    else:
        prefix = "0"
    return f"{prefix} {gain_name}"


@dataclass
class TreeNodeGene:
    depth: int
    feature: str | None = None
    threshold: float | None = None
    actions: list[int] = field(default_factory=lambda: [0, 0, 0, 0])

    @property
    def is_leaf(self) -> bool:
        return self.depth >= 3

    def clone(self) -> "TreeNodeGene":
        return TreeNodeGene(
            depth=self.depth,
            feature=self.feature,
            threshold=self.threshold,
            actions=list(self.actions),
        )


@dataclass
class TreeGenome:
    nodes: list[TreeNodeGene]
    max_depth: int = 3

    @classmethod
    def random(cls, rng: random.Random, max_depth: int = 3) -> "TreeGenome":
        nodes: list[TreeNodeGene] = []
        size = tree_size(max_depth)
        for index in range(size):
            depth = node_depth(index)
            if is_leaf_index(index, max_depth):
                actions = [rng.choice(ACTION_VALUES) for _ in range(4)]
                nodes.append(TreeNodeGene(depth=depth, actions=actions))
            else:
                feature = rng.choice(FEATURES)
                threshold = random_threshold(feature, rng)
                nodes.append(TreeNodeGene(depth=depth, feature=feature, threshold=threshold))
        return cls(nodes=nodes, max_depth=max_depth)

    @classmethod
    def default(cls, max_depth: int = 3) -> "TreeGenome":
        rng = random.Random(0)
        genome = cls.random(rng, max_depth=max_depth)
        genome.nodes[0].feature = "field_delta"
        genome.nodes[0].threshold = 0.0
        return genome

    def clone(self) -> "TreeGenome":
        return TreeGenome(nodes=[node.clone() for node in self.nodes], max_depth=self.max_depth)

    def subtree_indices(self, root_index: int) -> list[int]:
        indices: list[int] = []
        stack = [root_index]
        limit = len(self.nodes)
        while stack:
            index = stack.pop()
            if index >= limit:
                continue
            indices.append(index)
            if not self.nodes[index].is_leaf:
                stack.append(2 * index + 2)
                stack.append(2 * index + 1)
        return indices

    def internal_indices(self) -> list[int]:
        return [index for index, node in enumerate(self.nodes) if not node.is_leaf]

    def to_cli_args(self) -> list[str]:
        args: list[str] = []
        for index, node in enumerate(self.nodes):
            if node.is_leaf:
                actions = [str(value) for value in node.actions]
                spec = f"{index},leaf,{actions[0]},{actions[1]},{actions[2]},{actions[3]}"
            else:
                assert node.feature is not None
                assert node.threshold is not None
                left_id = 2 * index + 1
                right_id = 2 * index + 2
                spec = f"{index},split,{node.feature},{node.threshold:.6f},{left_id},{right_id}"
            args.extend(["--tree-node", spec])
        args.extend(["--tree-root", "0"])
        return args

    def write_tree_file(self, path: Path) -> None:
        lines = []
        for index, node in enumerate(self.nodes):
            if node.is_leaf:
                lines.append(
                    f"{index},leaf,{node.actions[0]},{node.actions[1]},{node.actions[2]},{node.actions[3]}"
                )
            else:
                assert node.feature is not None
                assert node.threshold is not None
                left_id = 2 * index + 1
                right_id = 2 * index + 2
                lines.append(
                    f"{index},split,{node.feature},{node.threshold:.6f},{left_id},{right_id}"
                )
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def to_mermaid(self) -> str:
        lines = ["flowchart TD"]

        for index, node in enumerate(self.nodes):
            if node.is_leaf:
                label_lines = [format_gain_action(action, gain_name) for action, gain_name in zip(node.actions, GAIN_NAMES)]
                label = "<br/>".join(label_lines)
            else:
                assert node.feature is not None
                assert node.threshold is not None
                label = f"{node.feature} < {node.threshold:.4f}"

            lines.append(f'    n{index}["{escape_mermaid_label(label)}"]')

        for index, node in enumerate(self.nodes):
            if node.is_leaf:
                continue

            left_id = 2 * index + 1
            right_id = 2 * index + 2
            if left_id < len(self.nodes):
                lines.append(f"    n{index} -- yes --> n{left_id}")
            if right_id < len(self.nodes):
                lines.append(f"    n{index} -- no --> n{right_id}")

        return "\n".join(lines) + "\n"

    def write_mermaid_file(self, path: Path) -> None:
        path.write_text(self.to_mermaid(), encoding="utf-8")

    def pretty(self) -> str:
        rows = []
        for index, node in enumerate(self.nodes):
            if node.is_leaf:
                rows.append(f"{index}: leaf actions={node.actions}")
            else:
                rows.append(f"{index}: split feature={node.feature} threshold={node.threshold:.4f}")
        return "\n".join(rows)

    def crossover_subtree(self, other: "TreeGenome", rng: random.Random) -> "TreeGenome":
        child = self.clone()
        root = rng.choice(self.internal_indices()) if self.internal_indices() else 0
        for index in other.subtree_indices(root):
            child.nodes[index] = other.nodes[index].clone()
        return child

    def mutate(
        self,
        rng: random.Random,
        feature_mutation_rate: float,
        threshold_mutation_rate: float,
        threshold_resample_rate: float,
        action_mutation_rate: float,
        node_reset_rate: float,
        threshold_jitter_scale: float,
    ) -> None:
        for index, node in enumerate(self.nodes):
            if rng.random() < node_reset_rate:
                depth = node.depth
                if node.is_leaf:
                    self.nodes[index] = TreeNodeGene(
                        depth=depth,
                        actions=[rng.choice(ACTION_VALUES) for _ in range(4)],
                    )
                else:
                    feature = rng.choice(FEATURES)
                    self.nodes[index] = TreeNodeGene(
                        depth=depth,
                        feature=feature,
                        threshold=random_threshold(feature, rng),
                    )
                continue

            if node.is_leaf:
                for col in range(4):
                    if rng.random() < action_mutation_rate:
                        node.actions[col] = mutate_action(node.actions[col], rng)
                continue

            assert node.feature is not None
            assert node.threshold is not None

            feature = node.feature
            lower, upper = feature_threshold_range(feature)

            if rng.random() < feature_mutation_rate:
                feature = pick_distinct_feature(feature, rng)
                lower, upper = feature_threshold_range(feature)
                node.feature = feature
                node.threshold = random_threshold(feature, rng)

            if rng.random() < threshold_mutation_rate:
                if rng.random() < threshold_resample_rate:
                    node.threshold = random_threshold(feature, rng)
                else:
                    span = max(upper - lower, 1e-6)
                    jitter = rng.gauss(0.0, span * threshold_jitter_scale)
                    node.threshold = clamp(node.threshold + jitter, lower, upper)


@dataclass
class CandidateScore:
    score: float
    genome: TreeGenome
    summary: dict


@dataclass
class EvalConfig:
    exe_path: Path
    base_seed: int
    field_seed: int
    field_count: int
    trials_per_field: int
    agent_count: int
    max_steps: int
    window: int
    hold: int
    num_gaussians: int
    eps_v: float
    eps_f: float
    use_speed_check: bool
    init_pos_range: float
    init_vel_range: float


def score_summary(summary: dict) -> float:
    improvement = float(summary.get("improvement_from_start", 0.0))
    converged = float(summary.get("converged", 0.0))
    steps_executed = float(summary.get("steps_executed", 0.0))
    best_field_final = float(summary.get("best_field_final", 0.0))
    avg_speed_final = float(summary.get("avg_speed_final", 0.0))
    runtime_ms = float(summary.get("runtime_ms", 0.0))

    score = 0.0
    score += improvement
    score += 5.0 * converged
    score += -0.02 * best_field_final
    score += -0.05 * avg_speed_final
    score += -0.001 * steps_executed
    score += -0.0001 * runtime_ms
    return score


def run_single_evaluation(
    genome: TreeGenome,
    cfg: EvalConfig,
    field_index: int,
    trial_index: int,
) -> tuple[float, dict]:
    field_seed = cfg.field_seed + field_index
    run_seed = cfg.base_seed + field_index * cfg.trials_per_field + trial_index
    cmd = [
        str(cfg.exe_path),
        "--batch-run",
        "--agent-count",
        str(cfg.agent_count),
        "--seed",
        str(run_seed),
        "--field-seed",
        str(field_seed),
        "--max-steps",
        str(cfg.max_steps),
        "--window",
        str(cfg.window),
        "--hold",
        str(cfg.hold),
        "--num-gaussians",
        str(cfg.num_gaussians),
        "--eps-v",
        str(cfg.eps_v),
        "--eps-f",
        str(cfg.eps_f),
        "--init-pos-range",
        str(cfg.init_pos_range),
        "--init-vel-range",
        str(cfg.init_vel_range),
    ]

    if cfg.use_speed_check:
        cmd.append("--use-speed-check")

    cmd.extend(genome.to_cli_args())

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        stderr_tail = proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else ""
        raise RuntimeError(f"evaluation failed: code={proc.returncode}, stderr_tail={stderr_tail}")

    summary = parse_summary_json(proc.stdout)
    summary["eval_field_index"] = field_index
    summary["eval_trial_index"] = trial_index
    summary["eval_field_seed"] = field_seed
    summary["eval_run_seed"] = run_seed
    return score_summary(summary), summary


def evaluate_genome(genome: TreeGenome, cfg: EvalConfig) -> tuple[float, dict]:
    total_score = 0.0
    collected_summaries: list[dict] = []

    for field_index in range(cfg.field_count):
        for trial_index in range(cfg.trials_per_field):
            score, summary = run_single_evaluation(genome, cfg, field_index, trial_index)
            total_score += score
            collected_summaries.append(summary)

    total_runs = max(1, cfg.field_count * cfg.trials_per_field)
    average_score = total_score / total_runs
    merged_summary = dict(collected_summaries[-1]) if collected_summaries else {}
    merged_summary["field_count"] = cfg.field_count
    merged_summary["trials_per_field"] = cfg.trials_per_field
    merged_summary["total_eval_runs"] = total_runs
    merged_summary["mean_score"] = average_score
    return average_score, merged_summary


def tournament_select(population: list[CandidateScore], tournament_size: int, rng: random.Random) -> CandidateScore:
    competitors = rng.sample(population, k=min(tournament_size, len(population)))
    return max(competitors, key=lambda cand: cand.score)


def make_next_generation(
    evaluated: list[CandidateScore],
    cfg: argparse.Namespace,
    rng: random.Random,
) -> list[TreeGenome]:
    evaluated = sorted(evaluated, key=lambda cand: cand.score, reverse=True)
    next_population: list[TreeGenome] = [cand.genome.clone() for cand in evaluated[: cfg.elite]]

    while len(next_population) < cfg.population:
        parent_a = tournament_select(evaluated, cfg.tournament_size, rng).genome
        child = parent_a.clone()

        if rng.random() < cfg.crossover_rate and len(evaluated) > 1:
            parent_b = tournament_select(evaluated, cfg.tournament_size, rng).genome
            if parent_b is parent_a and len(evaluated) > 1:
                parent_b = rng.choice(evaluated).genome
            child = parent_a.crossover_subtree(parent_b, rng)

        if rng.random() < cfg.mutation_rate:
            child.mutate(
                rng=rng,
                feature_mutation_rate=cfg.feature_mutation_rate,
                threshold_mutation_rate=cfg.threshold_mutation_rate,
                threshold_resample_rate=cfg.threshold_resample_rate,
                action_mutation_rate=cfg.action_mutation_rate,
                node_reset_rate=cfg.node_reset_rate,
                threshold_jitter_scale=cfg.threshold_jitter_scale,
            )

        next_population.append(child)

    return next_population[: cfg.population]


def ensure_csv_writer(path: Path, overwrite: bool) -> tuple[csv.DictWriter, object]:
    fieldnames = [
        "row_type",
        "generation",
        "rank",
        "population_size",
        "score",
        "score_mean",
        "score_median",
        "score_std",
        "score_min",
        "score_max",
        "best_rank",
        "best_tree_summary",
        "mean_score",
        "field_count",
        "trials_per_field",
        "total_eval_runs",
        "agent_count",
        "seed",
        "field_seed",
        "eval_field_index",
        "eval_trial_index",
        "eval_field_seed",
        "eval_run_seed",
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
        "tree_summary",
    ]

    mode = "w" if overwrite else "a"
    file_obj = path.open(mode, newline="", encoding="utf-8")
    writer = csv.DictWriter(file_obj, fieldnames=fieldnames)
    if overwrite or path.stat().st_size == 0:
        writer.writeheader()
        file_obj.flush()
    return writer, file_obj


def evaluate_population(
    population: list[TreeGenome],
    cfg: EvalConfig,
    workers: int,
) -> list[CandidateScore]:
    results: list[CandidateScore] = []
    with ThreadPoolExecutor(max_workers=workers) as executor:
        future_map = {executor.submit(evaluate_genome, genome, cfg): genome for genome in population}
        for future in as_completed(future_map):
            genome = future_map[future]
            score, summary = future.result()
            results.append(CandidateScore(score=score, genome=genome, summary=summary))
    return results


def summarize_candidate(candidate: CandidateScore) -> dict:
    summary = dict(candidate.summary)
    summary["row_type"] = "candidate"
    summary["score"] = candidate.score
    return summary


def summarize_generation(evaluated: list[CandidateScore], generation: int) -> dict:
    scores = [candidate.score for candidate in evaluated]
    improvements = [float(candidate.summary.get("improvement_from_start", 0.0)) for candidate in evaluated]
    converged_values = [float(candidate.summary.get("converged", 0.0)) for candidate in evaluated]
    steps_values = [float(candidate.summary.get("steps_executed", 0.0)) for candidate in evaluated]
    runtime_values = [float(candidate.summary.get("runtime_ms", 0.0)) for candidate in evaluated]
    best_field_values = [float(candidate.summary.get("best_field_final", 0.0)) for candidate in evaluated]
    mean_field_values = [float(candidate.summary.get("mean_field_final", 0.0)) for candidate in evaluated]
    swarm_radius_values = [float(candidate.summary.get("swarm_radius_final", 0.0)) for candidate in evaluated]
    avg_speed_values = [float(candidate.summary.get("avg_speed_final", 0.0)) for candidate in evaluated]

    best_candidate = max(evaluated, key=lambda cand: cand.score)
    best_rank = next(index for index, cand in enumerate(evaluated, start=1) if cand is best_candidate)

    return {
        "row_type": "generation",
        "generation": generation,
        "rank": "",
        "population_size": len(evaluated),
        "score": best_candidate.score,
        "score_mean": mean(scores),
        "score_median": median(scores),
        "score_std": pstdev(scores) if len(scores) > 1 else 0.0,
        "score_min": min(scores),
        "score_max": max(scores),
        "best_rank": best_rank,
        "best_tree_summary": best_candidate.genome.pretty().replace("\n", " | "),
        "mean_score": best_candidate.summary.get("mean_score", best_candidate.score),
        "field_count": best_candidate.summary.get("field_count", 0),
        "trials_per_field": best_candidate.summary.get("trials_per_field", 0),
        "total_eval_runs": best_candidate.summary.get("total_eval_runs", 0),
        "agent_count": best_candidate.summary.get("agent_count", 0),
        "seed": best_candidate.summary.get("seed", 0),
        "field_seed": best_candidate.summary.get("field_seed", 0),
        "eval_field_index": best_candidate.summary.get("eval_field_index", 0),
        "eval_trial_index": best_candidate.summary.get("eval_trial_index", 0),
        "eval_field_seed": best_candidate.summary.get("eval_field_seed", 0),
        "eval_run_seed": best_candidate.summary.get("eval_run_seed", 0),
        "max_steps": best_candidate.summary.get("max_steps", 0),
        "steps_executed": mean(steps_values),
        "converged": mean(converged_values),
        "converge_step": best_candidate.summary.get("converge_step", -1),
        "best_field_final": mean(best_field_values),
        "mean_field_final": mean(mean_field_values),
        "best_field_min_over_run": min(float(candidate.summary.get("best_field_min_over_run", 0.0)) for candidate in evaluated),
        "improvement_from_start": mean(improvements),
        "swarm_radius_final": mean(swarm_radius_values),
        "avg_speed_final": mean(avg_speed_values),
        "runtime_ms": mean(runtime_values),
    }


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parent

    if args.population <= 0:
        raise ValueError("--population must be > 0")
    if args.generations <= 0:
        raise ValueError("--generations must be > 0")
    if args.elite < 0:
        raise ValueError("--elite must be >= 0")
    if args.max_depth != 3:
        raise ValueError("This prototype is intentionally fixed to depth 3. Keep --max-depth 3.")
    if args.field_count <= 0:
        raise ValueError("--field-count must be > 0")
    if args.trials_per_field <= 0:
        raise ValueError("--trials-per-field must be > 0")

    if args.workers > 0:
        workers = args.workers
    else:
        workers = cpu_workers(args.worker_ratio)

    exe_path = detect_executable(root)
    if exe_path is None or not args.skip_build:
        print("[setup] configuring and building project...")
        exe_path = ensure_build(root)

    print(f"[setup] executable: {exe_path}")
    print(f"[setup] workers: {workers}")
    print(f"[setup] population: {args.population}")
    print(f"[setup] generations: {args.generations}")
    print(f"[setup] field_count: {args.field_count}")
    print(f"[setup] trials_per_field: {args.trials_per_field}")
    print(f"[setup] total_eval_runs: {args.field_count * args.trials_per_field}")

    eval_cfg = EvalConfig(
        exe_path=exe_path,
        base_seed=args.base_seed,
        field_seed=args.field_seed,
        field_count=args.field_count,
        trials_per_field=args.trials_per_field,
        agent_count=args.agent_count,
        max_steps=args.max_steps,
        window=args.window,
        hold=args.hold,
        num_gaussians=args.num_gaussians,
        eps_v=args.eps_v,
        eps_f=args.eps_f,
        use_speed_check=bool(args.use_speed_check),
        init_pos_range=args.init_pos_range,
        init_vel_range=args.init_vel_range,
    )

    rng = random.Random(args.base_seed)

    population = [TreeGenome.default(max_depth=args.max_depth)]
    while len(population) < args.population:
        population.append(TreeGenome.random(rng, max_depth=args.max_depth))

    log_path = (root / args.log_csv).resolve()
    writer, log_file = ensure_csv_writer(log_path, overwrite=bool(args.overwrite_log))

    best_overall: CandidateScore | None = None
    started = time.time()

    try:
        for generation in range(1, args.generations + 1):
            print(f"[gen {generation}] evaluating population...")
            evaluated = evaluate_population(population, eval_cfg, workers=workers)
            evaluated.sort(key=lambda cand: cand.score, reverse=True)

            current_best = evaluated[0]
            if best_overall is None or current_best.score > best_overall.score:
                best_overall = CandidateScore(
                    score=current_best.score,
                    genome=current_best.genome.clone(),
                    summary=dict(current_best.summary),
                )

            for rank, candidate in enumerate(evaluated[: min(5, len(evaluated))], start=1):
                row = summarize_candidate(candidate)
                row.update(
                    {
                        "generation": generation,
                        "rank": rank,
                        "tree_summary": candidate.genome.pretty().replace("\n", " | "),
                    }
                )
                writer.writerow(row)

            generation_row = summarize_generation(evaluated, generation)
            writer.writerow(generation_row)
            log_file.flush()

            print(
                f"[gen {generation}] best_score={current_best.score:.6f} "
                f"mean_score={generation_row['score_mean']:.6f} "
                f"score_std={generation_row['score_std']:.6f} "
                f"converged_mean={generation_row['converged']:.3f} "
                f"improvement_mean={generation_row['improvement_from_start']:.6f}"
            )

            if generation < args.generations:
                population = make_next_generation(evaluated, args, rng)

    finally:
        log_file.flush()
        log_file.close()

    elapsed = time.time() - started
    if best_overall is None:
        raise RuntimeError("GA completed without any evaluated candidate")

    best_tree_path = (root / args.output_best_tree).resolve()
    best_overall.genome.write_tree_file(best_tree_path)
    best_tree_mermaid_path = (root / args.output_best_tree_mermaid).resolve()
    best_overall.genome.write_mermaid_file(best_tree_mermaid_path)

    print(f"[done] elapsed_sec={elapsed:.1f}")
    print(f"[done] best_score={best_overall.score:.6f}")
    print(f"[done] best_tree={best_tree_path}")
    print(f"[done] best_tree_mermaid={best_tree_mermaid_path}")
    print(best_overall.genome.pretty())
    print(json.dumps(best_overall.summary, ensure_ascii=False))

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("[fatal] interrupted")
        raise SystemExit(130)
    except Exception as exc:
        print(f"[fatal] {exc}")
        raise SystemExit(1)