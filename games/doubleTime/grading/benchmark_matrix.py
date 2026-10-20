#!/usr/bin/env python3

import argparse
import csv
import math
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_values(value):
    values = [int(item) for item in value.split(",") if item]
    if not values:
        raise argparse.ArgumentTypeError("expected a comma-separated list")
    return values


def main():
    parser = argparse.ArgumentParser(description="Run the Double Time grading benchmark matrix.")
    parser.add_argument("--binary", type=Path)
    parser.add_argument(
        "--clients", type=parse_values, default=[1, 2, 4],
        help="client counts to test",
    )
    parser.add_argument(
        "--platforms", type=parse_values, default=[0, 16, 96],
        help="additional simulated platforms beyond the four level platforms",
    )
    parser.add_argument(
        "--latency-ms", type=int, default=100,
        help="latency used by the latency experiment",
    )
    parser.add_argument("--iterations", type=int, default=100000)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, default=Path("double_time_benchmark_matrix.csv"))
    parser.add_argument("--summary-output", type=Path)
    args = parser.parse_args()

    repository_root = Path(__file__).resolve().parents[3]
    binary = args.binary or repository_root / "out/build/macos-clang/bin/Debug/svanes_game_double_time_grading_benchmark"
    binary = binary.resolve()
    if not binary.is_file():
        raise SystemExit(f"Benchmark executable not found: {binary}")
    if args.iterations <= 0 or args.runs <= 0:
        raise SystemExit("Iterations and runs must be positive.")
    if any(value <= 0 for value in args.clients):
        raise SystemExit("Client counts must be positive.")
    if any(value < 0 for value in args.platforms):
        raise SystemExit("Additional platform counts must be non-negative.")

    experiments = ("normal", "latency", "disruption")
    strategies = ("input", "client_transform")
    condition_count = (
        len(experiments) * len(strategies) * len(args.clients)
        * len(args.platforms)
    )
    completed = 0
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(
        prefix=".double-time-benchmark-", dir=str(args.output.parent)
    ) as temporary_directory:
        temporary_directory = Path(temporary_directory)
        all_rows = []
        with args.output.open("w", newline="") as output_file:
            writer = None
            for experiment in experiments:
                for strategy in strategies:
                    for clients in args.clients:
                        for extra_platforms in args.platforms:
                            total_platforms = 4 + extra_platforms
                            latency = args.latency_ms if experiment == "latency" else 0
                            completed += 1
                            print(
                                f"[{completed}/{condition_count}] {experiment} | "
                                f"strategy={strategy} | clients={clients} | "
                                f"platforms={total_platforms} (+{extra_platforms}) | "
                                f"latency={latency}ms",
                                flush=True,
                            )
                            trial_output = temporary_directory / f"{completed}.csv"
                            subprocess.run(
                                [
                                    str(binary),
                                    "--experiment",
                                    experiment,
                                    "--strategy",
                                    strategy,
                                    "--clients",
                                    str(clients),
                                    "--platforms",
                                    str(total_platforms),
                                    "--latency-ms",
                                    str(latency),
                                    "--iterations",
                                    str(args.iterations),
                                    "--runs",
                                    str(args.runs),
                                    "--output",
                                    str(trial_output),
                                ],
                                check=True,
                            )
                            with trial_output.open(newline="") as trial_file:
                                reader = csv.DictReader(trial_file)
                                if writer is None:
                                    writer = csv.DictWriter(output_file, fieldnames=reader.fieldnames)
                                    writer.writeheader()
                                trial_rows = list(reader)
                                writer.writerows(trial_rows)
                                all_rows.extend(trial_rows)

    summary_output = args.summary_output or args.output.with_name(
        f"{args.output.stem}_summary.csv"
    )
    summary_output.parent.mkdir(parents=True, exist_ok=True)
    numeric_metrics = (
        "total_loop_ms",
        "max_client_server_player_error",
        "max_client_server_platform_error",
        "max_client_player_error",
        "max_client_platform_error",
        "max_timeline_error",
    )
    groups = {}
    for row in all_rows:
        key = tuple(row[field] for field in ("experiment", "strategy", "clients", "platforms", "latency_ms"))
        groups.setdefault(key, []).append(row)

    summary_fields = ["experiment", "strategy", "clients", "platforms", "latency_ms", "runs"]
    for metric in numeric_metrics:
        summary_fields.extend((f"{metric}_mean", f"{metric}_variance", f"{metric}_stddev", f"{metric}_worst"))
    summary_fields.extend(("lost_actions_mean", "lost_actions_variance", "lost_actions_worst", "permanent_divergence_runs"))

    with summary_output.open("w", newline="") as summary_file:
        writer = csv.DictWriter(summary_file, fieldnames=summary_fields)
        writer.writeheader()
        for key in sorted(groups):
            group = groups[key]
            summary = dict(zip(("experiment", "strategy", "clients", "platforms", "latency_ms"), key))
            summary["runs"] = len(group)
            for metric in numeric_metrics:
                values = [float(row[metric]) for row in group]
                mean = sum(values) / len(values)
                variance = sum((value - mean) ** 2 for value in values) / max(1, len(values) - 1)
                summary[f"{metric}_mean"] = mean
                summary[f"{metric}_variance"] = variance
                summary[f"{metric}_stddev"] = math.sqrt(variance)
                summary[f"{metric}_worst"] = max(values)
            lost_actions = [float(row["lost_actions"]) for row in group]
            lost_mean = sum(lost_actions) / len(lost_actions)
            lost_variance = sum((value - lost_mean) ** 2 for value in lost_actions) / max(1, len(lost_actions) - 1)
            summary["lost_actions_mean"] = lost_mean
            summary["lost_actions_variance"] = lost_variance
            summary["lost_actions_worst"] = max(lost_actions)
            summary["permanent_divergence_runs"] = sum(
                int(row["permanently_divergent"]) for row in group
            )
            writer.writerow(summary)

    print(f"Wrote {condition_count * args.runs} trial rows to {args.output}")
    print(f"Wrote {len(groups)} condition summaries to {summary_output}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
