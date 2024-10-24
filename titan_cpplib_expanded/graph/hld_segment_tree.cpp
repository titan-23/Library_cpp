// #include "titan_cpplib/graph/hld_segment_tree.cpp"
#include <vector>
// #include "titan_cpplib/graph/hld.cpp"
#include <vector>
#include <stack>
using namespace std;

// HLD
namespace titan23 {

    class HLD {
      private:
        vector<vector<int>> G;
        int root, n;
        vector<int> size, par, dep, nodein, nodeout, head, hld;

        void _dfs() {
            dep[root] = 0;
            stack<int> st;
            st.emplace(root);
            while (!st.empty()) {
                int v = st.top(); st.pop();
                if (v >= 0) {
                    int dep_nxt = dep[v] + 1;
                    for (const int x: G[v]) {
                        if (dep[x] != -1) continue;
                        dep[x] = dep_nxt;
                        st.emplace(~x);
                        st.emplace(x);
                    }
                } else {
                    v = ~v;
                    for (int i = 0; i < (int)G[v].size(); ++i) {
                        int x = G[v][i];
                        if (dep[x] < dep[v]) {
                            par[v] = x;
                            continue;
                        }
                        size[v] += size[x];
                        if (size[x] > size[G[v][0]]) {
                            swap(G[v][0], G[v][i]);
                        }
                    }
                }
            }

            int curtime = 0;
            st.emplace(~root);
            st.emplace(root);
            while (!st.empty()) {
                int v = st.top(); st.pop();
                if (v >= 0) {
                    if (par[v] == -1) {
                        head[v] = v;
                    }
                    nodein[v] = curtime;
                    hld[curtime] = v;
                    ++curtime;
                    if (G[v].empty()) continue;
                    int G_v0 = (int)G[v][0];
                    for (int i = (int)G[v].size()-1; i >= 0; --i) {
                        int x = G[v][i];
                        if (x == par[v]) continue;
                        head[x] = (x == G_v0? head[v]: x);
                        st.emplace(~x); 
                        st.emplace(x); 
                    }
                } else {
                    nodeout[~v] = curtime;          
                }
            }
        }

      public:
        HLD() {}
        HLD(const vector<vector<int>> &G, const int root) :
                G(G), root(root), n(G.size()),
                size(n, 1), par(n, -1), dep(n, -1),
                nodein(n, -1), nodeout(n, -1), head(n, -1), hld(n, -1) {
            _dfs();
        }

        template<typename T>
        vector<T> build_list(vector<T> a) const {
            vector<T> res(a.size());
            for (int i = 0; i < n; ++i) {
                res[i] = a[hld[i]];
            }
            return res;
        }

        //! `u`, `v` の lca を返す / `O(logn)`
        int lca(int u, int v) const {
            while (true) {
                if (nodein[u] > nodein[v]) swap(u, v);
                if (head[u] == head[v]) return u;
                v = par[head[v]];
            }
        }

        //! `s` から `t` へ、`k` 個進んだ頂点を返す / `O(logn)`
        int path_kth_elm(int s, int t, int k) {
            int x = lca(s, t);
            int d = dep[s] + dep[t] - 2*dep[x];
            if (d < k) return -1;
            if (dep[s] - dep[x] < k) {
                s = t;
                k = d - k;
            }
            int hs = head[s];
            while (dep[s] - dep[hs] < k) {
                k -= dep[s] - dep[hs] + 1;
                s = par[hs];
                hs = head[s];
            }
            return hld[nodein[s] - k];
        }
    };
}  // namespace titan23

// #include "titan_cpplib/data_structures/segment_tree.cpp"
#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

    template <class T,
              T (*_op)(T, T),
              T (*_e)()>
    struct SegmentTree {
      private:
        int n, _size, _log;
        vector<T> _data;

      public:
        SegmentTree() {}

        SegmentTree(const int n) {
            _build(n);
        }

        SegmentTree(const vector<T> &a) {
            int n = (int)a.size();
            _build(n);
            for (int i = 0; i < n; ++i) {
                _data[i+_size] = a[i];
            }
            for (int i = _size-1; i > 0; --i) {
                _data[i] = _op(_data[i<<1], _data[i<<1|1]);
            }
        }

        void _build(const int n) {
            this->n = n;
            this->_log = 32 - __builtin_clz(n);
            this->_size = 1 << _log;
            this->_data.resize(_size << 1, _e());
        }

        T get(int const k) const {
            return _data[(k<0? (k+n+_size): (k+_size))];
        }

        void set(int k, const T v) {
            if (k < 0) k += n;
            k += _size;
            _data[k] = v;
            for (int i = 0; i < _log; ++i) {
                k >>= 1;
                _data[k] = _op(_data[k<<1], _data[k<<1|1]);
            }
        }

        T prod(int l, int r) const {
            l += _size;
            r += _size;
            T lres = _e(), rres = _e();
            while (l < r) {
                if (l & 1) lres = _op(lres, _data[l++]);
                if (r & 1) rres = _op(_data[r^1], rres);
                l >>= 1;
                r >>= 1;
            }
            return _op(lres, rres);
        }

        T all_prod() const {
            return _data[1];
        }

        template<typename F>  // F: function<bool (T)> f
        int max_right(int l, F &&f) const {
            assert(0 <= l && l <= _size);
            // assert(f(_e()));
            if (l == n) return n;
            l += _size;
            T s = _e();
            while (1) {
                while ((l & 1) == 0) {
                l >>= 1;
                }
                if (!f(_op(s, _data[l]))) {
                while (l < _size) {
                    l <<= 1;
                    if (f(_op(s, _data[l]))) {
                    s = _op(s, _data[l]);
                    l |= 1;
                    }
                }
                return l - _size;
                }
                s = _op(s, _data[l]);
                ++l;
                if ((l & (-l)) == l) break;
            }
            return n;
        }

        template<typename F>  // F: function<bool (T)> f
        int min_left(int r, F &&f) const {
            assert(0 <= r && r <= n);
            // assert(f(_e()));
            if (r == 0) return 0;
            r += _size;
            T s = _e();
            while (r > 0) {
                --r;
                while (r > 1 && (r & 1)) {
                r >>= 1;
                }
                if (!f(_op(_data[r], s))) {
                while (r < _size) {
                    r = (r << 1) | 1;
                    if (f(_op(_data[r], s))) {
                    s = _op(_data[r], s);
                    r ^= 1;
                    }
                }
                return r + 1 - _size;
                }
                s = _op(_data[r], s);
                if ((r & (-r)) == r) break;
            }
            return 0;
        }

        vector<T> tovector() const {
            vector<T> res(n);
            for (int i = 0; i < n; ++i) {
                res[i] = get(i);
            }
            return res;
        }

        void print() const {
            cout << '[';
            for (int i = 0; i < n-1; ++i) {
                cout << get(i) << ", ";
            }
            if (n > 0) cout << get(n-1);
            cout << ']' << endl;
        }
    };
}  // namespace titan23

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

