#include <vector>
#include "titan_cpplib/graph/hld.cpp"
#include "titan_cpplib/data_structures/segment_tree.cpp"
using namespace std;

namespace titan23 {

    /**
     * @brief セグ木搭載HLD
     * 
     * @tparam T 
     * @tparam (*op)(T, T) 
     * @tparam (*e)() 
     */
    template <class T,
              T (*op)(T, T),
              T (*e)()>
    class HLDSegmentTree {
      private:
        titan23::HLD hld;
        titan23::SegmentTree<T, op, e> seg;

      public:
        HLDSegmentTree(const titan23::HLD &hld) {
            this->hld = hld;
            this->seg = titan23::SegmentTree<T, op, e>(hld.n);
        }

        HLDSegmentTree(const titan23::HLD &hld, constvector<T> &a) {
            this->hld = hld;
            this->seg = titan23::SegmentTree<T, op, e>(hld.build_list(a));
        }

        //! `u` から `v` へのパスの総積を返す / `O(logn)`
        T path_prod(int u, int v) const {
            T res = e();
            while (hld.head[u] != hld.head[v]) {
                if (hld.dep[hld.head[u]] < hld.dep[hld.head[v]]) {
                    swap(u, v);
                }
                res = op(res, seg.prod(hld.nodein[hld.head[u]], hld.nodein[u]+1));
                u = hld.par[hld.head[u]];
            }
            if (hld.dep[u] < hld.dep[v]) {
                swap(u, v);
            }
            return op(res, seg.prod(hld.nodein[v], hld.nodein[u]+1));
        }

        //! 頂点 `k` の値を返す / `O(1)`
        T get(const int k) const {
            return seg.get(hld.nodein[k]);
        }

        //! 頂点 `k` の値を `v` に更新する / `O(logn)`
        void set(const int k, const T v) {
            seg.set(hld.nodein[k], v);
        }

        //! 頂点 `v` の部分木の総積を返す / `O(logn)`
        T subtree_prod(const int v) const {
            return seg.prod(hld.nodein[v], hld.nodeout[v]);
        }
    };
}  // namespace titan23
