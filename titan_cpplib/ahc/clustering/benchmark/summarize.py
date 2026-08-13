#!/usr/bin/env python3
import argparse
import csv
import math
import statistics
from collections import defaultdict

MOVE_NAMES = ["relocate", "swap", "cycle", "rebuild"]


def finite_values(rows, key):
    result = []
    for row in rows:
        try:
            value = float(row[key])
        except (KeyError, TypeError, ValueError):
            continue
        if math.isfinite(value):
            result.append(value)
    return result


def percentile(values, probability):
    if not values:
        return ""
    values = sorted(values)
    position = (len(values) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return values[lower]
    return values[lower] * (upper - position) + values[upper] * (position - lower)


def average(values):
    return statistics.fmean(values) if values else ""


def median(values):
    return statistics.median(values) if values else ""


def ratio(numerator, denominator):
    return numerator / denominator if denominator else ""


def summarize_group(rows):
    ok = [row for row in rows if row["status"] == "ok"]
    valid = [row for row in ok if row["valid"] == "1"]
    costs = finite_values(valid, "recalculated_cost")
    times = finite_values(ok, "elapsed_ms")
    initialization_times = finite_values(ok, "initialization_ms")
    search_times = finite_values(ok, "search_ms")
    differences = finite_values(valid, "difference_from_best_percent")
    improvements = finite_values(valid, "improvement_from_initial_percent")
    rand_indices = finite_values(valid, "adjusted_rand_index")
    trials = finite_values(ok, "trials")
    result = {
        "case_path": rows[0]["case_path"],
        "case_name": rows[0]["case_name"],
        "source": rows[0]["source"],
        "experiment_tag": rows[0]["experiment_tag"],
        "compiler": rows[0]["compiler"],
        "method": rows[0]["method"],
        "limit_kind": rows[0]["limit_kind"],
        "size_condition": rows[0]["size_condition"],
        "max_iterations": rows[0]["max_iterations"],
        "balanced_edge_limit": rows[0]["balanced_edge_limit"],
        "time_limit_ms": rows[0]["time_limit_ms"],
        "requested_runs": len(rows),
        "ok_runs": len(ok),
        "valid_runs": len(valid),
        "skipped_runs": sum(row["status"] == "skipped" for row in rows),
        "error_runs": sum(row["status"] == "error" for row in rows),
        "best_cost": min(costs) if costs else "",
        "median_cost": median(costs),
        "p90_cost": percentile(costs, 0.90),
        "median_difference_from_best_percent": median(differences),
        "p90_difference_from_best_percent": percentile(differences, 0.90),
        "median_improvement_from_initial_percent": median(improvements),
        "median_adjusted_rand_index": median(rand_indices),
        "best_elapsed_ms": min(times) if times else "",
        "median_elapsed_ms": median(times),
        "mean_elapsed_ms": average(times),
        "p90_elapsed_ms": percentile(times, 0.90),
        "median_initialization_ms": median(initialization_times),
        "median_search_ms": median(search_times),
        "median_trials": median(trials),
    }
    total_seconds = sum(search_times) / 1000
    for move in MOVE_NAMES:
        attempts = sum(int(row[f"attempts_{move}"]) for row in ok)
        valid_proposals = sum(int(row[f"valid_{move}"]) for row in ok)
        accepted = sum(int(row[f"accepted_{move}"]) for row in ok)
        improvements = sum(int(row[f"improvements_{move}"]) for row in ok)
        improvement_sum = sum(finite_values(ok, f"improvement_sum_{move}"))
        result[f"{move}_attempts_per_second"] = ratio(attempts, total_seconds)
        result[f"{move}_valid_rate"] = ratio(valid_proposals, attempts)
        result[f"{move}_acceptance_rate"] = ratio(accepted, valid_proposals)
        result[f"{move}_improvement_rate"] = ratio(improvements, accepted)
        result[f"{move}_improvement_per_second"] = ratio(improvement_sum, total_seconds)
        result[f"{move}_mean_improvement"] = ratio(improvement_sum, improvements)
    candidate_refreshes = sum(int(row["candidate_refreshes"]) for row in ok)
    rebuild_same_state = sum(int(row["rebuild_same_state"]) for row in ok)
    rebuild_attempts = sum(int(row["attempts_rebuild"]) for row in ok)
    result["candidate_refreshes_per_second"] = ratio(candidate_refreshes, total_seconds)
    result["rebuild_same_state_rate"] = ratio(rebuild_same_state, rebuild_attempts)
    rebuild_three_attempts = sum(int(row["rebuild_three_attempts"]) for row in ok)
    rebuild_three_valid = sum(int(row["rebuild_three_valid"]) for row in ok)
    rebuild_three_accepted = sum(int(row["rebuild_three_accepted"]) for row in ok)
    rebuild_three_improvements = sum(int(row["rebuild_three_improvements"]) for row in ok)
    rebuild_three_improvement_sum = sum(finite_values(ok, "rebuild_three_improvement_sum"))
    rebuild_three_same_state = sum(int(row["rebuild_three_same_state"]) for row in ok)
    result["rebuild_three_attempts_per_second"] = ratio(rebuild_three_attempts, total_seconds)
    result["rebuild_three_valid_rate"] = ratio(rebuild_three_valid, rebuild_three_attempts)
    result["rebuild_three_acceptance_rate"] = ratio(rebuild_three_accepted, rebuild_three_valid)
    result["rebuild_three_improvement_rate"] = ratio(rebuild_three_improvements, rebuild_three_accepted)
    result["rebuild_three_improvement_per_second"] = ratio(rebuild_three_improvement_sum, total_seconds)
    result["rebuild_three_mean_improvement"] = ratio(rebuild_three_improvement_sum, rebuild_three_improvements)
    result["rebuild_three_same_state_rate"] = ratio(rebuild_three_same_state, rebuild_three_attempts)
    return result


def write_summary(input_path, output_path):
    with open(input_path, encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise ValueError("input CSV has no rows")
    groups = defaultdict(list)
    for row in rows:
        key = (
            row["case_path"], row["case_name"], row["source"], row["experiment_tag"],
            row["compiler"], row["method"], row["limit_kind"], row["max_iterations"],
            row["balanced_edge_limit"], row["time_limit_ms"]
        )
        groups[key].append(row)
    summary = [summarize_group(group) for _, group in sorted(groups.items())]
    with open(output_path, "w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv")
    parser.add_argument("output_csv")
    args = parser.parse_args()
    write_summary(args.input_csv, args.output_csv)


if __name__ == "__main__":
    main()
