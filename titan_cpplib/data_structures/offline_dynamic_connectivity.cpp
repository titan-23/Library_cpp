#include <vector>
#include <cassert>
#include <unordered_map>
#include <ext/pb_ds/assoc_container.hpp>

#include "titan_cpplib/data_structures/undoable_union_find.cpp"
using namespace std;

// OfflineDynamicConnectivity
namespace titan23 {

    class OfflineDynamicConnectivity {
      private:
        int _n, _query_count, _size, _q;
        long long _bit, _msk;
        unordered_map<long long, pair<int, int>> start;
        vector<vector<long long>> data;
        vector<tuple<int, int, long long>> edge_data;

        int bit_length(const int n) const {
            if (n == 0) return 0;
            return 32 - __builtin_clz(n);
        }

        void _internal_add(int l, int r, const long long edge) {
            l += _size;
            r += _size;
            while (l < r) {
                if (l & 1) data[l++].emplace_back(edge);
                if (r & 1) data[--r].emplace_back(edge);
                l >>= 1;
                r >>= 1;
            }
        }

      public:
        titan23::UndoableUnionFind uf;

        OfflineDynamicConnectivity(const int n, const int q) :
                _n(n),
                _query_count(0),
                _size(1 << (bit_length(q-1))),
                _q(q),
                _bit(bit_length(n) + 1),
                _msk((1ll << (bit_length(n) + 1)) - 1),
                data(_size<<1),
                uf(n) {
            start.reserve(_q);
            edge_data.reserve(_q);
        }

        void add_edge(const int u, const int v) {
            auto [s, t] = minmax(u, v);
            long long edge = (long long)s<<_bit|t;
            if (start[edge].first == 0) {
                start[edge].second = _query_count;
            }
            ++start[edge].first;
        }

        void delete_edge(const int u, const int v) {
            auto [s, t] = minmax(u, v);
            long long edge = (long long)s<<_bit|t;
            auto it = start.find(edge);
            if (it->second.first == 1) {
                edge_data.emplace_back(it->second.second, _query_count, edge);
            }
            --it->second.first;
        }

        void next_query() {
            ++_query_count;
        }

        template<typename F> // void out(int k) {}
        void run(F &&out) {
            assert(_query_count <= _q);
            for (const auto &[edge, p]: start) {
                if (p.first != 0) {
                    _internal_add(p.second, _query_count, edge);
                }
            }
            for (const auto &[l, r, edge]: edge_data) {
                _internal_add(l, r, edge);
            }
            int size2 = _size<<1;
            int ptr = 0;
            int todo[bit_length(_q)<<2];
            todo[++ptr] = 1;
            while (ptr) {
                int v = todo[ptr--];
                if (v >= 0) {
                    for (const long long &uv: data[v]) {
                        uf.unite(uv>>_bit, uv&_msk);
                    }
                    todo[++ptr] = ~v;
                    if ((v<<1|1) < size2) {
                        todo[++ptr] = v<<1|1;
                        todo[++ptr] = v<<1;
                    } else if (v - _size < _query_count) {
                        out(v-_size);
                    }
                } else {
                    int s = data[~v].size();
                    for (int i = 0; i < s; ++i) {
                        uf.undo();
                    }
                }
            }
        }
    };
}  // namespace titan23
