#include <vector>
#include "titan_cpplib/graph/hld.cpp"
#include "titan_cpplib/data_structures/segment_tree.cpp"
using namespace std;

namespace titan23 {

  template <class T,
            T (*op)(T, T),
            T (*e)()>
  struct HLDSegmentTree {
    titan23::HLD hld;
    titan23::SegmentTree<T, op, e> seg;

    HLDSegmentTree(titan23::HLD hld, int n) {
      this->hld = hld;
      titan23::SegmentTree<T, op, e> seg(n);
      this->seg = seg;
    }

    HLDSegmentTree(titan23::HLD hld, vector<T> a) {
      this->hld = hld;
      a = hld.build_list(a);
      titan23::SegmentTree<T, op, e> seg(a);
      this->seg = seg;
    }

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

    T get(const int k) const {
      return seg.get(hld.nodein[k]);
    }

    void set(const int k, const T v) {
      seg.set(hld.nodein[k], v);
    }

    T subtree_prod(const int v) const {
      return seg.prod(hld.nodein[v], hld.nodeout[v]);
    }
  };
}  // namespace titan23
