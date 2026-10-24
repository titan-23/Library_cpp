#!/usr/bin/env python3
"""Recreate the pinned pointer-based ordinary trees for the fixed benchmark.

  python3 test/ds/benchmark/prepare_dynamic_segment_tree_baseline.py /tmp/dst-pointer
  g++ -std=c++17 -O3 -DNDEBUG -I/tmp/dst-pointer -I. \
    '-DDST_BENCH_IMPLEMENTATION="pointer"' \
    test/ds/benchmark/dynamic_segment_tree_benchmark.cpp -o /tmp/dst-pointer-bench

Only the logical domain, overflow-safe midpoint, standalone includes, and an
untimed node_count() inspector differ from the pinned implementation.
"""

import argparse
from pathlib import Path
import subprocess

REVISION = "ae0e71fc1bdfa9041c76b64156a857b7d6817703"
SOURCES = ("dynamic_segment_tree.cpp", "dynamic_segment_tree_init.cpp")


def replace_exact(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise ValueError(f"expected exactly one occurrence: {old!r}")
    return source.replace(old, new)


def prepare(source: str) -> str:
    source = replace_exact(source, "#include <iostream>",
                           "#include <iostream>\n#include <algorithm>\n#include <cstddef>")
    source = replace_exact(source, "this->u = 1ll << bit_length(u_);", "this->u = u_;")
    source = replace_exact(source, "return (l + r) / 2;", "return l + (r - l) / 2;")
    return replace_exact(source, "    T prod(IndexType l, IndexType r) const {", """    std::size_t node_count() const {
        const auto count = [](const auto& self, NodePtr node) -> std::size_t {
            return node ? 1 + self(self, node->left) + self(self, node->right) : 0;
        };
        return count(count, root);
    }

    T prod(IndexType l, IndexType r) const {""")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="output include root")
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[3]
    destination = args.output / "titan_cpplib/ds"
    destination.mkdir(parents=True, exist_ok=True)
    for name in SOURCES:
        source = subprocess.run(
            ["git", "show", f"{REVISION}:titan_cpplib/ds/{name}"],
            cwd=repository, check=True, capture_output=True, encoding="utf-8",
        ).stdout
        (destination / name).write_text(prepare(source), encoding="utf-8")
    print(f"Wrote corrected pointer baselines from {REVISION} to {args.output}")


if __name__ == "__main__":
    main()
