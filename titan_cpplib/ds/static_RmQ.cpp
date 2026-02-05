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
    int n;
    T INF;
    static constexpr const int W = 64;
    vector<T> bucket;
    vector<u64> bucket_bit;
    vector<T> sp_data;
    vector<int> offset;

    static int bit_length(const u64 x) {
        return x == 0 ? 0 : 64 - __builtin_clzll(x);
    }

    void build_sparse_table(const vector<T> &a) {
        int sp_n = a.size();
        int log = 32 - __builtin_clz(sp_n) - 1;
        offset.resize(log+1);
        int sm = 0;
        for (int i = 0; i <= log; ++i) {
            offset[i] = sm;
            sm += sp_n - (1<<i) + 1;
        }
        sp_data.resize(sm);
        copy(a.begin(), a.end(), sp_data.begin());
        for (int i = 0; i < log; ++i) {
            int l = 1 << i;
            int s = sp_n - l + 1;
            const int x = s - l;
            const auto pre1 = &sp_data[offset[i]];
            const auto pre2 = &sp_data[offset[i]+l];
            auto nxt = &sp_data[offset[i+1]];
            for (int j = 0; j < x; ++j) {
                nxt[j] = min(pre1[j], pre2[j]);
            }
        }
    }

    T prod_sp(const int l, const int r) const {
        if (l == r) return INF;
        int u = 32 - __builtin_clz(r-l) - 1;
        return min(sp_data[offset[u]+l], sp_data[offset[u]+r-(1<<u)]);
    }

public:
    StaticRmQ() {}
    StaticRmQ(const vector<T> &a, T INF) : n(a.size()), INF(INF) {
        int bucket_cnt = (n + W - 1) / W;
        bucket.resize(W*bucket_cnt, INF);
        copy(a.begin(), a.end(), bucket.begin());
        vector<T> init(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            init[i] = *min_element(bucket.begin()+i*W, bucket.begin()+(i+1)*W);
        }
        build_sparse_table(init);
        bucket_bit.resize(W*bucket_cnt, 0);

        int s[65];
        int ptr;
        for (int i = 0; i < bucket_cnt; ++i) {
            ptr = 0;
            const int start = W*i;
            for (int j = 0; j < W; ++j) {
                int g = -1;
                while (ptr && bucket[start+s[ptr-1]] > bucket[start+j]) {
                    ptr--;
                }
                if (ptr) g = s[ptr-1];
                s[ptr] = j; ptr++;
                if (g == -1) continue;
                bucket_bit[i*W+j] = bucket_bit[i*W+g] | (1ull<<g);
            }
        }
    }

    // min a[l, r) / O(1)
    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return INF;
        const int k1 = l / W;
        const int k2 = (r-1) / W;
        l -= k1 * W;
        r -= k2 * W + 1;
        if (k1 == k2) {
            const u64 bit = bucket_bit[k1*W+r] >> l;
            return bucket[k1*W + (bit ? bit_length(bit&(-bit))+l-1 : r)];
        }
        const u64 bitL = bucket_bit[k1*W+W-1] >> l;
        const u64 bitR = bucket_bit[k2*W+r];
        T ans = min(
            bucket[k1*W + (bitL ? bit_length(bitL&(-bitL))+l-1 : W-1)],
            bucket[k2*W + (bitR ? bit_length(bitR&(-bitR))-1 : r)]
        );
        ans = min(ans, prod_sp(k1+1, k2));
        return ans;
    }
};
} // namespace
