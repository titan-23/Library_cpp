#!/usr/bin/env python3
"""Compare the saved implementation with the current LinkCutTreeSubtree."""

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
    parser.add_argument("--shapes", default="0,1,2,3", help="random,path,star,balanced = 0,1,2,3")
    parser.add_argument("--seeds", default="23,81,2026")
    parser.add_argument("--opt", choices=["O2", "O3"], default="O3")
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[4]
    bench = Path(__file__).with_name("link_cut_tree_subtree_benchmark.cpp").resolve()
    baseline = bench.parent / "fixtures/link_cut_tree_subtree_20261112.cpp"
    flags = ["-std=c++20", "-" + args.opt, "-DNDEBUG"]
    if args.native:
        flags.append("-march=native")
    compiler = subprocess.check_output(["g++", "--version"], text=True).splitlines()[0]
    shapes = list(map(int, args.shapes.split(",")))
    seeds = list(map(int, args.seeds.split(",")))
    shape_names = ["random", "path", "star", "balanced"]
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="lcts-benchmark-") as tmp:
        binaries = {v: str(Path(tmp) / v) for v in ["before", "after"]}
        for version, binary in binaries.items():
            cmd = ["g++", *flags, "-I", str(repo), str(bench), "-o", binary]
            if version == "before":
                cmd.append(f'-DLCTS_HEADER="{baseline}"')
            subprocess.run(cmd, check=True)
        with args.output.open("w", newline="") as output:
            columns = ["compiler", "flags", "version", "n", "q", "shape", "seed",
                       "workload", "seconds", "checksum"]
            writer = csv.DictWriter(output, fieldnames=columns)
            writer.writeheader()
            for case, (shape, seed) in enumerate((s, r) for s in shapes for r in seeds):
                checksums = []
                versions = ["before", "after"] if case % 2 == 0 else ["after", "before"]
                for version in versions:
                    result = subprocess.check_output(
                        [binaries[version], str(args.n), str(args.q), str(shape), str(seed)], text=True)
                    checksum = re.search(r"^checksum (\d+)$", result, re.MULTILINE).group(1)
                    checksums.append(checksum)
                    for line in result.splitlines():
                        match = re.fullmatch(r"(.+?)\s+(\d+\.\d+) s\s+.*", line)
                        if match:
                            writer.writerow(dict(compiler=compiler, flags=" ".join(flags),
                                                 version=version, n=args.n, q=args.q,
                                                 shape=shape_names[shape], seed=seed,
                                                 workload=match[1], seconds=match[2], checksum=checksum))
                    output.flush()
                if checksums[0] != checksums[1]:
                    raise RuntimeError(f"Checksum mismatch: shape={shape}, seed={seed}: {checksums}")
                print(f"N={args.n} Q={args.q} shape={shape_names[shape]} seed={seed}: OK", flush=True)


if __name__ == "__main__":
    main()
