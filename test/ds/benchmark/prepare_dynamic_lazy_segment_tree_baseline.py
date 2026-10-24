#!/usr/bin/env python3
"""Extract the pinned pointer-based tree and fix it for benchmark comparison.

Run from the repository root:
  python3 test/ds/benchmark/prepare_dynamic_lazy_segment_tree_baseline.py /tmp/dlst_baseline.cpp

Compile the baseline benchmark (the header macro must include literal quotes):
  g++ -std=c++17 -O3 -DNDEBUG -I. -DDLST_BENCH_BASELINE \
    '-DDYNAMIC_LAZY_SEGMENT_TREE_HEADER="/tmp/dlst_baseline.cpp"' \
    test/ds/benchmark/dynamic_lazy_segment_tree_benchmark.cpp -o /tmp/dlst_baseline

Compile the current implementation by omitting both -D options. Invoke each
binary in a separate process with the same benchmark arguments. Node allocation
remains timed; node_count() traverses the baseline tree only after timing ends.
"""

import argparse
from pathlib import Path
import subprocess


REVISION = "ae0e71fc1bdfa9041c76b64156a857b7d6817703"
SOURCE = "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"


def replace_exact(source: str, old: str, new: str, count: int = 1) -> str:
    actual = source.count(old)
    if actual != count:
        raise ValueError(f"expected {count} occurrences, found {actual}: {old!r}")
    return source.replace(old, new)


def prepare(source: str) -> str:
    # Initialize pending actions before composition reads them.
    source = replace_exact(
        source,
        "key(key), data(pow(key, r-l)) {",
        "key(key), data(pow(key, r-l)), lazy(id()) {",
    )
    # Empty ranges always return the monoid identity.
    source = replace_exact(source, "return mapping(f, e());", "return e();")
    # Use the same logical domain and subdivision as the current tree.
    source = replace_exact(
        source,
        "this->u = (IndexType)1 << bit_length(u_);",
        "this->u = u_;",
        count=2,
    )
    # Fix the loop-variable typo and the missing point-access bounds check.
    source = replace_exact(
        source,
        "for (IndexType i = 0; i < u; ++id)",
        "for (IndexType i = 0; i < u; ++i)",
    )
    source = replace_exact(
        source,
        "T get(IndexType k) {\n        return",
        "T get(IndexType k) {\n        assert(0 <= k && k < u);\n        return",
    )
    # Instrument outside the timed loop without changing node layout or updates.
    return replace_exact(
        source,
        "    /// 適当に表示する",
        """    std::size_t node_count() const {
        const auto count = [](const auto& self, NodePtr node) -> std::size_t {
            return node ? 1 + self(self, node->left) + self(self, node->right) : 0;
        };
        return count(count, root);
    }

    /// 適当に表示する""",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="output header path, e.g. /tmp/dlst_baseline.cpp")
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[3]
    source = subprocess.run(
        ["git", "show", f"{REVISION}:{SOURCE}"],
        cwd=repository,
        check=True,
        capture_output=True,
        encoding="utf-8",
    ).stdout
    args.output.write_text(prepare(source), encoding="utf-8")
    print(f"Wrote corrected baseline {REVISION} to {args.output}")


if __name__ == "__main__":
    main()
