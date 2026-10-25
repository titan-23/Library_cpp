#!/usr/bin/env python3
"""Generate isolated join/range-operation candidates from the saved fast tree.

Use --source to apply the same algorithm changes to an allocator candidate and
--output to select a temporary directory. The installed library is not edited.
"""
from pathlib import Path
import argparse

def replace_method(source, marker, body):
    start=source.index(marker); pos=source.index('{',start)+1; depth=1
    while depth:
        depth+=(source[pos]=='{')-(source[pos]=='}'); pos+=1
    return source[:start]+body+source[pos:]

EXTRACT_LEFT='''    pair<SizeType, SizeType> extract_left_node(SizeType node, size_t first) {
        node = own(node, first);
        propagate(node, first);
        if (!ma.tree[node].left) {
            SizeType rest = ma.tree[node].right;
            ma.tree[node].right = 0;
            update(node);
            return {rest, node};
        }
        auto result = extract_left_node(ma.tree[node].left, first);
        ma.tree[node].left = result.first;
        update(node);
        return {balance(node, first), result.second};
    }

'''
MERGE_SMALLER='''    SizeType _merge_node(SizeType l, SizeType r, size_t first) {
        if (!l) return r;
        if (!r) return l;
        if (ma.tree[r].size < ma.tree[l].size) {
            auto [rest, pivot] = extract_left_node(r, first);
            return _merge_with_root(l, pivot, rest, first);
        }
        auto [rest, pivot] = extract_right_node(l, first);
        return _merge_with_root(rest, pivot, r, first);
    }'''
APPLY_LOCAL='''    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(f != id())) return _new(root);
        const size_t first = ma.ptr;
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType l, SizeType r) -> SizeType {
            node = own(node, first);
            if (l == 0 && r == ma.tree[node].size) {
                push(node, f);
                return node;
            }
            propagate(node, first);
            SizeType left_size = ma.tree[ma.tree[node].left].size;
            if (l < left_size) {
                ma.tree[node].left = dfs(dfs, ma.tree[node].left, l, min(r, left_size));
            }
            if (l <= left_size && left_size < r) ma.data[node].key = mapping(f, ma.data[node].key);
            if (left_size+1 < r) {
                SizeType start = l > left_size+1 ? l-left_size-1 : 0;
                ma.tree[node].right = dfs(dfs, ma.tree[node].right, start, r-left_size-1);
            }
            update(node);
            return node;
        };
        return _new(dfs(dfs, root, l, r));
    }'''
PROD_LOCAL='''    T prod(SizeType l, SizeType r) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType l, SizeType r, F acc, bool rev) -> T {
            if (l == 0 && r == ma.tree[node].size) {
                return acc != id() ? mapping(acc, ma.data[node].data) : ma.data[node].data;
            }
            rev ^= ma.data[node].rev;
            SizeType lch = rev ? ma.tree[node].right : ma.tree[node].left;
            SizeType rch = rev ? ma.tree[node].left : ma.tree[node].right;
            SizeType left_size = ma.tree[lch].size;
            F next = ma.data[node].lazy != id() ? composition(acc, ma.data[node].lazy) : acc;
            if (r <= left_size) return dfs(dfs, lch, l, r, next, rev);
            if (l > left_size) return dfs(dfs, rch, l-left_size-1, r-left_size-1, next, rev);
            T result = acc != id() ? mapping(acc, ma.data[node].key) : ma.data[node].key;
            if (l < left_size) result = op(dfs(dfs, lch, l, left_size, next, rev), result);
            if (left_size+1 < r) result = op(result, dfs(dfs, rch, 0, r-left_size-1, next, rev));
            return result;
        };
        return dfs(dfs, root, l, r, id(), false);
    }'''
REVERSE_LOCAL_HELPER='''    SizeType reverse_node(SizeType node, SizeType l, SizeType r, size_t first) {
        if (r-l <= 1) return node;
        node = own(node, first);
        if (l == 0 && r == ma.tree[node].size) {
            ma.data[node].rev ^= 1;
            return node;
        }
        propagate(node, first);
        SizeType left_size = ma.tree[ma.tree[node].left].size;
        if (r <= left_size) {
            ma.tree[node].left = reverse_node(ma.tree[node].left, l, r, first);
            update(node);
            return node;
        }
        if (l > left_size) {
            ma.tree[node].right = reverse_node(ma.tree[node].right, l-left_size-1, r-left_size-1, first);
            update(node);
            return node;
        }
        auto [prefix, suffix] = _split_node(node, r, first);
        auto [left, middle] = _split_node(prefix, l, first);
        middle = own(middle, first);
        ma.data[middle].rev ^= 1;
        return _merge_node(_merge_node(left, middle, first), suffix, first);
    }

'''
REVERSE_LOCAL='''    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        return _new(reverse_node(root, l, r, ma.ptr));
    }'''

def merge_smaller(s):
    s=s.replace('    SizeType _merge_node(',EXTRACT_LEFT+'    SizeType _merge_node(',1)
    return replace_method(s,'    SizeType _merge_node(',MERGE_SMALLER)
def range_local(s):
    s=replace_method(s,'    PLTM apply(',APPLY_LOCAL)
    return replace_method(s,'    T prod(',PROD_LOCAL)
def reverse_local(s):
    s=s.replace('    static PLTM _new(',REVERSE_LOCAL_HELPER+'    static PLTM _new(',1)
    return replace_method(s,'    PLTM reverse(',REVERSE_LOCAL)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--source',type=Path,default=Path(__file__).resolve().parent / 'baseline/persistent_lazy_wbtree_fast_vector.cpp')
    p.add_argument('--output',type=Path,default=Path('/tmp/wbt_algorithm_candidates'))
    a=p.parse_args(); source=a.source.read_text(); a.output.mkdir(parents=True,exist_ok=True)
    variants={'direct_base':source,'merge_smaller':merge_smaller(source),'range_local':range_local(source),'reverse_local':reverse_local(source),'combined':reverse_local(range_local(merge_smaller(source)))}
    for name,text in variants.items():
        out=a.output/(name+'.hpp');out.write_text(text);print(out)

if __name__=='__main__':main()
