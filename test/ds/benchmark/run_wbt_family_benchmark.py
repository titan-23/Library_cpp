#!/usr/bin/env python3
"""Compare exact saved baselines with current WB-tree implementations.

python3 test/ds/benchmark/run_wbt_family_benchmark.py --cpu 24 \
    --n 500000 --q 500000 --repeats 3 --output /tmp/wbt-family.json
python3 test/ds/benchmark/run_wbt_family_benchmark.py \
    --n 64 --q 1000 --verify --repeats 1 --output /tmp/wbt-family-smoke.json

Builds and runs are sequential. Mutable families run latest only; persistent
families support latest/history and retain every version. Mixed workloads omit
set() for all families because some exact baselines cannot instantiate it.
Optional corrected baseline headers require an explicit path and label; this
runner never edits or silently repairs a baseline. Both observed-result and
post-measurement state checksums must match. Small --verify runs use a vector
oracle, including a separate mutable replay after timing/RSS capture.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import statistics
import subprocess
import sys
import tempfile


FAMILIES = {
    "wb_tree": 1,
    "wb_tree_seg": 2,
    "lazy_wb_tree": 3,
    "persistent_wbtree": 4,
    "persistent_seg_wbtree": 5,
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def quoted_dependencies(header, root):
    """Hash quoted local includes, searching the including directory then -Iroot."""
    result = {}
    pending = [header.resolve()]
    seen = set()
    include = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        seen.add(path)
        try:
            name = str(path.relative_to(root))
        except ValueError:
            name = str(path)
        result[name] = sha256(path)
        for target in include.findall(path.read_text()):
            for candidate in (path.parent / target, root / target):
                if candidate.is_file():
                    pending.append(candidate.resolve())
                    break
    return dict(sorted(result.items()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--family", choices=("all", *FAMILIES), default="all")
    parser.add_argument("--n", type=int, default=500000)
    parser.add_argument("--q", type=int, default=500000)
    parser.add_argument("--seed", type=int, default=23)
    parser.add_argument("--mode", choices=("latest", "history", "both"), default="both")
    parser.add_argument("--workload", choices=("mixed", "get", "prod", "set"), default="mixed")
    parser.add_argument("--enable-set", action="store_true")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--cpu", type=int)
    parser.add_argument("--baseline-dir", type=Path)
    parser.add_argument("--optimized-dir", type=Path)
    parser.add_argument("--baseline-header", type=Path)
    parser.add_argument("--optimized-header", type=Path)
    parser.add_argument("--baseline-label", default="raw-snapshot")
    parser.add_argument("--baseline-notes")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--cxxflags", default="-std=c++17 -O3 -DNDEBUG")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.repeats <= 0:
        parser.error("--repeats must be positive")
    if args.family == "all" and (args.baseline_header or args.optimized_header):
        parser.error("individual header overrides require --family NAME")
    if args.workload == "set" and not args.enable_set:
        parser.error("--workload set requires --enable-set and compilable baseline set()")
    selected = list(FAMILIES) if args.family == "all" else [args.family]
    if args.workload == "prod" and any(FAMILIES[name] in (1, 4) for name in selected):
        parser.error("prod workload requires wb_tree_seg, lazy_wb_tree or persistent_seg_wbtree")
    if args.workload == "set" and "persistent_wbtree" in selected:
        parser.error("persistent_wbtree has no set() API")
    if args.mode == "history" and any(FAMILIES[name] <= 3 for name in selected):
        parser.error("history-only runs require a persistent family; --mode both skips mutable history")
    if (args.baseline_header or args.baseline_dir) and args.baseline_label == "raw-snapshot":
        parser.error("a custom baseline path requires an explicit --baseline-label describing its provenance")
    if args.cpu is not None:
        if not hasattr(os, "sched_getaffinity") or not hasattr(os, "sched_setaffinity"):
            parser.error("--cpu is unavailable on this platform")
        if args.cpu not in os.sched_getaffinity(0):
            parser.error(f"CPU {args.cpu} is outside the allowed affinity mask")
        os.sched_setaffinity(0, {args.cpu})

    root = Path(__file__).resolve().parents[3]
    source = root / "test/ds/benchmark/wbt_family_benchmark.cpp"
    baseline_dir = args.baseline_dir or root / "test/ds/benchmark/baseline/wbt_family"
    optimized_dir = args.optimized_dir or root / "titan_cpplib/ds"
    compiler = shlex.split(args.cxx)
    flags = shlex.split(args.cxxflags)
    cpu_model = None
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        for line in cpuinfo.read_text().splitlines():
            if line.startswith("model name"):
                cpu_model = line.partition(":")[2].strip()
                break
    output = {
        "compiler": subprocess.check_output(compiler + ["--version"], text=True).splitlines()[0],
        "flags": flags,
        "platform": platform.platform(),
        "machine": platform.machine(),
        "cpu_model": cpu_model or platform.processor() or None,
        "cpu_affinity": sorted(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else None,
        "benchmark_sha256": sha256(source),
        "runner_sha256": sha256(Path(__file__)),
        "baseline_label": args.baseline_label,
        "baseline_notes": args.baseline_notes or (
            "Custom baseline supplied; see explicit label, source paths and hashes."
            if args.baseline_header or args.baseline_dir else
            "Exact saved source; no correctness or performance patches."),
        "parameters": {key: getattr(args, key) for key in ("n", "q", "seed", "mode", "workload", "repeats", "verify", "enable_set")},
        "families": {},
        "runs": [],
        "summaries": [],
    }

    def save():
        if args.output:
            args.output.write_text(json.dumps(output, indent=2) + "\n")

    failed = False
    with tempfile.TemporaryDirectory(prefix="wbt-family-") as build_dir:
        for family in selected:
            kind = FAMILIES[family]
            headers = {
                "baseline": (args.baseline_header or baseline_dir / f"{family}.cpp").resolve(),
                "optimized": (args.optimized_header or optimized_dir / f"{family}.cpp").resolve(),
            }
            for path in headers.values():
                if not path.is_file():
                    parser.error(f"missing header: {path}")
            output["families"][family] = {
                "kind": kind,
                "headers": {role: str(path) for role, path in headers.items()},
                "source_sha256": {role: sha256(path) for role, path in headers.items()},
                "quoted_dependency_sha256": {role: quoted_dependencies(path, root)
                                             for role, path in headers.items()},
            }
            modes = ("latest",) if kind <= 3 else (("latest", "history") if args.mode == "both" else (args.mode,))
            executables = {}
            for role, header in headers.items():
                binary = Path(build_dir) / f"{family}-{role}"
                command = compiler + flags + [
                    "-I", str(root), f"-DWBT_FAMILY_KIND={kind}",
                    f'-DWBT_FAMILY_HEADER="{header}"', f'-DWBT_FAMILY_VARIANT="{role}"',
                    str(source), "-o", str(binary),
                ]
                if args.enable_set:
                    command.append("-DWBT_FAMILY_ENABLE_SET")
                print(f"Compiling {family} {role}", file=sys.stderr, flush=True)
                completed = subprocess.run(command, text=True, capture_output=True)
                if completed.returncode:
                    failed = True
                    output.setdefault("build_errors", []).append({
                        "family": family, "variant": role, "command": command,
                        "exit_code": completed.returncode, "stderr": completed.stderr[-8000:],
                    })
                    print(completed.stderr[-8000:], file=sys.stderr, flush=True)
                    save()
                    break
                executables[role] = binary
            if len(executables) != 2:
                continue
            for mode in modes:
                for repeat in range(args.repeats):
                    order = ("baseline", "optimized") if repeat % 2 == 0 else ("optimized", "baseline")
                    pair = {}
                    for role in order:
                        command = [str(executables[role]), "--n", str(args.n), "--q", str(args.q),
                                   "--seed", str(args.seed), "--mode", mode, "--workload", args.workload]
                        if args.verify:
                            command.append("--verify")
                        print(f"Running {family} {role} {mode} {repeat + 1}/{args.repeats}", file=sys.stderr, flush=True)
                        completed = subprocess.run(command, text=True, capture_output=True)
                        if completed.returncode:
                            failed = True
                            result = {"family": family, "variant": role, "mode": mode, "workload": args.workload,
                                      "repeat": repeat + 1, "exit_code": completed.returncode,
                                      "stderr": completed.stderr[-4000:]}
                            print(f"Failed: {result}", file=sys.stderr, flush=True)
                        else:
                            result = json.loads(completed.stdout)
                            result["repeat"] = repeat + 1
                            pair[role] = result
                            print(f"  {result['query_seconds']:.6f} s, {result['peak_rss_bytes']/2**20:.2f} MiB",
                                  file=sys.stderr, flush=True)
                        if role == "baseline":
                            result["baseline_label"] = args.baseline_label
                        output["runs"].append(result)
                        save()
                    if len(pair) == 2 and any(pair["baseline"][key] != pair["optimized"][key]
                                              for key in ("checksum", "validation_checksum", "operations")):
                        failed = True
                        message = f"checksum/workload mismatch: {family}, {mode}, repeat {repeat + 1}"
                        output.setdefault("errors", []).append(message)
                        print(message, file=sys.stderr, flush=True)
                        save()
            for mode in modes:
                for role in headers:
                    runs = [run for run in output["runs"] if run["family"] == family and run["variant"] == role
                            and run["mode"] == mode and "exit_code" not in run]
                    if runs:
                        summary = {"family": family, "variant": role, "mode": mode,
                                   "workload": args.workload, "successful_runs": len(runs)}
                        for metric in ("build_seconds", "query_seconds", "peak_rss_bytes", "allocated_capacity_bytes", "node_count"):
                            values = [run[metric] for run in runs if run[metric] >= 0]
                            summary["median_" + metric] = statistics.median(values) if values else None
                        output["summaries"].append(summary)
            save()
    output["ok"] = not failed
    save()
    if not args.output:
        print(json.dumps(output, indent=2))
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
