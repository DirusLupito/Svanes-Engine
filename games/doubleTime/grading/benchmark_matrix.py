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
        "--clients", type=parse_values, default=[10, 20, 40],
        help="client counts to test",
    )
    parser.add_argument(
        "--platforms", type=parse_values, default=[4, 100, 500],
        help="platform counts for the moving-platform experiment",
    )
    parser.add_argument(
        "--latency-values", type=parse_values, default=[250, 500, 1000],
        help="maximum latency spikes for the latency experiment",
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
    if any(value <= 0 for value in args.platforms):
        raise SystemExit("Platform counts must be positive.")

    experiments = (
        "client_count",
        "platform_count",
        "latency",
        "latency_max_load",
    )
    network_strategies = ("input", "client_transform")
    condition_count = len(network_strategies) * (
        len(args.clients) + len(args.platforms) + 2 * len(args.latency_values)
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
            condition_index = 0
            condition_sets = {
                "client_count": [(clients, 20, 0, 0)
                                  for clients in args.clients],
                "platform_count": [(2, platforms, 0, 0)
                                    for platforms in args.platforms],
        "latency": [(2, 20, latency, 0)
                            for latency in args.latency_values],
                "latency_max_load": [(40, 500, latency, 0)
                                      for latency in args.latency_values],
            }
            for experiment in experiments:
                strategies = network_strategies
                for strategy in strategies:
                    for clients, platforms, latency, disruption in condition_sets[experiment]:
                        condition_index += 1
                        print(
                            f"[{condition_index}/{condition_count}] {experiment} | "
                            f"strategy={strategy} | clients={clients} | platforms={platforms} | "
                            f"latency={latency}ms | disruption={disruption}ms",
                            flush=True,
                        )
                        trial_output = temporary_directory / f"{condition_index}.csv"
                        subprocess.run(
                            [str(binary), "--experiment", experiment, "--strategy", strategy,
                             "--clients", str(clients), "--platforms", str(platforms),
                             "--latency-ms", str(latency), "--disruption-ms", str(disruption),
                             "--iterations", str(args.iterations), "--runs", str(args.runs),
                             "--output", str(trial_output)], check=True)
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
        "worst_loop_us",
        "average_timeline_error",
        "max_timeline_error",
        "timeline_error_variance",
    )
    groups = {}
    for row in all_rows:
        key = tuple(row[field] for field in ("experiment", "strategy", "clients", "platforms", "latency_ms", "disruption_ms"))
        groups.setdefault(key, []).append(row)

    summary_fields = ["experiment", "strategy", "clients", "platforms", "latency_ms", "disruption_ms", "runs"]
    for metric in numeric_metrics:
        summary_fields.append(f"{metric}_mean")
        if metric != "worst_loop_us":
            summary_fields.extend((f"{metric}_variance", f"{metric}_stddev"))
        summary_fields.append(f"{metric}_worst")

    with summary_output.open("w", newline="") as summary_file:
        writer = csv.DictWriter(summary_file, fieldnames=summary_fields)
        writer.writeheader()
        for key in sorted(groups):
            group = groups[key]
            summary = dict(zip(("experiment", "strategy", "clients", "platforms", "latency_ms", "disruption_ms"), key))
            summary["runs"] = len(group)
            for metric in numeric_metrics:
                values = [float(row[metric]) for row in group]
                mean = sum(values) / len(values)
                variance = sum((value - mean) ** 2 for value in values) / max(1, len(values) - 1)
                summary[f"{metric}_mean"] = mean
                if metric != "worst_loop_us":
                    summary[f"{metric}_variance"] = variance
                    summary[f"{metric}_stddev"] = math.sqrt(variance)
                summary[f"{metric}_worst"] = max(values)
            writer.writerow(summary)

    print(f"Wrote {condition_count * args.runs} trial rows to {args.output}")
    print(f"Wrote {len(groups)} condition summaries to {summary_output}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
