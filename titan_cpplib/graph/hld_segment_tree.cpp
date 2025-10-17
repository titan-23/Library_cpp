#include <vector>
#include "titan_cpplib/graph/hld.cpp"
#include "titan_cpplib/data_structures/segment_tree.cpp"
using namespace std;

namespace titan23 {

/**
 * @brief セグ木搭載HLD / 非可換に対応
 *
 * @tparam T
 * @tparam (*op)(T, T)
 * @tparam (*e)()
 */
template <class T, T (*op)(T, T), T (*e)()>
class HLDSegmentTree {
private:
    titan23::HLD hld;
    titan23::SegmentTree<T, op, e> seg, rseg;

public:
    HLDSegmentTree(const titan23::HLD &hld) : hld(hld), seg(hld.n), rseg(hld.n) {}

    HLDSegmentTree(const titan23::HLD &hld, const vector<T> &a) : hld(hld) {
        vector<T> b = hld.build_list(a);
        this->seg = titan23::SegmentTree<T, op, e>(b);
        reverse(b.begin(), b.end());
        this->rseg = titan23::SegmentTree<T, op, e>(b);
    }

    //! `u` から `v` へのパスの総積を返す / `O(logn)`
    T path_prod(int u, int v) const {
        T lres = e(), rres = e();
        while (hld.head[u] != hld.head[v]) {
            if (hld.dep[hld.head[u]] > hld.dep[hld.head[v]]) {
                lres = op(lres, rseg.prod(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[hld.head[u]]));
                u = hld.par[hld.head[u]];
            } else {
                rres = op(seg.prod(hld.nodein[hld.head[v]], hld.nodein[v] + 1), rres);
                v = hld.par[hld.head[v]];
            }
        }
        if (hld.dep[u] > hld.dep[v]) {
            lres = op(lres, rseg.prod(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[v]));
        } else {
            lres = op(lres, seg.prod(hld.nodein[u], hld.nodein[v] + 1));
        }
        return op(lres, rres);
    }

    //! 頂点 `k` の値を返す / `O(1)`
    T get(const int k) const {
        return seg.get(hld.nodein[k]);
    }

    //! 頂点 `k` の値を `v` に更新する / `O(logn)`
    void set(const int k, const T v) {
        seg.set(hld.nodein[k], v);
        rseg.set(hld.n - hld.nodein[k] - 1, v);
    }

    //! 頂点 `v` の部分木の総積を返す / `O(logn)`
    T subtree_prod(const int v) const {
        return seg.prod(hld.nodein[v], hld.nodeout[v]);
    }
};
}  // namespace titan23
