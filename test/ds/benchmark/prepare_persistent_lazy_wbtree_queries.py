#!/usr/bin/env python3
"""Generate get()/prod() traversal candidates without changing the source header."""

import argparse
from pathlib import Path


def replace_function(source: str, signature: str, replacement: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[:start] + replacement + source[end:]


def reflected_get(leaf: bool, conditional_move: bool, prefetch: bool, mask_select: bool = False,
                  cached_lazy: bool = False, separate_leaves: bool = False) -> str:
    lines = [
        "    T get(SizeType k) const {",
        "        assert(0 <= k && k < len());",
        "        SizeType node = root;",
        "        const F identity = id();",
        "        F acc = identity;",
        "        while (true) {",
    ]
    if separate_leaves:
        lines += [
            "            if (ma.is_leaf(node)) {",
            "                return acc != identity ? mapping(acc, ma.value(node)) : ma.value(node);",
            "            }",
            "            const auto &slot = ma.pool[ma.record_index(node)];",
        ]
    else:
        lines += ["            const auto &slot = ma.pool[node];"]
    if leaf and not separate_leaves:
        lines += [
            "            if (slot.tree.size == 1) {",
            "                return acc != identity ? mapping(acc, slot.data.data) : slot.data.data;",
            "            }",
        ]
    lines += [
        "            // Reflect the local rank before choosing physical children.",
        "            // The next node handles its own pending reversal independently.",
        "            if (slot.data.rev) k = slot.tree.size - 1 - k;",
        "            const SizeType left_size = slot.tree.left_size;",
    ]
    if not leaf:
        lines += [
            "            if (k == left_size) {",
            "                return acc != identity ? mapping(acc, slot.data.key) : slot.data.key;",
            "            }",
        ]
    lazy_test = "slot.data.has_lazy" if cached_lazy else "slot.data.lazy != identity"
    lines += [f"            if ({lazy_test}) acc = composition(acc, slot.data.lazy);"]
    if prefetch:
        lines += ["#if defined(__GNUC__) || defined(__clang__)"]
        for side in ("left", "right"):
            if separate_leaves:
                lines += [
                    f"            if (ma.is_leaf(slot.tree.{side}))",
                    f"                __builtin_prefetch(&ma.leaves[ma.record_index(slot.tree.{side})], 0, 1);",
                    "            else",
                    f"                __builtin_prefetch(&ma.pool[ma.record_index(slot.tree.{side})], 0, 1);",
                ]
            else:
                lines += [f"            __builtin_prefetch(&ma.pool[slot.tree.{side}], 0, 1);"]
        lines += ["#endif"]
    right_begin = "left_size" if leaf else "left_size + 1"
    if mask_select:
        comparison = ">=" if leaf else ">"
        lines += [
            "            using Unsigned = typename std::make_unsigned<SizeType>::type;",
            f"            const Unsigned mask = Unsigned(0) - Unsigned(k {comparison} left_size);",
            "            const Unsigned left = static_cast<Unsigned>(slot.tree.left);",
            "            const Unsigned right = static_cast<Unsigned>(slot.tree.right);",
            "            node = static_cast<SizeType>(left ^ ((left ^ right) & mask));",
            f"            k -= static_cast<SizeType>(static_cast<Unsigned>({right_begin}) & mask);",
        ]
    elif conditional_move:
        comparison = ">=" if leaf else ">"
        lines += [
            f"            const bool right = k {comparison} left_size;",
            "            node = right ? slot.tree.right : slot.tree.left;",
            f"            k -= right ? {right_begin} : 0;",
        ]
    else:
        lines += [
            "            if (k < left_size) {",
            "                node = slot.tree.left;",
            "            } else {",
            f"                k -= {right_begin};",
            "                node = slot.tree.right;",
            "            }",
        ]
    lines += ["        }", "    }"]
    return "\n".join(lines)


def reflected_prod(leaf: bool, cached_lazy: bool = False, separate_leaves: bool = False) -> str:
    lazy_test = "slot.data.has_lazy" if cached_lazy else "slot.data.lazy != identity"
    lines = [
        "    T prod(SizeType l, SizeType r) const {",
        "        assert(0 <= l && l <= r && r <= len());",
        "        if (l == r) return e();",
        "        const F identity = id();",
        "        auto dfs = [&](auto &&self, SizeType node, SizeType l, SizeType r, F acc) -> T {",
    ]
    if separate_leaves:
        lines += [
            "            if (ma.is_leaf(node)) {",
            "                return acc != identity ? mapping(acc, ma.value(node)) : ma.value(node);",
            "            }",
            "            const auto &slot = ma.pool[ma.record_index(node)];",
        ]
    else:
        lines += ["            const auto &slot = ma.pool[node];"]
    lines += [
        "            if (l == 0 && r == slot.tree.size) {",
        "                return acc != identity ? mapping(acc, slot.data.data) : slot.data.data;",
        "            }",
        "            // reverse() already requires reversal-invariant aggregates.",
        "            // Reflecting the interval therefore permits physical-order folding.",
        "            if (slot.data.rev) {",
        "                const SizeType old_l = l;",
        "                l = slot.tree.size - r;",
        "                r = slot.tree.size - old_l;",
        "            }",
        "            const SizeType left_size = slot.tree.left_size;",
    ]
    if leaf:
        lines += [
            f"            if ({lazy_test}) acc = composition(acc, slot.data.lazy);",
            "            if (r <= left_size) return self(self, slot.tree.left, l, r, acc);",
            "            if (l >= left_size) return self(self, slot.tree.right, l-left_size, r-left_size, acc);",
            "            return op(self(self, slot.tree.left, l, left_size, acc),",
            "                      self(self, slot.tree.right, 0, r-left_size, acc));",
        ]
    else:
        lines += [
            f"            F next = {lazy_test} ? composition(acc, slot.data.lazy) : acc;",
            "            if (r <= left_size) return self(self, slot.tree.left, l, r, next);",
            "            if (l > left_size) return self(self, slot.tree.right, l-left_size-1, r-left_size-1, next);",
            "            T result = acc != identity ? mapping(acc, slot.data.key) : slot.data.key;",
            "            if (l < left_size) result = op(self(self, slot.tree.left, l, left_size, next), result);",
            "            if (left_size+1 < r) result = op(result, self(self, slot.tree.right, 0, r-left_size-1, next));",
            "            return result;",
        ]
    lines += [
        "        };",
        "        return dfs(dfs, root, l, r, identity);",
        "    }",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--leaf", action="store_true")
    parser.add_argument("--separate-leaves", action="store_true",
                        help="Use tagged branch/leaf handles and separate storage pools (implies --leaf)")
    parser.add_argument("--cached-lazy", action="store_true",
                        help="Read Data.has_lazy in a source that already maintains that flag")
    args = parser.parse_args()
    args.leaf = args.leaf or args.separate_leaves
    source = args.source.read_text()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    variants = {
        "get_reflect_branch": (False, False, False),
        "get_reflect_cmov": (True, False, False),
        "get_reflect_branch_prefetch": (False, True, False),
        "get_reflect_cmov_prefetch": (True, True, False),
        "get_reflect_mask": (False, False, True),
        "get_reflect_mask_prefetch": (False, True, True),
    }
    for name, (conditional_move, prefetch, mask_select) in variants.items():
        candidate = replace_function(
            source,
            "    T get(SizeType k) const {",
            reflected_get(args.leaf, conditional_move, prefetch, mask_select,
                          args.cached_lazy, args.separate_leaves),
        )
        (args.output_dir / f"{name}.hpp").write_text(candidate)
    prod_candidate = replace_function(
        source, "    T prod(SizeType l, SizeType r) const {",
        reflected_prod(args.leaf, args.cached_lazy, args.separate_leaves),
    )
    (args.output_dir / "prod_reflect.hpp").write_text(prod_candidate)
    for prefetch in (False, True):
        combined = replace_function(
            prod_candidate, "    T get(SizeType k) const {",
            reflected_get(args.leaf, False, prefetch, True, args.cached_lazy, args.separate_leaves),
        )
        name = "get_reflect_mask" + ("_prefetch" if prefetch else "") + "_prod_reflect.hpp"
        (args.output_dir / name).write_text(combined)


if __name__ == "__main__":
    main()
