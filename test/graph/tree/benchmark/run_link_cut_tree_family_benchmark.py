#!/usr/bin/env python3
"""Run each workload in a fresh process; the old pointer trees cannot be cloned."""

import argparse
import csv
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=int, default=200000)
    parser.add_argument("--q", type=int, default=200000)
    parser.add_argument("--shapes", default="0,1,2,3")
    parser.add_argument("--seeds", default="23,81,2026")
    parser.add_argument("--types", default="plain,sum,lazy")
    parser.add_argument("--workloads", default="path,rewire,mixed")
    parser.add_argument("--opt", choices=["O2", "O3"], default="O3")
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[4]
    bench = Path(__file__).with_name("link_cut_tree_family_benchmark.cpp").resolve()
    flags = ["-std=c++20", "-" + args.opt, "-DNDEBUG"]
    if args.native:
        flags.append("-march=native")
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    shapes = list(map(int, args.shapes.split(",")))
    seeds = list(map(int, args.seeds.split(",")))
    types = args.types.split(",")
    workloads = args.workloads.split(",")
    shape_names = ["random", "path", "star", "balanced"]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    cases = [(t, w, s, seed) for t in types for w in workloads for s in shapes for seed in seeds
             if not (t == "plain" and w in ("get", "set", "apply")) and not (t == "sum" and w == "apply")]

    with tempfile.TemporaryDirectory(prefix="lct-family-") as tmp:
        binaries = {v: str(Path(tmp) / v) for v in ["before", "after"]}
        for version, binary in binaries.items():
            cmd = ["g++", *flags, "-I", str(repo), str(bench), "-o", binary]
            if version == "before":
                for key, file in [("PLAIN", "link_cut_tree"), ("SUM", "link_cut_tree_sum"), ("LAZY", "lazy_link_cut_tree")]:
                    cmd.append(f'-DLCT_{key}_HEADER="{bench.parent}/fixtures/{file}_20261112.cpp"')
            subprocess.run(cmd, check=True)
        with args.output.open("w", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=["compiler", "flags", "version", "type", "n", "q",
                                                        "shape", "seed", "workload", "build_seconds",
                                                        "query_seconds", "total_seconds", "rss_kib", "checksum"])
            writer.writeheader()
            for case, (kind, work, shape, seed) in enumerate(cases):
                checksums = []
                versions = ["before", "after"] if case % 2 == 0 else ["after", "before"]
                for version in versions:
                    cmd = ["/usr/bin/time", "-f", "rss_kib=%M", binaries[version], kind, work,
                           str(args.n), str(args.q), str(shape), str(seed)]
                    result = subprocess.run(cmd, text=True, capture_output=True, check=True)
                    build, query, checksum = result.stdout.strip().split(",")
                    checksums.append(checksum)
                    rss = re.search(r"rss_kib=(\d+)", result.stderr).group(1)
                    writer.writerow(dict(compiler=compiler, flags=" ".join(flags), version=version,
                                         type=kind, n=args.n, q=args.q, shape=shape_names[shape], seed=seed,
                                         workload=work, build_seconds=build, query_seconds=query,
                                         total_seconds=float(build) + float(query), rss_kib=rss, checksum=checksum))
                    output.flush()
                if checksums[0] != checksums[1]:
                    raise RuntimeError(f"Checksum mismatch: {kind}, {work}, {shape}, {seed}: {checksums}")
                print(f"{case + 1}/{len(cases)}: {kind} {work} {shape_names[shape]} seed={seed}: OK", flush=True)


if __name__ == "__main__":
    main()
