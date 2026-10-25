#!/usr/bin/env python3
"""Add a physical-left-subtree size cache to an AoS direct-pool fast header.

python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_left_size.py \
    --source /tmp/blocked_aos_direct_65536_align64.hpp \
    --output /tmp/wbt_left_size.hpp

The input must be the combined-algorithm AoS header before this cache is added.
For the benchmark's int index and sum/count payload, Node grows from 12 to 16
bytes while the combined Slot remains 64 bytes by using existing alignment
padding. Other template instantiations can have different storage sizes.
"""

import argparse
from pathlib import Path


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise ValueError(f"expected exactly one occurrence: {before!r}")
    return source.replace(before, after, 1)


def edit_method(source, marker, transform):
    start = source.index(marker)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[:start] + transform(source[start:end]) + source[end:]


def prepare(source):
    if "SizeType left, right, size, left_size;" in source:
        raise ValueError("input already has the left_size cache")
    source = replace_once(source, "SizeType left, right, size;", "SizeType left, right, size, left_size;")
    source = replace_once(source, "Node() : left(0), right(0), size(0) {}",
                          "Node() : left(0), right(0), size(0), left_size(0) {}")
    source = replace_once(source,
                          "Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s) {}",
                          "Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s), left_size(0) {}")
    source = replace_once(source,
                          "return ma.pool[ma.pool[node].tree.right].tree.size + 1;",
                          "return ma.pool[node].tree.size - ma.pool[node].tree.left_size + (node == 0);")
    source = replace_once(source,
                          "return ma.pool[ma.pool[node].tree.left].tree.size + 1;",
                          "return ma.pool[node].tree.left_size + 1;")
    source = replace_once(source,
                          "ma.pool[node].tree.size = 1 + ma.pool[ma.pool[node].tree.left].tree.size + ma.pool[ma.pool[node].tree.right].tree.size;",
                          "ma.pool[node].tree.left_size = ma.pool[ma.pool[node].tree.left].tree.size;\n"
                          "        ma.pool[node].tree.size = 1 + ma.pool[node].tree.left_size + ma.pool[ma.pool[node].tree.right].tree.size;")
    source = edit_method(source, "    void propagate(", lambda method: replace_once(
        method, "        ma.pool[node].tree.left = l;",
        "        // The cache follows physical pointers; pending rev alone does not change it.\n"
        "        if (rev) ma.pool[node].tree.left_size = ma.pool[node].tree.size - 1 - ma.pool[node].tree.left_size;\n"
        "        ma.pool[node].tree.left = l;"))

    # These methods normalize node before consulting its physical left child.
    for marker in ("    SizeType insert_node(", "    SizeType erase_node(",
                   "    pair<SizeType, SizeType> _split_node(", "    SizeType _split_node_left(",
                   "    SizeType _split_node_right(", "    SizeType reverse_node(",
                   "    PLTM apply(", "    PLTM set("):
        def cache_normalized(method):
            for expression in ("ma.pool[ma.pool[node].tree.left].tree.size",
                               "ma.pool[left].tree.size", "ma.pool[lch].tree.size"):
                method = method.replace(expression, "ma.pool[node].tree.left_size")
            return method
        source = edit_method(source, marker, cache_normalized)
    source = edit_method(source, "    SizeType erase_node(", lambda method: replace_once(
        method, "if (left_size >= ma.pool[right].tree.size)",
        "if (left_size >= ma.pool[node].tree.size-left_size-1)"))

    # Read-only traversal has inherited reversal in addition to node.rev.
    effective_left = "rev ? ma.pool[node].tree.size - 1 - ma.pool[node].tree.left_size : ma.pool[node].tree.left_size"
    for marker in ("    T prod(", "    T get("):
        source = edit_method(source, marker, lambda method: replace_once(
            method, "ma.pool[lch].tree.size", effective_left))
    source = edit_method(source, "    void check(", lambda method: replace_once(
        method, "            assert(s == ma.pool[node].tree.size);",
        "            assert(s == ma.pool[node].tree.size);\n"
        "            assert(ls == ma.pool[node].tree.left_size);"))
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(prepare(args.source.read_text()))
    print(args.output)


if __name__ == "__main__":
    main()
