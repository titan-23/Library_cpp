#!/usr/bin/env python3
"""Write side-by-side CSV medians from family benchmark JSON files.

Ratios require a completed artifact with ok=true and matching, successful paired
runs. Failed or incomplete artifacts retain raw medians but have blank ratios.
"""

import argparse
import csv
import json
from pathlib import Path
import statistics
import sys


def median(runs, key, scale=1):
    values = [run[key] / scale for run in runs if key in run and run[key] >= 0]
    return statistics.median(values) if values else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", type=Path, nargs="+")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    columns = [
        "source", "artifact_ok", "family", "mode", "workload", "n", "q", "seed",
        "baseline_label", "paired_repeats", "checksums_match",
        "baseline_query_seconds", "optimized_query_seconds", "speedup",
        "baseline_build_seconds", "optimized_build_seconds",
        "baseline_peak_rss_mib", "optimized_peak_rss_mib", "rss_ratio",
        "baseline_capacity_mib", "optimized_capacity_mib",
        "baseline_nodes", "optimized_nodes",
    ]
    stream = args.output.open("w", newline="") if args.output else sys.stdout
    try:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        for source in args.inputs:
            artifact = json.loads(source.read_text())
            artifact_ok = artifact.get("ok") is True
            parameters = artifact.get("parameters", {})
            groups = {}
            for run in artifact.get("runs", []):
                key = tuple(run.get(field, parameters.get(field))
                            for field in ("family", "mode", "workload", "n", "q", "seed"))
                groups.setdefault(key, []).append(run)
            for key, runs in groups.items():
                row = dict(zip(("family", "mode", "workload", "n", "q", "seed"), key))
                row.update(source=str(source), artifact_ok=artifact_ok,
                           baseline_label=artifact.get("baseline_label", "unknown"))
                by_role = {
                    role: [run for run in runs if run["variant"] == role and "exit_code" not in run]
                    for role in ("baseline", "optimized")
                }
                by_repeat = {role: {run["repeat"]: run for run in values}
                             for role, values in by_role.items()}
                repeats = set(by_repeat["baseline"]) & set(by_repeat["optimized"])
                same = bool(repeats) and len(repeats) == parameters.get("repeats", len(repeats))
                same &= all("exit_code" not in run for run in runs)
                same &= set(by_repeat["baseline"]) == set(by_repeat["optimized"])
                same &= all(len(by_role[role]) == len(by_repeat[role]) for role in by_role)
                same &= all(by_repeat["baseline"][repeat][field] == by_repeat["optimized"][repeat][field]
                            for repeat in repeats for field in ("checksum", "validation_checksum", "operations"))
                row.update(paired_repeats=len(repeats), checksums_match=same)
                for role, values in by_role.items():
                    row[role + "_query_seconds"] = median(values, "query_seconds")
                    row[role + "_build_seconds"] = median(values, "build_seconds")
                    row[role + "_peak_rss_mib"] = median(values, "peak_rss_bytes", 2**20)
                    row[role + "_capacity_mib"] = median(values, "allocated_capacity_bytes", 2**20)
                    row[role + "_nodes"] = median(values, "node_count")
                for result, numerator, denominator in (
                    ("speedup", "baseline_query_seconds", "optimized_query_seconds"),
                    ("rss_ratio", "optimized_peak_rss_mib", "baseline_peak_rss_mib"),
                ):
                    row[result] = (row[numerator] / row[denominator]
                                   if artifact_ok and same and row[numerator] is not None and row[denominator] else None)
                writer.writerow(row)
    finally:
        if args.output:
            stream.close()


if __name__ == "__main__":
    main()
