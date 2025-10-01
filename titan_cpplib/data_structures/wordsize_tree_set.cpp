#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

class WordsizeTreeSet {
private:
    using u64 = unsigned long long;
    int u, _len;
    vector<vector<u64>> data;

    inline constexpr int bit_length(const u64 x) const {
        return x ? 64-__builtin_clzll(x) : 0;
    }

    void _build(int u, vector<u64> &A) {
        data[0] = A;
        int dep = 1;
        while (u) {
            data[dep] = vector<u64>((u>>6)+1);
            auto &a = data[dep];
            const auto &b = data[dep-1];
            for (int i = 0; i <= u; ++i) {
                if (b[i]) {
                    a[i>>6] |= 1ull << (i&63);
                }
            }
            dep++;
            u >>= 6;
        }
    }

    int get_dep(int u) const {
        int dep = 1;
        while (u) {
            u >>= 6;
            dep++;
        }
        return dep;
    }

public:
    WordsizeTreeSet(int u) : u(u+1), _len(0) {
        while (u) {
            u >>= 6;
            data.emplace_back(u+1);
        }
    }

    WordsizeTreeSet(int u, const string &s) : u(u+1), _len(0) {
        u >>= 6;
        data.resize(get_dep(u));
        vector<u64> A(u+1);
        int n = s.size();
        for (int i = 0; i < n; ++i) {
            if (s[i] == '1') {
                ++_len;
                A[i>>6] |= 1ull << (i&63);
            }
        }
        _build(u, A);
    }

    WordsizeTreeSet(int u, const vector<int> &a) : u(u+1), _len(0) {
        u >>= 6;
        data.resize(get_dep(u));
        vector<u64> A(u+1);
        for (int e : a) {
            if (((A[e>>6] >> (e&63)) & 1) == 0) {
                ++_len;
                A[e>>6] |= 1ull << (e&63);
            }
        }
        _build(u, A);
    }

    bool add(int v) {
        assert(0 <= v && v < u);
        if ((data[0][v>>6] >> (v&63)) & 1) return false;
        _len++;
        for (vector<u64> &a : data) {
            a[v>>6] |= 1ull << (v&63);
            v >>= 6;
        }
        return true;
    }

    bool discard(int v) {
        if (((data[0][v >> 6] >> (v & 63) & 1)) == 0) return false;
        _len--;
        for (vector<u64> &a : data) {
            a[v>>6] &= ~(1ull << (v & 63));
            v >>= 6;
            if (a[v]) break;
        }
        return true;
    }

    void remove(u64 v) {
        if (!discard(v)) {
            assert(false);
        }
    }

    int ge(int v) {
        int d = 0;
        while (1) {
            if (d >= (int)data.size() || ((v>>6) >= data[d].size())) { return -1; }
            u64 m = data[d][v>>6] & ((~(u64)0) << (v & 63));
            if (m == 0) {
                d++;
                v = (v >> 6) + 1;
            } else {
                v = (v >> 6 << 6) + bit_length(m & -m);
                v--;
                if (d == 0) break;
                v <<= 6;
                d--;
            }
        }
        return v;
    }

    int gt(int v) {
        return ge(v+1);
    }

    int le(int v) {
        int d = 0;
        while (1) {
            if (v < 0 || d >= (int)data.size()) return -1;
            u64 m = data[d][v >> 6] & ~((~1ull) << (v & 63));
            if (m == 0) {
                d++;
                v = (v >> 6) - 1;
            } else {
                v = (v >> 6 << 6) + bit_length(m) - 1;
                if (d == 0) break;
                v <<= 6;
                v += 63;
                --d;
            }
        }
        return v;
    }

    int lt(int v) {
        return le(v+1);
    }

    bool contains(int v) {
        return (data[0][v>>6] >> (v&63) & 1) == 1;
    }

    int len() { return _len; }

    vector<int> tovector() {
        int v = 0;
        vector<int> a(_len);
        int idx = 0;
        if (contains(v)) {
            a[0] = 0;
            idx++;
            v = gt(v);
        }
        while (1) {
            int nxt = gt(v);
            if (nxt == -1) break;
            v = nxt;
            a[idx] = 0;
            idx++;
        }
        return a;
    }
};

} // namespace titan23
