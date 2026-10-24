/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/range_distinct_query_dynamic.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/// 挿入・削除・一点変更に対応する区間内の異なる値の個数
template<typename T>
class DynamicRangeDistinctQuery {
private:
    struct Block;

    struct Node {
        T val;
        Node *prev;
        Block *blk;
        int idx;

        explicit Node(const T &x) : val(x), prev(nullptr), blk(nullptr), idx(0) {}
    };

    struct Block {
        int idx;
        vector<unique_ptr<Node>> data;
        vector<Node *> prev;

        explicit Block(const int idx) : idx(idx) {}
    };

    struct Order {
        bool operator()(const Node *a, const Node *b) const {
            if (a == b) return false;
            if (a == nullptr) return true;
            if (b == nullptr) return false;
            if (a->blk == b->blk) return a->idx < b->idx;
            return a->blk->idx < b->blk->idx;
        }
    };

    int _bs;
    int _n;
    vector<unique_ptr<Block>> blocks;
    map<T, std::set<Node *, Order>> pos;

    pair<int, int> _locate(int k) const {
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
            const int size = blocks[i]->data.size();
            if (k < size) return {i, k};
            k -= size;
        }
        assert(false);
        return {-1, -1};
    }

    Node *_node_at(const int k) const {
        const auto [b, off] = _locate(k);
        return blocks[b]->data[off].get();
    }

    static void _fix_pos(Block &b, const int start = 0) {
        for (int i = start; i < static_cast<int>(b.data.size()); ++i) {
            b.data[i]->blk = &b;
            b.data[i]->idx = i;
        }
    }

    void _fix_idx(const int start) {
        for (int i = start; i < static_cast<int>(blocks.size()); ++i) blocks[i]->idx = i;
    }

    static void _build_prev(Block &b) {
        b.prev.clear();
        for (const auto &node : b.data) b.prev.push_back(node->prev);
        sort(b.prev.begin(), b.prev.end(), Order());
    }

    static void _erase_prev(Block &b, Node *p) {
        const auto it = lower_bound(b.prev.begin(), b.prev.end(), p, Order());
        assert(it != b.prev.end() && *it == p);
        b.prev.erase(it);
    }

    static void _insert_prev(Block &b, Node *p) {
        const auto it = lower_bound(b.prev.begin(), b.prev.end(), p, Order());
        b.prev.insert(it, p);
    }

    static void _set_prev(Node *node, Node *p) {
        if (node->prev == p) return;
        _erase_prev(*node->blk, node->prev);
        node->prev = p;
        _insert_prev(*node->blk, p);
    }

    void _split(const int idx) {
        Block &left = *blocks[idx];
        const int middle = left.data.size() / 2;
        auto right = make_unique<Block>(idx + 1);
        right->data.reserve(left.data.size() - middle);
        right->prev.reserve(left.data.size() - middle);
        blocks.reserve(blocks.size() + 1);
        for (int i = middle; i < static_cast<int>(left.data.size()); ++i) {
            right->data.push_back(move(left.data[i]));
        }
        left.data.resize(middle);
        blocks.insert(blocks.begin() + idx + 1, move(right));
        // 全要素の順序情報を直してから比較する
        // 既存要素同士の前後関係は変わらない
        _fix_idx(idx + 1);
        _fix_pos(left);
        _fix_pos(*blocks[idx + 1]);
        _build_prev(left);
        _build_prev(*blocks[idx + 1]);
    }

    void _rebalance(const int idx) {
        if (blocks.size() <= 1 || blocks[idx]->data.size() >= static_cast<size_t>(_bs)) return;
        const int left_idx = idx == 0 ? 0 : idx - 1;
        Block &left = *blocks[left_idx];
        Block &right = *blocks[left_idx + 1];
        const int total = left.data.size() + right.data.size();
        if (static_cast<size_t>(total) <= size_t(3) * _bs) {
            left.data.reserve(total);
            left.prev.reserve(total);
            for (auto &node : right.data) left.data.push_back(move(node));
            blocks.erase(blocks.begin() + left_idx + 1);
            _fix_idx(left_idx + 1);
            _fix_pos(left);
            _build_prev(left);
            return;
        }

        const int middle = total / 2;
        vector<unique_ptr<Node>> data;
        data.reserve(total);
        left.data.reserve(middle);
        right.data.reserve(total - middle);
        left.prev.reserve(middle);
        right.prev.reserve(total - middle);
        for (auto &node : left.data) data.push_back(move(node));
        for (auto &node : right.data) data.push_back(move(node));
        left.data.clear();
        right.data.clear();
        for (int i = 0; i < middle; ++i) left.data.push_back(move(data[i]));
        for (int i = middle; i < total; ++i) right.data.push_back(move(data[i]));
        _fix_pos(left);
        _fix_pos(right);
        _build_prev(left);
        _build_prev(right);
    }

    void _build(const vector<T> &a) {
        assert(a.size() <= static_cast<size_t>(numeric_limits<int>::max()));
        _n = a.size();
        if (a.empty()) return;
        const size_t target = size_t(2) * _bs;
        const int count = (a.size() + target - 1) / target;
        blocks.reserve(count);
        int start = 0;
        for (int i = 0; i < count; ++i) {
            const int take = (_n - start - 1) / (count - i) + 1;
            auto b = make_unique<Block>(i);
            b->data.reserve(take);
            b->prev.reserve(take);
            for (int j = 0; j < take; ++j) {
                auto node = make_unique<Node>(a[start + j]);
                node->blk = b.get();
                node->idx = j;
                auto &s = pos.try_emplace(node->val).first->second;
                node->prev = s.empty() ? nullptr : *s.rbegin();
                s.emplace_hint(s.end(), node.get());
                b->prev.push_back(node->prev);
                b->data.push_back(move(node));
            }
            sort(b->prev.begin(), b->prev.end(), Order());
            blocks.push_back(move(b));
            start += take;
        }
    }

    void _swap(DynamicRangeDistinctQuery &other) noexcept {
        swap(_bs, other._bs);
        swap(_n, other._n);
        blocks.swap(other.blocks);
        pos.swap(other.pos);
    }

public:
    /// 空列を作る
    /// bs は正の基準ブロック幅（既定512）
    explicit DynamicRangeDistinctQuery(const int bs = 512) : _bs(bs), _n(0) {
        assert(bs > 0);
    }

    /// O(n log n)
    /// T にデフォルトコンストラクタや operator== は不要
    explicit DynamicRangeDistinctQuery(const vector<T> &a, const int bs = 512)
        : DynamicRangeDistinctQuery(bs) { _build(a); }

    DynamicRangeDistinctQuery(const DynamicRangeDistinctQuery &other)
        : DynamicRangeDistinctQuery(other.tovector(), other._bs) {}

    DynamicRangeDistinctQuery(DynamicRangeDistinctQuery &&other) noexcept
        : _bs(other._bs), _n(exchange(other._n, 0)),
          blocks(move(other.blocks)), pos(move(other.pos)) {
        other.pos.clear();
        other.blocks.clear();
    }

    DynamicRangeDistinctQuery &operator=(const DynamicRangeDistinctQuery &other) {
        if (this != &other) {
            DynamicRangeDistinctQuery copy(other);
            _swap(copy);
        }
        return *this;
    }

    DynamicRangeDistinctQuery &operator=(DynamicRangeDistinctQuery &&other) noexcept {
        if (this != &other) {
            DynamicRangeDistinctQuery moved(move(other));
            _swap(moved);
        }
        return *this;
    }

    /// 区間 [l,r) の異なる値の個数 / B=bs として O(B+(n/B)log(B+1))
    int count(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r) return 0;
        if (l == 0 && r == _n) return pos.size();
        auto [idx, off] = _locate(l);
        const Node *first = blocks[idx]->data[off].get();
        int rem = r - l;
        int res = 0;
        while (rem > 0) {
            const Block &b = *blocks[idx];
            const int take = min(rem, static_cast<int>(b.data.size()) - off);
            if (off == 0 && take == static_cast<int>(b.data.size())) {
                if (Order()(b.prev.back(), first)) res += take;
                else if (Order()(b.prev.front(), first)) {
                    res += lower_bound(b.prev.begin(), b.prev.end(), first, Order()) - b.prev.begin();
                }
            } else {
                for (int i = off; i < off + take; ++i) res += Order()(b.data[i]->prev, first);
            }
            rem -= take;
            off = 0;
            ++idx;
        }
        return res;
    }

    int range_distinct(const int l, const int r) const { return count(l, r); }

    /// O(n/B)
    /// 挿入・削除で添字が変わっても、要素自体のアドレスは変わらない
    const T &access(const int k) const {
        assert(0 <= k && k < _n);
        return _node_at(k)->val;
    }

    /// 一点変更 / O(B+n/B+log n)
    /// 変わる直前出現は高々3個
    void set(const int k, const T &x) {
        assert(0 <= k && k < _n);
        T val(x);
        Node *node = _node_at(k);
        const auto old_map = pos.find(node->val);
        const auto new_map = pos.try_emplace(val).first;
        if (old_map == new_map) {
            node->val = move(val);
            return;
        }
        auto &old_s = old_map->second;
        auto &new_s = new_map->second;
        const auto old_it = old_s.find(node);
        const auto old_next = next(old_it);
        const auto new_next = new_s.lower_bound(node);
        Node *p = new_next == new_s.begin() ? nullptr : *prev(new_next);
        if (old_next != old_s.end()) _set_prev(*old_next, node->prev);
        _set_prev(node, p);
        if (new_next != new_s.end()) _set_prev(*new_next, node);
        new_s.insert(new_next, node);
        old_s.erase(old_it);
        node->val = move(val);
        if (old_s.empty()) pos.erase(old_map);
    }

    /// 位置kの直前に挿入 / 最悪 O(B log(B+1)+n/B+log n)
    void insert(const int k, const T &x) {
        assert(0 <= k && k <= _n);
        assert(_n < numeric_limits<int>::max());
        auto node = make_unique<Node>(x);
        auto &s = pos.try_emplace(node->val).first->second;
        int idx = 0, off = 0;
        if (blocks.empty()) {
            blocks.push_back(make_unique<Block>(0));
        } else if (k == _n) {
            idx = blocks.size() - 1;
            off = blocks[idx]->data.size();
        } else {
            tie(idx, off) = _locate(k);
        }
        Block &b = *blocks[idx];
        const auto next_it = k == _n ? s.end() : s.lower_bound(b.data[off].get());
        node->prev = next_it == s.begin() ? nullptr : *prev(next_it);
        Node *added = node.get();
        b.data.insert(b.data.begin() + off, move(node));
        _fix_pos(b, off);
        s.insert(next_it, added);
        _insert_prev(b, added->prev);
        if (next_it != s.end()) _set_prev(*next_it, added);
        ++_n;
        if (b.data.size() > size_t(4) * _bs) _split(idx);
    }

    /// 位置kを削除して値を返す / 最悪 O(B log(B+1)+n/B+log n)
    T pop(const int k) {
        assert(0 <= k && k < _n);
        const auto [idx, off] = _locate(k);
        Block &b = *blocks[idx];
        Node *node = b.data[off].get();
        T val(node->val);
        const auto entry = pos.find(node->val);
        auto &s = entry->second;
        const auto position = s.find(node);
        const auto next_it = next(position);
        if (next_it != s.end()) _set_prev(*next_it, node->prev);
        _erase_prev(b, node->prev);
        s.erase(position);
        if (s.empty()) pos.erase(entry);
        b.data.erase(b.data.begin() + off);
        --_n;
        _fix_pos(b, off);
        if (_n == 0) blocks.clear();
        else _rebalance(idx);
        return val;
    }

    int len() const { return _n; }
    bool empty() const { return _n == 0; }

    vector<T> tovector() const {
        vector<T> res;
        res.reserve(_n);
        for (const auto &b : blocks) {
            for (const auto &node : b->data) res.push_back(node->val);
        }
        return res;
    }
};

} // namespace titan23
