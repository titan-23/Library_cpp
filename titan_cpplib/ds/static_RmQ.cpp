#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

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
        if (n == 0) return;
        int bucket_cnt = (n + W - 1) / W;
        bucket.assign(W * bucket_cnt, INF);
        copy(a.begin(), a.end(), bucket.begin());
        bucket_bit.assign(W * bucket_cnt, 0);
        vector<T> init(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            const int start = i * W;
            u64 mask = 0;
            T mn = bucket[start];
            for (int j = 0; j < W; ++j) {
                while (mask) {
                    int p = 63 - __builtin_clzll(mask);
                    if (bucket[start + p] > bucket[start + j]) {
                        mask ^= (1ull << p);
                    } else {
                        break;
                    }
                }
                mask |= (1ull << j);
                bucket_bit[start + j] = mask;
                mn = min(mn, bucket[start + j]);
            }
            init[i] = mn;
        }
        build_sparse_table(init);
    }

    // min a[l, r) / O(1)
    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return INF;
        const int k1 = l / W;
        const int k2 = (r - 1) / W;
        const int rem_l = l & 63;
        const int rem_r = (r - 1) & 63;
        if (k1 == k2) {
            const u64 mask = bucket_bit[k1 * W + rem_r] & ~((1ull << rem_l) - 1);
            return bucket[k1 * W + __builtin_ctzll(mask)];
        }
        const u64 maskL = bucket_bit[k1 * W + 63] & ~((1ull << rem_l) - 1);
        const u64 maskR = bucket_bit[k2 * W + rem_r];
        T ans = min(
            bucket[k1 * W + __builtin_ctzll(maskL)],
            bucket[k2 * W + __builtin_ctzll(maskR)]
        );
        return min(ans, prod_sp(k1 + 1, k2));
    }
};
} // namespace titan23
