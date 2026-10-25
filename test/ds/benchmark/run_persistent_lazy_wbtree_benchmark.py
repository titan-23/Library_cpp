#!/usr/bin/env python3
"""Build and measure baseline/optimized trees in separate, sequential processes.

python3 test/ds/benchmark/run_persistent_lazy_wbtree_benchmark.py \
    --n 500000 --q 500000 --repeats 3 --output /tmp/plwbt-results.json
python3 test/ds/benchmark/run_persistent_lazy_wbtree_benchmark.py \
    --n 64 --q 1000 --verify --output /tmp/plwbt-small.json
python3 test/ds/benchmark/run_persistent_lazy_wbtree_benchmark.py \
    --baseline-header /tmp/recursive.cpp --optimized-header /tmp/iterative.cpp \
    --workload access --output /tmp/plwbt-access.json
python3 test/ds/benchmark/run_persistent_lazy_wbtree_benchmark.py \
    --baseline-header test/ds/benchmark/baseline/persistent_lazy_wbtree_before_fast.cpp \
    --optimized-header titan_cpplib/ds/persistent_lazy_wbtree.cpp \
    --n 500000 --q 500000 --repeats 3 --output /tmp/plwbt_fast_comparison.json

The preserved baseline is compiled byte-for-byte, without correctness fixes.
Use the same compiler, flags, seed, mode, and reservation for both variants.
Peak RSS includes allocator growth peaks and input/version bookkeeping. The
allocator capacity metric counts allocator storage only, including combined
physical storage when the allocator exposes capacity_bytes(). Verification runs
after timing and RSS capture. Operation operands are generated before timing.
Every run also computes a separate checksum of sampled version aggregates and
point values after capturing metrics, including for the pure-set workload.
For get/prod, --prepare-ranges performs alternating range updates/reversals on
the initial tree before query timing, with a separate RNG and preparation timer.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import statistics
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=int, default=500000)
    parser.add_argument("--q", type=int, default=500000)
    parser.add_argument("--seed", type=int, default=23)
    parser.add_argument("--mode", choices=("latest", "history", "both"), default="both")
    parser.add_argument("--workload", choices=("mixed", "get", "prod", "set", "access"), default="mixed")
    parser.add_argument("--baseline-header", type=Path)
    parser.add_argument("--optimized-header", type=Path)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--reserve", type=int, default=0)
    parser.add_argument("--prepare-ranges", type=int, default=0)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--cxxflags", default="-std=c++17 -O3 -DNDEBUG")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.repeats <= 0:
        parser.error("--repeats must be positive")
    if args.prepare_ranges < 0:
        parser.error("--prepare-ranges must be nonnegative")
    if args.prepare_ranges and args.workload not in ("get", "prod"):
        parser.error("--prepare-ranges requires --workload get or prod")
    root = Path(__file__).resolve().parents[3]
    source = root / "test/ds/benchmark/persistent_lazy_wbtree_benchmark.cpp"
    headers = {
        "baseline": (args.baseline_header or root / "test/ds/benchmark/baseline/persistent_lazy_wbtree.cpp").resolve(),
        "optimized": (args.optimized_header or root / "titan_cpplib/ds/persistent_lazy_wbtree.cpp").resolve(),
    }
    if not args.baseline_header and args.workload in ("set", "access"):
        parser.error("raw baseline set() is broken; pass a corrected --baseline-header for set/access comparisons")
    for path in headers.values():
        if not path.is_file():
            parser.error(f"missing header: {path}")
    compiler = shlex.split(args.cxx)
    flags = shlex.split(args.cxxflags)
    modes = ("latest", "history") if args.mode == "both" else (args.mode,)
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
        "prepare_ranges": args.prepare_ranges,
        "source_headers": {name: str(path) for name, path in headers.items()},
        "benchmark_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "source_sha256": {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in headers.items()},
        "runs": [],
        "summaries": [],
    }

    def save():
        if args.output:
            args.output.write_text(json.dumps(output, indent=2) + "\n")

    failed = False
    with tempfile.TemporaryDirectory(prefix="plwbt-benchmark-") as build_dir:
        executables = {}
        for name, header in headers.items():
            binary = Path(build_dir) / name
            command = compiler + flags + [
                "-I", str(root),
                f'-DPERSISTENT_LAZY_WBTREE_HEADER="{header}"',
                f'-DPERSISTENT_LAZY_WBTREE_BENCH_LABEL="{name}"',
                str(source), "-o", str(binary),
            ]
            if name == "baseline" and not args.baseline_header:
                command.append("-DPLWBT_BENCH_DISABLE_SET")
            print(f"Compiling {name}", file=sys.stderr, flush=True)
            subprocess.run(command, check=True)
            executables[name] = binary
        for mode in modes:
            for repeat in range(args.repeats):
                # Alternate ordering to reduce systematic warm-up/order bias.
                order = ("baseline", "optimized") if repeat % 2 == 0 else ("optimized", "baseline")
                pair = {}
                for name in order:
                    command = [str(executables[name]), "--n", str(args.n), "--q", str(args.q),
                               "--seed", str(args.seed), "--mode", mode,
                               "--workload", args.workload, "--reserve", str(args.reserve),
                               "--prepare-ranges", str(args.prepare_ranges)]
                    if args.verify:
                        command.append("--verify")
                    print(f"Running {name}, {mode}, repeat {repeat + 1}/{args.repeats}",
                          file=sys.stderr, flush=True)
                    completed = subprocess.run(command, text=True, capture_output=True)
                    if completed.returncode:
                        result = {"variant": name, "mode": mode, "workload": args.workload, "repeat": repeat + 1,
                                  "prepare_ranges": args.prepare_ranges,
                                  "exit_code": completed.returncode,
                                  "stderr": completed.stderr[-4000:]}
                        failed = True
                        print(f"Failed: {result}", file=sys.stderr, flush=True)
                    else:
                        result = json.loads(completed.stdout)
                        result["repeat"] = repeat + 1
                        pair[name] = result
                        print(f"  {result['query_seconds']:.6f} s, "
                              f"RSS {result['peak_rss_bytes'] / 2**20:.2f} MiB, "
                              f"{result['node_count']:,} nodes", file=sys.stderr, flush=True)
                    output["runs"].append(result)
                    save()
                if len(pair) == 2:
                    same = pair["baseline"]["checksum"] == pair["optimized"]["checksum"]
                    same &= pair["baseline"]["validation_checksum"] == pair["optimized"]["validation_checksum"]
                    same &= pair["baseline"]["operations"] == pair["optimized"]["operations"]
                    if not same:
                        failed = True
                        output.setdefault("errors", []).append(f"checksum/workload mismatch: {mode}, repeat {repeat + 1}")
                        print(output["errors"][-1], file=sys.stderr, flush=True)

    for mode in modes:
        for name in headers:
            runs = [r for r in output["runs"] if r["variant"] == name and r["mode"] == mode and "exit_code" not in r]
            if not runs:
                continue
            summary = {"variant": name, "mode": mode, "workload": args.workload,
                       "prepare_ranges": args.prepare_ranges, "successful_runs": len(runs)}
            for metric in ("build_seconds", "prepare_seconds", "query_seconds", "peak_rss_bytes",
                           "allocated_capacity_bytes", "nodes_after_prepare", "node_count"):
                summary["median_" + metric] = statistics.median(r[metric] for r in runs)
            output["summaries"].append(summary)
    output["ok"] = not failed
    save()
    if not args.output:
        print(json.dumps(output, indent=2))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
