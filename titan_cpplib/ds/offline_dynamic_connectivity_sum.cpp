#include <vector>
#include <cassert>
#include <unordered_map>
#include "titan_cpplib/ds/undoable_union_find_sum.cpp"
using namespace std;

// OfflineDynamicConnectivitySum
namespace titan23 {

template<typename T>
class OfflineDynamicConnectivitySum {
private:
    int n, query_count, size, q;
    long long bit, msk;
    long long bits, msks;
    unordered_map<long long, long long> start;
    vector<vector<long long>> data;
    vector<pair<long long, long long>> edge_data;

    int bit_length(const int n) const {
        return n ? 32 - __builtin_clz(n) : 0;
    }

    void _internal_add(int l, int r, const long long edge) {
        l += size;
        r += size;
        while (l < r) {
            if ((l & 1) && (l   < size+q)) data[l++].emplace_back(edge);
            if ((r & 1) && (r-1 < size+q)) data[--r].emplace_back(edge);
            l >>= 1;
            r >>= 1;
        }
    }

public:
    titan23::UndoableUnionFindSum<T> uf;

    OfflineDynamicConnectivitySum(const int n, const int q, T e) :
            n(n),
            query_count(0),
            size(-1),
            q(q),
            bit(bit_length(n) + 1),
            msk((1ll << (bit_length(n) + 1)) - 1),
            uf(n, e) {
        bits = max(31, bit_length(max(n, q)) + 1);
        msks = (1ll << bits) - 1;
    }

    void reserve(int cap) {
        start.reserve(cap);
        edge_data.reserve(cap);
    }

    void add_edge(const int u, const int v) {
        auto [s, t] = minmax(u, v);
        long long edge = (long long)s<<bit|t;
        auto it = start.find(edge);
        if (it == start.end()) {
            start[edge] = 1ll << bits | query_count;
        } else {
            int a = it->second >> bits, b = it->second & msks;
            if (a == 0) {
                it->second = 1ll << bits | query_count;
            } else {
                it->second = (long long)(a+1) << bits | b;
            }
        }
    }

    void delete_edge(const int u, const int v) {
        auto [s, t] = minmax(u, v);
        long long edge = (long long)s<<bit|t;
        auto it = start.find(edge);
        int a = it->second >> bits, b = it->second & msks;
        if (a == 1) {
            edge_data.emplace_back((long long)b<<bits|query_count, edge);
        }
        it->second = (long long)(a-1) << bits | b;
    }

    void next_query() {
        ++query_count;
    }

    template<typename F> // void out(int k) {}
    void run(F &&out) {
        assert(query_count <= q);
        size = 1 << bit_length(query_count),
        data.resize(size<<1);
        for (const auto &[edge, p]: start) {
            int a = p >> bits, b = p & msks;
            if (a != 0) {
                _internal_add(b, query_count, edge);
            }
        }
        for (const auto &[lr, edge] : edge_data) {
            _internal_add(lr>>bits, lr&msks, edge);
        }
        int size2 = size<<1;
        int ptr = 0;
        int todo[bit_length(query_count)<<2];
        todo[++ptr] = 1;
        while (ptr) {
            int v = todo[ptr--];
            if (v >= 0) {
                for (const long long &uv: data[v]) {
                    uf.unite(uv>>bit, uv&msk);
                }
                todo[++ptr] = ~v;
                if ((v<<1|1) < size2) {
                    todo[++ptr] = v<<1|1;
                    todo[++ptr] = v<<1;
                } else if (v - size < query_count) {
                    out(v-size);
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
