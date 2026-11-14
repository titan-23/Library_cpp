/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/string/hash_string.cpp
#pragma once

#include <vector>
#include <random>
#include <cassert>
#include "titan_cpplib/ds/segment_tree.cpp"
using namespace std;

namespace titan23 {

/// @brief mod 2^61-1 のローリングハッシュの基数と累乗表を持つ / HashString で共有する
/// 参考: https://qiita.com/keymoon/items/11fac5627672a6d6a9f6
class HashStringBase {
public:
    using u64 = unsigned long long;
    using u128 = __uint128_t;
    static constexpr const u64 MOD = (1ULL << 61) - 1;
    int n;
    u64 base, invpow;
    vector<u64> powb, invb;

    /// @brief a * b を MOD で割った余りを返す / a, b < MOD の前提 / O(1)
    static u64 mul(u64 a, u64 b) {
        u128 t = (u128)a * b;
        t = (t>>61) + (t&MOD);
        if (t >= MOD) t -= MOD;
        return t;
    }

    /// @brief a を MOD で割った余りを返す / O(1)
    static u64 mod(u64 a) {
        a = (a>>61) + (a&MOD);
        if (a >= MOD) a -= MOD;
        return a;
    }

    /// @brief a^b を MOD で割った余りを返す / O(log b)
    u64 pow_mod(u64 a, u64 b) {
        u64 res = 1ULL;
        while (b) {
            if (b & 1ULL) {
                res = mul(res, a);
            }
            a = mul(a, a);
            b >>= 1ULL;
        }
        return res;
    }

    /// @brief 基数を乱択し、長さ n までの累乗表を作る / O(n)
    HashStringBase(int n = 0) : n(n) {
        std::mt19937 rand(std::random_device{}());
        u64 base = std::uniform_int_distribution<>(414123123, 1000000000)(rand);
        // u64 base = 414123123;
        this->base = base;
        this->invpow = pow_mod(base, HashStringBase::MOD-2);
        powb.resize(n + 1, 1ULL);
        invb.resize(n + 1, 1ULL);
        for (int i = 1; i <= n; ++i) {
            powb[i] = mul(powb[i-1], base);
            invb[i] = mul(invb[i-1], this->invpow);
        }
    }

    /// @brief 累乗表を cap 個伸ばす / O(cap)
    void extend(const int cap) {
        assert(cap >= 0);
        int pre_cap = powb.size();
        powb.resize(pre_cap + cap, 0);
        invb.resize(pre_cap + cap, 0);
        for (int i = pre_cap; i < pre_cap + cap; ++i) {
            powb[i] = mul(powb[i-1], base);
            invb[i] = mul(invb[i-1], invpow);
        }
    }

    /// @brief 累乗表の長さを返す / O(1)
    int get_cap() const {
        return powb.size();
    }

    /// @brief ハッシュが h1 の列の後ろに、ハッシュが h2 で長さ k の列をつなげた列のハッシュを返す
    u64 unite(u64 h1, u64 h2, int k) {
        if (k >= get_cap()) extend(k - get_cap() + 1);
        u64 t = mul(h1, powb[k]) + h2;
        if (t >= MOD) t -= MOD;
        return t;
    }
};

/// @brief 文字列の区間のハッシュを返す / 1 文字の更新もできる / 文字は 'a' 以上を前提とする
class HashString {
private:
    using u64 = unsigned long long;
    HashStringBase* hsb;
    int n;
    bool used_seg;
    vector<u64> data, acc;

    static u64 op(u64 s, u64 t) {
        u64 u = s + t;
        if (u > HashStringBase::MOD) u -= HashStringBase::MOD;
        return u;
    }
    static u64 e() { return 0; }
    titan23::SegmentTree<u64, op, e> seg;

public:
    HashString() {}
    /// @brief hsb の基数で s のハッシュを前計算する / O(n)
    HashString(HashStringBase* hsb, const string &s) : n(s.size()), used_seg(false), hsb(hsb) {
        data.resize(n);
        acc.resize(n+1, 0);
        if (n > this->hsb->get_cap()) this->hsb->extend(n - this->hsb->get_cap() + 1);
        for (int i = 0 ; i < n; ++i) {
            assert(0 <= n-i-1 && n-i-1 < hsb->powb.size());
            data[i] = this->hsb->mul(this->hsb->powb[n-i-1], s[i]-'a'+1);
            acc[i+1] = this->hsb->mod(acc[i] + data[i]);
        }
        seg = titan23::SegmentTree<u64, HashString::op, HashString::e>(data);
    }

    /// @brief s[l, r) のハッシュを返す / set を呼ぶ前は O(1)、呼んだ後は O(log n)
    u64 get(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (used_seg) {
            return hsb->mul(seg.prod(l, r), hsb->invb[n - r]);
        }
        return hsb->mul(hsb->mod(4 * HashStringBase::MOD + acc[r] - acc[l]), hsb->invb[n - r]);
    }

    /// @brief s[k] のハッシュを返す
    u64 get(int k) const {
        assert(0 <= k && k < n);
        return get(k, k+1);
    }

    /// @brief s[k] を c に変更する / O(log n)
    void set(int k, char c) {
        assert(0 <= k && k < n);
        used_seg = true;
        seg.set(k, hsb->mul(hsb->powb[n-k-1], c-'a'+1));
    }

    /// @brief 各 i について s と s[i:] の最長共通接頭辞の長さを二分探索で求めて返す
    vector<int> get_lcp() const {
        vector<int> a(n);
        for (int i = 0; i < n; ++i) {
            int ok = 0, ng = n - i + 1;
                while (ng - ok > 1) {
                int mid = (ok + ng) / 2;
                (get(0, mid) == get(i, i + mid) ? ok : ng) = mid;
            }
            a[i] = ok;
        }
        return a;
    }
};
} // namespace titan23
