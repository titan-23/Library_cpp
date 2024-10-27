#include <vector>
#include <random>
#include <cassert>

#include "titan_cpplib/data_structures/segment_tree.cpp"
using namespace std;

// HashString
namespace titan23 {

    class HashStringBase {
        // ref: https://qiita.com/keymoon/items/11fac5627672a6d6a9f6
      public:
        using u64 = unsigned long long;
        static constexpr const u64 MASK30 = (1ULL << 30) - 1;
        static constexpr const u64 MASK31 = (1ULL << 31) - 1;
        static constexpr const u64 MOD = (1ULL << 61) - 1;
        static constexpr const u64 MASK61 = (1ULL << 61) - 1;
        int n;
        u64 base, invpow;
        vector<u64> powb, invb;

        static u64 get_mul(u64 a, u64 b) {
            u64 au = a >> 31;
            u64 ad = a & MASK31;
            u64 bu = b >> 31;
            u64 bd = b & MASK31;
            u64 mid = ad * bu + au * bd;
            u64 midu = mid >> 30;
            u64 midd = mid & MASK30;
            return get_mod(au * bu * 2 + midu + (midd << 31) + ad * bd);
        }

        static u64 get_mod(u64 x) {
            u64 xu = x >> 61;
            u64 xd = x & MASK61;
            u64 res = xu + xd;
            if (res >= MOD) res -= MOD;
            return res;
        }

        u64 pow_mod(u64 a, u64 b, const u64 mod) {
            u64 res = 1ULL;
            while (b) {
                if (b & 1ULL) {
                res = get_mul(res, a);
                }
                a = get_mul(a, a);
                b >>= 1ULL;
            }
            return res;
        }

        HashStringBase(int n = 0) : n(n) {
            std::mt19937 rand(std::random_device{}());
            u64 base = std::uniform_int_distribution<>(414123123, 1000000000)(rand);
            // u64 base = 414123123;
            this->base = base;
            this->invpow = pow_mod(base, HashStringBase::MOD-2, HashStringBase::MOD);
            powb.resize(n + 1, 1ULL);
            invb.resize(n + 1, 1ULL);
            for (int i = 1; i <= n; ++i) {
                powb[i] = get_mul(powb[i-1], base);
                invb[i] = get_mul(invb[i-1], this->invpow);
            }
        }

        void extend(const int cap) {
            assert(cap >= 0);
            int pre_cap = powb.size();
            powb.resize(pre_cap + cap, 0);
            invb.resize(pre_cap + cap, 0);
            for (int i = pre_cap; i < pre_cap + cap; ++i) {
                powb[i] = get_mul(powb[i-1], base);
                invb[i] = get_mul(invb[i-1], invpow);
            }
        }

        int get_cap() const {
            return powb.size();
        }

        u64 unite(u64 h1, u64 h2, int k) {
            if (k >= get_cap()) extend(k - get_cap() + 1);
            return get_mod(get_mul(h1, powb[k]) + h2);
        }
    };

    class HashString {
      private:
        using u64 = unsigned long long;
        HashStringBase* hsb;
        int n;
        bool used_seg;
        vector<u64> data, acc;

        static u64 op(u64 s, u64 t) { return (s+t) % HashStringBase::MOD; }
        static u64 e() { return 0; }
        titan23::SegmentTree<u64, op, e> seg;

      public:
        HashString() {}
        HashString(HashStringBase* hsb, const string &s) : n(s.size()), used_seg(false), hsb(hsb) {
            data.resize(n);
            acc.resize(n+1, 0);
            if (n > this->hsb->get_cap()) this->hsb->extend(n - this->hsb->get_cap());
            for (int i = 0 ; i < n; ++i) {
                assert(0 <= n-i-1 && n-i-1 < hsb->powb.size());
                data[i] = this->hsb->get_mul(this->hsb->powb[n-i-1], s[i]-'a'+1);
                acc[i+1] = this->hsb->get_mod(acc[i] + data[i]);
            }
            seg = titan23::SegmentTree<u64, HashString::op, HashString::e>(data);
        }

        u64 get(int l, int r) const {
            assert(0 <= l && l <= r && r <= n);
            if (used_seg) {
                return hsb->get_mul(seg.prod(l, r), hsb->invb[n - r]);
            }
            return hsb->get_mul(hsb->get_mod(4 * HashStringBase::MOD + acc[r] - acc[l]), hsb->invb[n - r]);
        }

        u64 get(int k) const {
            assert(0 <= k && k < n);
            return get(k, k+1);
        }

        void set(int k, char c) {
            assert(0 <= k && k < n);
            used_seg = true;
            seg.set(k, hsb->get_mul(hsb->powb[n-k-1], c-'a'+1));
        }

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
