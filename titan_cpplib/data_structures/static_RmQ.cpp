#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstring>
using namespace std;

namespace titan23 {

template<typename T>
class StaticRmQ {
private:
    using u64 = unsigned long long;
    class SparseTableRmQ;
    int n;
    T INF;
    int bucket_size;
    vector<vector<T>> bucket;
    SparseTableRmQ bucket_data;
    vector<u64> bucket_bit;

    static int bit_length(const u64 x) {
        return x == 0 ? 0 : 64 - __builtin_clzll(x);
    }

    class SparseTableRmQ {
    private:
        int n;
        T INF;
        vector<T> data;
        vector<int> offset;

    public:
        SparseTableRmQ() {}
        SparseTableRmQ(const vector<T> &a, T INF) : n((int)a.size()), INF(INF) {
            int log = 32 - __builtin_clz(n) - 1;
            offset.resize(log+1);
            int sm = 0;
            for (int i = 0; i <= log; ++i) {
                offset[i] = sm;
                sm += n - (1<<i) + 1;
            }
            data.resize(sm);
            copy(a.begin(), a.end(), data.begin());
            for (int i = 0; i < log; ++i) {
                int l = 1 << i;
                int s = n - l + 1;
                const int x = s - l;
                const auto pre1 = &data[offset[i]];
                const auto pre2 = &data[offset[i]+l];
                auto nxt = &data[offset[i+1]];
                for (int j = 0; j < x; ++j) {
                    nxt[j] = min(pre1[j], pre2[j]);
                }
            }
        }

        T prod(const int l, const int r) const {
            assert(0 <= l && l <= r && r < n);
            if (l == r) return INF;
            int u = 32 - __builtin_clz(r-l) - 1;
            return min(data[offset[u]+l], data[offset[u]+r-(1<<u)]);
        }
    };

public:
    StaticRmQ() {}
    StaticRmQ(const vector<T> &a, T INF) : n(a.size()), INF(INF), bucket_size(63) {
        int bucket_cnt = (n + bucket_size - 1) / bucket_size;
        bucket.resize(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            bucket[i] = vector<T>(a.begin()+bucket_size*i, a.begin()+min(n, bucket_size*(i+1)));
        }
        vector<T> init(bucket.size());
        for (int i = 0; i < bucket_cnt; ++i) {
            init[i] = *min_element(bucket[i].begin(), bucket[i].end());
        }
        bucket_data = SparseTableRmQ(init, INF);
        bucket_bit.resize(bucket_size*bucket_cnt, 0);

        int s[65];
        int ptr;
        for (int i = 0; i < bucket_cnt; ++i) {
            ptr = 0;
            const vector<T> &b = bucket[i];
            for (int j = 0; j < b.size(); ++j) {
                int g = -1;
                while (ptr && b[s[ptr-1]] > b[j]) {
                    ptr--;
                }
                if (ptr) g = s[ptr-1];
                s[ptr] = j; ptr++;
                if (g == -1) continue;
                bucket_bit[i*bucket_size+j] = bucket_bit[i*bucket_size+g] | (1ull<<g);
            }
        }
    }

    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return INF;
        int k1 = l / bucket_size;
        int k2 = r / bucket_size;
        l -= k1 * bucket_size;
        r -= k2 * bucket_size + 1;
        if (k1 == k2) {
            u64 bit = bucket_bit[k1*bucket_size+r] >> l;
            return bucket[k1][bit ? bit_length(bit&(-bit))+l-1 : r];
        }
        T ans = INF;
        if (l < bucket[k1].size()) {
            u64 bit = bucket_bit[k1*bucket_size+bucket[k1].size()-1] >> l;
            ans = bucket[k1][bit ? bit_length(bit&(-bit))+l-1 : (int)bucket[k1].size()-1];
        }
        ans = min(ans, bucket_data.prod(k1+1, k2));
        if (r >= 0) {
            u64 bit = bucket_bit[k2*bucket_size+r];
            ans = min(ans, bucket[k2][bit ? bit_length(bit&(-bit))-1 : r]);
        }
        return ans;
    }
};
} // namespace
