#!/usr/bin/env python3
"""Create otherwise identical set/get implementations for comparison.

python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_accessors.py /tmp/plwbt-accessors
Compare headers with run_persistent_lazy_wbtree_benchmark.py --baseline-header
... --optimized-header ... --workload set|get|access. Generated headers are
standalone snapshots; the library itself has no benchmark selection macros.
"""
import argparse
from pathlib import Path

SET_RECURSIVE = '''    PLTM set(SizeType k, T v) {
        assert(0 <= k && k < len());
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType k) -> SizeType {
            propagate(node);
            node = ma.copy(node);
            SizeType t = ma.tree[ma.tree[node].left].size;
            if (t == k) {
                ma.data[node].key = v;
            } else if (k < t) {
                ma.tree[node].left = dfs(dfs, ma.tree[node].left, k);
            } else {
                ma.tree[node].right = dfs(dfs, ma.tree[node].right, k-t-1);
            }
            update(node);
            return node;
        };
        return _new(dfs(dfs, root, k));
    }'''

SET_ITERATIVE = '''    PLTM set(SizeType k, T v) {
        assert(0 <= k && k < len());
        PATH_DECL
        SizeType node = root, new_root = 0;
        bool right = false;
        while (true) {
            propagate(node);
            node = ma.copy(node);
            if (PATH_EMPTY) {
                new_root = node;
            } else if (right) {
                ma.tree[PATH_TOP].right = node;
            } else {
                ma.tree[PATH_TOP].left = node;
            }
            PATH_PUSH;
            SizeType t = ma.tree[ma.tree[node].left].size;
            if (k == t) {
                ma.data[node].key = v;
                break;
            }
            right = t < k;
            if (right) {
                k -= t + 1;
                node = ma.tree[node].right;
            } else {
                node = ma.tree[node].left;
            }
        }
        while (!(PATH_EMPTY)) {
            update(PATH_TOP);
            PATH_POP;
        }
        return _new(new_root);
    }'''

GET_ITERATIVE = '''    T get(SizeType k) const {
        assert(0 <= k && k < len());
        SizeType node = root;
        F acc = id();
        bool rev = false;
        while (1) {
            rev ^= ma.data[node].rev;
            SizeType lch = rev ? ma.tree[node].right : ma.tree[node].left;
            SizeType rch = rev ? ma.tree[node].left : ma.tree[node].right;
            SizeType t = ma.tree[lch].size;
            if (t == k) {
                return acc != id() ? mapping(acc, ma.data[node].key) : ma.data[node].key;
            }
            if (ma.data[node].lazy != id()) acc = composition(acc, ma.data[node].lazy);
            if (t < k) {
                k -= t + 1;
                node = rch;
            } else {
                node = lch;
            }
        }
    }'''

GET_RECURSIVE = '''    T get(SizeType k) const {
        assert(0 <= k && k < len());
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType k, F acc, bool rev) -> T {
            rev ^= ma.data[node].rev;
            SizeType lch = rev ? ma.tree[node].right : ma.tree[node].left;
            SizeType rch = rev ? ma.tree[node].left : ma.tree[node].right;
            SizeType t = ma.tree[lch].size;
            if (t == k) {
                return acc != id() ? mapping(acc, ma.data[node].key) : ma.data[node].key;
            }
            if (ma.data[node].lazy != id()) acc = composition(acc, ma.data[node].lazy);
            if (k < t) return dfs(dfs, lch, k, acc, rev);
            return dfs(dfs, rch, k-t-1, acc, rev);
        };
        return dfs(dfs, root, k, id(), false);
    }'''


def replace_method(source, marker, body):
    start = source.index(marker)
    opening = source.index('{', start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[:start] + body + source[pos:]


def iterative_set(array):
    # WBT child weight is at most 3/4 of parent weight. Therefore the path
    # is shorter than 3 * bit_width(SizeType) + 1 for every representable size.
    substitutions = {
        'PATH_DECL': ('std::array<SizeType, 3 * std::numeric_limits<SizeType>::digits + 1> path;\n'
                      '        size_t depth = 0;') if array else 'std::stack<SizeType> path;',
        'PATH_EMPTY': 'depth == 0' if array else 'path.empty()',
        'PATH_TOP': 'path[depth-1]' if array else 'path.top()',
        'PATH_PUSH': 'path[depth++] = node' if array else 'path.emplace(node)',
        'PATH_POP': '--depth' if array else 'path.pop()',
    }
    result = SET_ITERATIVE
    for key, value in substitutions.items():
        result = result.replace(key, value)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--source', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    path = args.source or root / 'test/ds/benchmark/baseline/persistent_lazy_wbtree_before_fast.cpp'
    source = path.read_text()
    source = source.replace('#pragma once', '#pragma once\n#include <array>\n#include <limits>', 1)
    args.output.mkdir(parents=True, exist_ok=True)
    for name, setter, getter in (
        ('recursive_set_iterative_get', SET_RECURSIVE, GET_ITERATIVE),
        ('stack_set_iterative_get', iterative_set(False), GET_ITERATIVE),
        ('array_set_iterative_get', iterative_set(True), GET_ITERATIVE),
        ('recursive_set_recursive_get', SET_RECURSIVE, GET_RECURSIVE),
    ):
        result = replace_method(source, '    PLTM set(SizeType k, T v)', setter)
        result = replace_method(result, '    T get(SizeType k) const', getter)
        target = args.output / (name + '.hpp')
        target.write_text(result)
        print(target)


if __name__ == '__main__':
    main()
