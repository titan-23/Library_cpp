#!/usr/bin/env python3
"""Generate carried-tag split and crossing-range reversal candidates.

python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_round2_algorithms.py \
    --layout key --output /tmp/wbt-round2-key
python3 test/ds/benchmark/prepare_persistent_lazy_wbtree_round2_algorithms.py \
    --layout leaf --output /tmp/wbt-round2-leaf

Defaults to the preserved round2 snapshot for the chosen layout. Pass --source
HEADER to combine these changes with another allocator or operation variant.
Writes isolated reverse_cross/split_carry candidates and their combination;
the input and production library files are left unchanged.
"""

from pathlib import Path
import argparse

def edit(source,marker,body):
 start=source.index(marker);end=source.index('{',start)+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[:start]+body+source[end:]

def reverse_cross(source,leaf):
 marker='    SizeType reverse_node('
 start=source.index(marker);end=source.index('    static PLTM _new(',start)
 original=source[start:end]
 if leaf:
  old='''        auto tail = split_node(node, r, first);
        auto head = split_node(tail.first, l, first);
        SizeType middle = own(head.second, first);
        if (size_of(middle) > 1) ma.pool[middle].data.rev ^= 1;
        return join(join(head.first, middle, first), tail.second, first);'''
  new='''        auto left_parts = split_node(ma.pool[node].tree.left, l, first);
        auto right_parts = split_node(ma.pool[node].tree.right, r-left_size, first);
        SizeType left_middle = left_parts.second, right_middle = right_parts.first;
        if (size_of(left_middle) > 1) {
            left_middle = own(left_middle, first);
            ma.pool[left_middle].data.rev ^= 1;
        }
        if (size_of(right_middle) > 1) {
            right_middle = own(right_middle, first);
            ma.pool[right_middle].data.rev ^= 1;
        }
        SizeType left = join(left_parts.first, right_middle, first);
        SizeType right = join(left_middle, right_parts.second, first);
        return join(left, right, first, node);'''
 else:
  old='''        auto [prefix, suffix] = _split_node(node, r, first);
        auto [left, middle] = _split_node(prefix, l, first);
        middle = own(middle, first);
        ma.pool[middle].data.rev ^= 1;
        return _merge_node(_merge_node(left, middle, first), suffix, first);'''
  new='''        auto left_parts = _split_node(ma.pool[node].tree.left, l, first);
        auto right_parts = _split_node(ma.pool[node].tree.right, r-left_size-1, first);
        SizeType left_middle = left_parts.second, right_middle = right_parts.first;
        if (ma.pool[left_middle].tree.size > 1) {
            left_middle = own(left_middle, first);
            ma.pool[left_middle].data.rev ^= 1;
        }
        if (ma.pool[right_middle].tree.size > 1) {
            right_middle = own(right_middle, first);
            ma.pool[right_middle].data.rev ^= 1;
        }
        SizeType left = _merge_node(left_parts.first, right_middle, first);
        SizeType right = _merge_node(left_middle, right_parts.second, first);
        return _merge_with_root(left, node, right, first);'''
 if old not in original:raise ValueError('reverse pattern missing')
 return source[:start]+original.replace(old,new)+source[end:]

INHERIT='''    SizeType inherit_subtree(SizeType node, const F &acc, bool rev, size_t first) {
        if (!node) return 0;
        const bool action = acc != id();
        rev = rev && ma.pool[node].tree.size > 1;
        if (!action && !rev) return node;
        node = own(node, first);
        if (action) push(node, acc);
        if (rev) ma.pool[node].data.rev ^= 1;
        return node;
    }
'''
LEAF_CARRY='''    pair<SizeType, SizeType> split_carry(SizeType node, SizeType k, F acc, bool rev, size_t first) {
        if (!k) return {0, inherit_subtree(node, acc, rev, first)};
        if (k == size_of(node)) return {inherit_subtree(node, acc, rev, first), 0};
        rev ^= ma.pool[node].data.rev;
        SizeType l = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        SizeType r = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        SizeType ls = rev ? size_of(node)-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
        if (ma.pool[node].data.lazy != id()) acc = composition(acc, ma.pool[node].data.lazy);
        if (k == ls) return {inherit_subtree(l, acc, rev, first), inherit_subtree(r, acc, rev, first)};
        if (k < ls) {
            auto parts = split_carry(l, k, acc, rev, first);
            r = inherit_subtree(r, acc, rev, first);
            node = own(node, first);
            return {parts.first, join(parts.second, r, first, node)};
        }
        auto parts = split_carry(r, k-ls, acc, rev, first);
        l = inherit_subtree(l, acc, rev, first);
        node = own(node, first);
        return {join(l, parts.first, first, node), parts.second};
    }
    SizeType prefix_carry(SizeType node, SizeType k, F acc, bool rev, size_t first) {
        if (!k) return 0;
        if (k == size_of(node)) return inherit_subtree(node, acc, rev, first);
        rev ^= ma.pool[node].data.rev;
        SizeType l = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        SizeType r = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        SizeType ls = rev ? size_of(node)-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
        if (ma.pool[node].data.lazy != id()) acc = composition(acc, ma.pool[node].data.lazy);
        if (k <= ls) return prefix_carry(l, k, acc, rev, first);
        l = inherit_subtree(l, acc, rev, first);
        r = prefix_carry(r, k-ls, acc, rev, first);
        return join(l, r, first, own(node, first));
    }
    SizeType suffix_carry(SizeType node, SizeType k, F acc, bool rev, size_t first) {
        if (!k) return inherit_subtree(node, acc, rev, first);
        if (k == size_of(node)) return 0;
        rev ^= ma.pool[node].data.rev;
        SizeType l = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        SizeType r = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        SizeType ls = rev ? size_of(node)-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
        if (ma.pool[node].data.lazy != id()) acc = composition(acc, ma.pool[node].data.lazy);
        if (k >= ls) return suffix_carry(r, k-ls, acc, rev, first);
        l = suffix_carry(l, k, acc, rev, first);
        r = inherit_subtree(r, acc, rev, first);
        return join(l, r, first, own(node, first));
    }
'''
KEY_CARRY='''    SizeType inherit_pivot(SizeType node, const F &acc, size_t first) {
        node = own(node, first);
        if (acc != id()) ma.pool[node].data.key = mapping(acc, ma.pool[node].data.key);
        ma.pool[node].data.lazy = id();
        ma.pool[node].data.rev = 0;
        return node;
    }
    pair<SizeType, SizeType> split_carry(SizeType node, SizeType k, F acc, bool rev, size_t first) {
        if (!k) return {0, inherit_subtree(node, acc, rev, first)};
        if (k == ma.pool[node].tree.size) return {inherit_subtree(node, acc, rev, first), 0};
        rev ^= ma.pool[node].data.rev;
        SizeType l = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        SizeType r = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        SizeType ls = rev ? ma.pool[node].tree.size-1-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
        F next = ma.pool[node].data.lazy != id() ? composition(acc, ma.pool[node].data.lazy) : acc;
        if (k <= ls) {
            auto parts = split_carry(l, k, next, rev, first);
            r = inherit_subtree(r, next, rev, first);
            node = inherit_pivot(node, acc, first);
            return {parts.first, _merge_with_root(parts.second, node, r, first)};
        }
        auto parts = split_carry(r, k-ls-1, next, rev, first);
        l = inherit_subtree(l, next, rev, first);
        node = inherit_pivot(node, acc, first);
        return {_merge_with_root(l, node, parts.first, first), parts.second};
    }
    SizeType prefix_carry(SizeType node, SizeType k, F acc, bool rev, size_t first) {
        if (!k) return 0;
        if (k == ma.pool[node].tree.size) return inherit_subtree(node, acc, rev, first);
        rev ^= ma.pool[node].data.rev;
        SizeType l = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        SizeType r = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        SizeType ls = rev ? ma.pool[node].tree.size-1-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
        F next = ma.pool[node].data.lazy != id() ? composition(acc, ma.pool[node].data.lazy) : acc;
        if (k <= ls) return prefix_carry(l, k, next, rev, first);
        l = inherit_subtree(l, next, rev, first);
        r = prefix_carry(r, k-ls-1, next, rev, first);
        node = inherit_pivot(node, acc, first);
        return _merge_with_root(l, node, r, first);
    }
    SizeType suffix_carry(SizeType node, SizeType k, F acc, bool rev, size_t first) {
        if (!k) return inherit_subtree(node, acc, rev, first);
        if (k == ma.pool[node].tree.size) return 0;
        rev ^= ma.pool[node].data.rev;
        SizeType l = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        SizeType r = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        SizeType ls = rev ? ma.pool[node].tree.size-1-ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
        F next = ma.pool[node].data.lazy != id() ? composition(acc, ma.pool[node].data.lazy) : acc;
        if (k > ls) return suffix_carry(r, k-ls-1, next, rev, first);
        l = suffix_carry(l, k, next, rev, first);
        r = inherit_subtree(r, next, rev, first);
        node = inherit_pivot(node, acc, first);
        return _merge_with_root(l, node, r, first);
    }
'''

def split_carry(source,leaf):
 split='split_node' if leaf else '_split_node'; pre='prefix' if leaf else '_split_node_left';suf='suffix' if leaf else '_split_node_right'
 marker='    pair<SizeType, SizeType> '+split+'('
 source=source.replace(marker,INHERIT+(LEAF_CARRY if leaf else KEY_CARRY)+marker,1)
 source=edit(source,marker,'    pair<SizeType, SizeType> '+split+'''(SizeType node, SizeType k, size_t first) {
        return split_carry(node, k, id(), false, first);
    }''')
 source=edit(source,'    SizeType '+pre+'(','    SizeType '+pre+'''(SizeType node, SizeType k, size_t first) {
        return prefix_carry(node, k, id(), false, first);
    }''')
 return edit(source,'    SizeType '+suf+'(','    SizeType '+suf+'''(SizeType node, SizeType k, size_t first) {
        return suffix_carry(node, k, id(), false, first);
    }''')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--layout', choices=('key', 'leaf'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    default_name = 'persistent_lazy_wbtree_' + ('leaf' if args.layout == 'leaf' else 'fast') + '_round2.cpp'
    source_path = args.source or Path(__file__).resolve().parent / 'baseline' / default_name
    source = source_path.read_text()
    leaf = args.layout == 'leaf'
    args.output.mkdir(parents=True, exist_ok=True)
    variants = {
        'base': source,
        'reverse_cross': reverse_cross(source, leaf),
        'split_carry': split_carry(source, leaf),
        'combined': reverse_cross(split_carry(source, leaf), leaf),
    }
    for name, text in variants.items():
        output = args.output / (args.layout + '_' + name + '.hpp')
        output.write_text(text)
        print(output)


if __name__ == '__main__':
    main()
