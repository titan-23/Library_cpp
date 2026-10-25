#!/usr/bin/env python3
"""Generate leaf-tree updates that carry inherited actions and reversal.

python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_round2_ops.py \
    --source titan_cpplib/ds/persistent_lazy_wbtree_leaf.cpp \
    --output /tmp/wbt_round2_ops

Writes isolated fused-set, fused-apply, and combined candidates. This script
does not modify the source header. Recomputed internal nodes are constructed
from their new children instead of copying an aggregate that will be replaced.
The operations accept general composable actions; no inverse is required.
"""

import argparse
from pathlib import Path


INHERIT = '''    SizeType inherit_edit_subtree(SizeType node, const F &acc, bool rev, size_t first) {
        if (!node) return 0;
        const bool action = acc != id();
        rev = rev && size_of(node) > 1;
        if (!action && !rev) return node;
        node = own(node, first);
        if (action) push(node, acc);
        if (rev) ma.pool[node].data.rev ^= 1;
        return node;
    }

'''

FUSED_SET = '''    PLTM set(SizeType k, T value) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        auto dfs = [&](auto &&self, SizeType node, SizeType k, F acc, bool rev) -> SizeType {
            if (size_of(node) == 1) {
                // Setting a leaf replaces all inherited actions and its value.
                if (static_cast<size_t>(node) < first) return ma.new_node(value, id());
                ma.pool[node].data.data = value;
                ma.pool[node].data.lazy = id();
                ma.pool[node].data.rev = 0;
                return node;
            }
            rev ^= ma.pool[node].data.rev;
            SizeType left = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
            SizeType right = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
            const SizeType left_size = rev ? size_of(node)-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
            if (ma.pool[node].data.lazy != id()) acc = composition(acc, ma.pool[node].data.lazy);
            if (k < left_size) {
                left = self(self, left, k, acc, rev);
                right = inherit_edit_subtree(right, acc, rev, first);
            } else {
                left = inherit_edit_subtree(left, acc, rev, first);
                right = self(self, right, k-left_size, acc, rev);
            }
            // branch() resets lazy/rev and computes the aggregate exactly once.
            return branch(left, right, static_cast<size_t>(node) >= first ? node : 0);
        };
        return _new(dfs(dfs, root, k, id(), false));
    }'''

FUSED_APPLY = '''    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(f != id())) return _new(root);
        const size_t first = ma.ptr;
        auto dfs = [&](auto &&self, SizeType node, SizeType l, SizeType r, F acc, bool rev) -> SizeType {
            if (!l && r == size_of(node)) {
                node = own(node, first);
                // node.data already includes node.lazy; apply ancestor actions
                // and the new action together without materializing that path.
                push(node, acc != id() ? composition(f, acc) : f);
                if (rev && size_of(node) > 1) ma.pool[node].data.rev ^= 1;
                return node;
            }
            rev ^= ma.pool[node].data.rev;
            SizeType left = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
            SizeType right = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
            const SizeType left_size = rev ? size_of(node)-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
            if (ma.pool[node].data.lazy != id()) acc = composition(acc, ma.pool[node].data.lazy);
            if (l < left_size) left = self(self, left, l, min(r, left_size), acc, rev);
            else left = inherit_edit_subtree(left, acc, rev, first);
            if (r > left_size) right = self(self, right, l > left_size ? l-left_size : 0, r-left_size, acc, rev);
            else right = inherit_edit_subtree(right, acc, rev, first);
            return branch(left, right, static_cast<size_t>(node) >= first ? node : 0);
        };
        return _new(dfs(dfs, root, l, r, id(), false));
    }'''


def replace_method(source, marker, body):
    start = source.index(marker)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[:start] + body + source[end:]


def with_helper(source):
    if "    SizeType inherit_edit_subtree(" in source:
        return source
    marker = "    static PLTM _new("
    if source.count(marker) != 1:
        raise ValueError("expected one _new helper in the leaf header")
    return source.replace(marker, INHERIT + marker, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = with_helper(args.source.read_text())
    if "    SizeType branch(" not in source or "    SizeType size_of(" not in source:
        raise ValueError("source must be a leaf-oriented fast header")
    args.output.mkdir(parents=True, exist_ok=True)
    variants = {
        "leaf_fused_set": replace_method(source, "    PLTM set(", FUSED_SET),
        "leaf_fused_apply": replace_method(source, "    PLTM apply(", FUSED_APPLY),
        "leaf_fused_ops": replace_method(replace_method(source, "    PLTM set(", FUSED_SET), "    PLTM apply(", FUSED_APPLY),
    }
    for name, candidate in variants.items():
        path = args.output / (name + ".hpp")
        path.write_text(candidate)
        print(path)


if __name__ == "__main__":
    main()
