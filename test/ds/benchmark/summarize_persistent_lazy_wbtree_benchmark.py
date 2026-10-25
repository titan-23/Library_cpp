#!/usr/bin/env python3
"""Summarize completed tree benchmarks as CSV, without rerunning benchmarks.

python3 test/ds/benchmark/summarize_persistent_lazy_wbtree_benchmark.py
python3 test/ds/benchmark/summarize_persistent_lazy_wbtree_benchmark.py \
    /tmp/plwbt_before_after.json --output /tmp/plwbt-summary.csv
python3 test/ds/benchmark/summarize_persistent_lazy_wbtree_benchmark.py \
    /tmp/plwbt_fast_comparison.json --output /tmp/plwbt-fast-summary.csv

Defaults to the supported comparison JSON files beside this script and writes
persistent_lazy_wbtree_benchmark.csv there. Incomplete JSON results are skipped.
All times are seconds; memory uses MiB (2**20 bytes). Medians are computed from
successful individual runs. compare_checksums is the runner's overall ok flag,
which includes query and post-measurement validation checksum comparisons.
Fast comparison JSON files are accepted as explicit inputs; default discovery
continues to select only the earlier benchmark comparisons.
"""

import argparse
import csv
import json
from pathlib import Path
import statistics
import sys


PATTERNS = (
    "*_before_after*.json",
    "*_set_stack_vs_recursive.json",
    "*_set_array_vs_recursive.json",
    "*_get_iterative_vs_recursive.json",
    "*_access_set_comparison.json",
    "*_access_get_comparison.json",
)
FIELDS = (
    "source_json", "comparison", "variant", "mode", "workload", "n", "q", "seed",
    "reserve_nodes", "prepare_ranges", "repeats", "median_query_seconds", "median_build_seconds",
    "median_peak_rss_mib", "median_capacity_mib", "median_node_count", "compare_checksums",
)
ACCESSOR_LABELS = {
    "set_stack_vs_recursive": ("set", "stack_set", "recursive_set"),
    "set_array_vs_recursive": ("set", "array_set", "recursive_set"),
    "get_iterative_vs_recursive": ("get", "iterative_get", "recursive_get"),
}


def comparison_name(path):
    for name in (*ACCESSOR_LABELS, "access_set_comparison", "access_get_comparison"):
        if path.stem.endswith(name):
            return name
    index = path.stem.find("before_after")
    if index >= 0:
        return path.stem[index:]
    index = path.stem.find("fast_")
    if index >= 0:
        return path.stem[index:]
    raise ValueError(f"unsupported comparison filename: {path.name}")


def variant_label(data, comparison, variant, hashes):
    if comparison.startswith("fast_"):
        header = data.get("source_headers", {}).get(variant)
        if not header:
            raise ValueError(f"missing source_headers for {variant} in {comparison}")
        return Path(header).stem
    if comparison.startswith("before_after"):
        return {"baseline": "original", "optimized": "improved"}[variant]
    if comparison in ACCESSOR_LABELS:
        _, baseline, optimized = ACCESSOR_LABELS[comparison]
        return baseline if variant == "baseline" else optimized
    method = "set" if comparison == "access_set_comparison" else "get"
    # New runner output records paths; older output can be identified by the
    # same source hashes in the pure-set/get comparisons in this directory.
    header = Path(data.get("source_headers", {}).get(variant, "")).stem
    candidates = ("stack_set", "array_set", "recursive_set") if method == "set" else ("iterative_get", "recursive_get")
    for label in candidates:
        if label in header:
            return label
    digest = data.get("source_sha256", {}).get(variant)
    if digest in hashes[method]:
        return hashes[method][digest]
    raise ValueError(f"cannot identify {variant} {method} implementation in {comparison}; "
                     "include the pure accessor comparison JSON or source_headers metadata")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path)
    parser.add_argument("--directory", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    paths = args.inputs or sorted({path for pattern in PATTERNS for path in args.directory.glob(pattern)})
    if not paths:
        parser.error("no supported benchmark JSON files found")
    records = []
    for path in paths:
        try:
            data = json.loads(path.read_text())
        except json.JSONDecodeError:
            print(f"Skipping incomplete JSON: {path}", file=sys.stderr)
            continue
        if "ok" not in data:
            print(f"Skipping unfinished benchmark: {path}", file=sys.stderr)
            continue
        records.append((path, data, comparison_name(path)))
    hashes = {"set": {}, "get": {}}
    for _, data, comparison in records:
        if comparison in ACCESSOR_LABELS:
            method, baseline, optimized = ACCESSOR_LABELS[comparison]
            for variant, label in (("baseline", baseline), ("optimized", optimized)):
                digest = data.get("source_sha256", {}).get(variant)
                if digest:
                    hashes[method][digest] = label

    rows = []
    for path, data, comparison in records:
        groups = {}
        for run in data.get("runs", []):
            if "exit_code" in run:
                continue
            key = tuple(run[field] for field in ("variant", "mode", "workload", "n", "q", "seed", "reserve_nodes")) + (run.get("prepare_ranges", 0),)
            groups.setdefault(key, []).append(run)
        for key, runs in groups.items():
            variant, mode, workload, n, q, seed, reserve, prepare = key
            row = {
                "source_json": path.name,
                "comparison": comparison,
                "variant": variant_label(data, comparison, variant, hashes),
                "mode": mode,
                "workload": workload,
                "n": n,
                "q": q,
                "seed": seed,
                "reserve_nodes": reserve,
                "prepare_ranges": prepare,
                "repeats": len(runs),
                "median_query_seconds": f"{statistics.median(r['query_seconds'] for r in runs):.9f}",
                "median_build_seconds": f"{statistics.median(r['build_seconds'] for r in runs):.9f}",
                "median_peak_rss_mib": f"{statistics.median(r['peak_rss_bytes'] for r in runs) / 2**20:.6f}",
                "median_capacity_mib": f"{statistics.median(r['allocated_capacity_bytes'] for r in runs) / 2**20:.6f}",
                "median_node_count": statistics.median(r['node_count'] for r in runs),
                "compare_checksums": str(bool(data["ok"])).lower(),
            }
            rows.append(row)
    if not rows:
        parser.error("no completed successful benchmark runs found")
    destination = args.output or args.directory / "persistent_lazy_wbtree_benchmark.csv"
    with destination.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"Wrote {len(rows)} rows to {destination}")


if __name__ == "__main__":
    main()
