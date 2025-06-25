#include <vector>
#include <cassert>
#include <algorithm>
using namespace std;

// PerfectBinaryTree
namespace titan23 {

// 1-indexedの完全二分木クラス
class PerfectBinaryTree {
private:
    int bit_length(long long x) {
        return x == 0 ? 0 : 64 - __builtin_clzll(x);
    }

public:
    PerfectBinaryTree() {}

    // 親を返す
    long long par(long long u) {
        assert(1 <= u && u < LONG_LONG_MAX);
        return u > 1 ? (u >> 1) : -1;
    }

    // 左の子を返す
    long long child_left(long long u) {
        assert(1 <= u && u < LONG_LONG_MAX);
        return u << 1;
    }

    // 右の子を返す
    long long child_right(long long u) {
        assert(1 <= u && u < LONG_LONG_MAX);
        return u << 1 | 1;
    }

    // 左右の子をタプルで返す
    pair<long long, long long> children(long long u) {
        assert(1 <= u && u < LONG_LONG_MAX);
        return {child_left(u), child_right(u)};
    }

    // 兄弟を返す
    long long sibling(long long u) {
        assert(2 <= u && u < LONG_LONG_MAX);
        return u ^ 1;
    }

    // 深さを返す / :math:`O(1)`
    long long dep(long long u) {
        assert(1 <= u && u < LONG_LONG_MAX);
        return bit_length(u);
    }

    // k個上の祖先を返す
    // k == 0 のとき、u自身を返す
    long long la(long long u, long long k) {
        return u >> k;
    }

    // lcaを返す
    long long lca(long long u, long long v) {
        assert(1 <= u && u < LONG_LONG_MAX);
        assert(1 <= v && v < LONG_LONG_MAX);
        if (dep(u) > dep(v)) {
            swap(u, v);
        }
        v = la(v, dep(v) - dep(u));
        return u >> bit_length(u ^ v);
    }

    // 距離を返す
    long long dist(long long u, long long v) {
        assert(1 <= u && u < LONG_LONG_MAX);
        assert(1 <= v && v < LONG_LONG_MAX);
        return dep(u) + dep(v) - 2*dep(lca(u, v));
    }

    // uからvへのパスをリストで返す
    vector<long long> get_path(long long u, long long v) {
        assert(1 <= u && u < LONG_LONG_MAX);
        assert(1 <= v && v < LONG_LONG_MAX);
        long long x = lca(u, v);
        auto get = [&] (long long x) -> vector<long long> {
            vector<long long> a;
            while (x != x) {
                a.push_back(x);
                x = par(x);
            }
            return a;
        };
        vector<long long> res = get(u);
        res.push_back(u);
        vector<long long> r = get(v);
        reverse(r.begin(), r.end());
        for (long long k : r) {
            res.push_back(k);
        }
        return res;
    }
};
} // namespace titan23
