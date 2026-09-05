/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/big_int.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstdint>
#include <istream>
#include <limits>
#include <numeric>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <atcoder/convolution>

using namespace std;

namespace titan23 {

class BigInt {
private:
    using u32 = uint32_t;
    using u64 = uint64_t;
    using i128 = __int128_t;
    using u128 = __uint128_t;
    using Digits = vector<u32>;

    static constexpr u32 BASE = 100000000;
    static constexpr u32 BASE4 = 10000;
    static constexpr size_t MUL_NAIVE = 24;
    static constexpr size_t MUL_KARA = 36;
    static constexpr size_t SQR_MIN = 2;
    static constexpr size_t SQR_KARA = 76;
    static constexpr size_t MUL_ACC = numeric_limits<u64>::max() / (u64(BASE) * (BASE - 1));
    static constexpr u64 NTT_COST = 9;
    static constexpr size_t DIV_BZ = 32;
    static constexpr size_t DIV_BZ_GAP = 8;
    static constexpr size_t DIV_BZ_WORK = 1280;
    static constexpr unsigned int NTT_MOD1 = 469762049;
    static constexpr unsigned int NTT_MOD2 = 998244353;
    static constexpr u64 NTT_INV = 554580198;
    static constexpr size_t NTT_MAX = size_t{1} << 23;

    int sign_ = 0;
    Digits digits_;

    BigInt(int s, Digits a) : sign_(s), digits_(move(a)) { normalize(); }

    static void trim(Digits &a) {
        while (!a.empty() && a.back() == 0) a.pop_back();
    }

    void normalize() {
        trim(digits_);
        if (digits_.empty()) sign_ = 0;
    }

    static int cmp_abs(const Digits &a, const Digits &b) {
        if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        for (size_t i = a.size(); i-- > 0;) {
            if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        }
        return 0;
    }

    static Digits add_abs(const Digits &a, const Digits &b) {
        const size_t n = max(a.size(), b.size());
        Digits res;
        res.reserve(n + 1);
        res.resize(n);
        u64 carry = 0;
        const size_t m = min(a.size(), b.size());
        size_t i = 0;
        for (; i < m; ++i) {
            const u64 cur = u64(a[i]) + b[i] + carry;
            if (cur >= BASE) {
                res[i] = static_cast<u32>(cur - BASE);
                carry = 1;
            } else {
                res[i] = static_cast<u32>(cur);
                carry = 0;
            }
        }
        const Digits &c = a.size() < b.size() ? b : a;
        for (; i < n; ++i) {
            const u64 cur = u64(c[i]) + carry;
            if (cur >= BASE) {
                res[i] = static_cast<u32>(cur - BASE);
                carry = 1;
            } else {
                res[i] = static_cast<u32>(cur);
                carry = 0;
            }
        }
        if (carry != 0) res.push_back(1);
        return res;
    }

    // Requires a >= b.
    static Digits sub_abs(const Digits &a, const Digits &b) {
        assert(cmp_abs(a, b) >= 0);
        Digits res(a.size());
        u64 borrow = 0;
        size_t i = 0;
        for (; i < b.size(); ++i) {
            const u64 sub = u64(b[i]) + borrow;
            if (a[i] < sub) {
                res[i] = static_cast<u32>(u64(a[i]) + BASE - sub);
                borrow = 1;
            } else {
                res[i] = static_cast<u32>(u64(a[i]) - sub);
                borrow = 0;
            }
        }
        for (; i < a.size(); ++i) {
            if (borrow != 0 && a[i] == 0) {
                res[i] = BASE - 1;
            } else {
                res[i] = a[i] - static_cast<u32>(borrow);
                borrow = 0;
            }
        }
        assert(borrow == 0);
        trim(res);
        return res;
    }

    static void add_to(Digits &a, const Digits &b) {
        const size_t n = max(a.size(), b.size());
        a.reserve(n + 1);
        a.resize(n);
        u64 carry = 0;
        size_t i = 0;
        for (; i < b.size(); ++i) {
            const u64 cur = u64(a[i]) + b[i] + carry;
            if (cur >= BASE) {
                a[i] = static_cast<u32>(cur - BASE);
                carry = 1;
            } else {
                a[i] = static_cast<u32>(cur);
                carry = 0;
            }
        }
        while (carry != 0 && i < n) {
            if (a[i] == BASE - 1) {
                a[i++] = 0;
            } else {
                ++a[i];
                carry = 0;
            }
        }
        if (carry != 0) a.push_back(1);
    }

    static void sub_to(Digits &a, const Digits &b) {
        u64 borrow = 0;
        size_t i = 0;
        for (; i < b.size(); ++i) {
            const u64 sub = u64(b[i]) + borrow;
            if (a[i] < sub) {
                a[i] = static_cast<u32>(u64(a[i]) + BASE - sub);
                borrow = 1;
            } else {
                a[i] = static_cast<u32>(u64(a[i]) - sub);
                borrow = 0;
            }
        }
        while (borrow != 0) {
            assert(i < a.size());
            if (a[i] == 0) {
                a[i++] = BASE - 1;
            } else {
                --a[i];
                borrow = 0;
            }
        }
        trim(a);
    }

    static Digits mul_small(const Digits &a, u32 b) {
        if (a.empty() || b == 0) return {};
        Digits res;
        res.reserve(a.size() + 1);
        res.resize(a.size());
        u64 carry = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            const u64 cur = u64(a[i]) * b + carry;
            res[i] = static_cast<u32>(cur % BASE);
            carry = cur / BASE;
        }
        if (carry != 0) res.push_back(static_cast<u32>(carry));
        return res;
    }

    static u64 div_inv(u64 x, u32 b, u64 inv) {
        u64 q = static_cast<u64>(u128(x) * inv >> 64);
        if (q * b > x) --q;
        return q;
    }

    static pair<Digits, u32> div_small(const Digits &a, u32 b) {
        assert(b != 0);
        if (b == 1) return {a, 0};
        Digits q(a.size());
        u64 r = 0;
        if (a.size() <= 4) {
            for (size_t i = a.size(); i-- > 0;) {
                const u64 cur = r * BASE + a[i];
                q[i] = static_cast<u32>(cur / b);
                r = cur % b;
            }
            trim(q);
            return {move(q), static_cast<u32>(r)};
        }
        const u64 inv = numeric_limits<u64>::max() / b + 1;
        for (size_t i = a.size(); i-- > 0;) {
            const u64 cur = r * BASE + a[i];
            const u64 x = div_inv(cur, b, inv);
            q[i] = static_cast<u32>(x);
            r = cur - x * b;
        }
        trim(q);
        return {move(q), static_cast<u32>(r)};
    }

    static u32 rem_small(const Digits &a, u32 b) {
        assert(b != 0);
        if (b == 1) return 0;
        u64 r = 0;
        if (a.size() <= 4) {
            for (size_t i = a.size(); i-- > 0;) r = (r * BASE + a[i]) % b;
            return static_cast<u32>(r);
        }
        const u64 inv = numeric_limits<u64>::max() / b + 1;
        for (size_t i = a.size(); i-- > 0;) {
            const u64 cur = r * BASE + a[i];
            const u64 q = div_inv(cur, b, inv);
            r = cur - q * b;
        }
        return static_cast<u32>(r);
    }

    static Digits lin_abs(const Digits &a, const Digits &b, i128 x, i128 y) {
        const size_t n = max(a.size(), b.size());
        Digits res;
        res.reserve(n + 2);
        i128 carry = 0;
        for (size_t i = 0; i < n; ++i) {
            i128 cur = carry;
            if (i < a.size()) cur += x * a[i];
            if (i < b.size()) cur += y * b[i];
            carry = cur / BASE;
            cur %= BASE;
            if (cur < 0) {
                cur += BASE;
                --carry;
            }
            res.push_back(static_cast<u32>(cur));
        }
        assert(carry >= 0);
        while (carry != 0) {
            res.push_back(static_cast<u32>(carry % BASE));
            carry /= BASE;
        }
        trim(res);
        return res;
    }

    static Digits mul_naive(const Digits &a, const Digits &b) {
        if (a.empty() || b.empty()) return {};
        if (a.size() <= MUL_ACC) {
            vector<u64> c(a.size() + b.size());
            for (size_t i = 0; i < a.size(); ++i) {
                for (size_t j = 0; j < b.size(); ++j) c[i + j] += u64(a[i]) * b[j];
            }
            return carry_base(c);
        }
        Digits res(a.size() + b.size());
        for (size_t i = 0; i < a.size(); ++i) {
            u64 carry = 0;
            for (size_t j = 0; j < b.size(); ++j) {
                const u64 cur = u64(a[i]) * b[j] + res[i + j] + carry;
                res[i + j] = static_cast<u32>(cur % BASE);
                carry = cur / BASE;
            }
            size_t p = i + b.size();
            while (carry != 0) {
                const u64 cur = u64(res[p]) + carry;
                res[p] = static_cast<u32>(cur % BASE);
                carry = cur / BASE;
                ++p;
                if (p == res.size() && carry != 0) res.push_back(0);
            }
        }
        trim(res);
        return res;
    }

    static Digits square_naive(const Digits &a) {
        if (a.empty()) return {};
        if (a.size() <= MUL_ACC) {
            vector<u64> c(a.size() * 2);
            for (size_t i = 0; i < a.size(); ++i) {
                c[i * 2] += u64(a[i]) * a[i];
                for (size_t j = i + 1; j < a.size(); ++j) c[i + j] += u64(2) * a[i] * a[j];
            }
            return carry_base(c);
        }
        vector<i128> c(a.size() * 2);
        for (size_t i = 0; i < a.size(); ++i) {
            c[i * 2] += i128(a[i]) * a[i];
            for (size_t j = i + 1; j < a.size(); ++j) c[i + j] += i128(2) * a[i] * a[j];
        }
        return carry_base(c);
    }

    [[gnu::hot, gnu::aligned(32)]] static void karatsuba(const i128 *a, const i128 *b, size_t n, i128 *res,
                                                        i128 *buf) {
        fill(res, res + n * 2, i128(0));
        if (n <= MUL_NAIVE) {
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j) {
                    res[i + j] += i128(static_cast<u64>(a[i])) * static_cast<u64>(b[j]);
                }
            }
            return;
        }

        const size_t h = n / 2;
        const size_t k = n - h;
        i128 *sa = buf;
        i128 *sb = buf + k;
        i128 *mid = buf + k * 2;
        i128 *tmp = buf + k * 4;
        for (size_t i = 0; i < k; ++i) {
            sa[i] = a[h + i] + (i < h ? a[i] : 0);
            sb[i] = b[h + i] + (i < h ? b[i] : 0);
        }

        karatsuba(a, b, h, res, tmp);
        karatsuba(a + h, b + h, k, res + h * 2, tmp);
        karatsuba(sa, sb, k, mid, tmp);
        for (size_t i = 0; i < k * 2; ++i) {
            if (i < h * 2) mid[i] -= res[i];
            mid[i] -= res[h * 2 + i];
        }
        for (size_t i = 0; i < k * 2; ++i) res[h + i] += mid[i];
    }

    [[gnu::hot, gnu::aligned(32)]] static void karatsuba_sq(const i128 *a, size_t n, i128 *res, i128 *buf) {
        fill(res, res + n * 2, i128(0));
        if (n <= MUL_NAIVE) {
            for (size_t i = 0; i < n; ++i) {
                res[i * 2] += i128(static_cast<u64>(a[i])) * static_cast<u64>(a[i]);
                for (size_t j = i + 1; j < n; ++j) {
                    res[i + j] += (i128(static_cast<u64>(a[i])) * static_cast<u64>(a[j])) * 2;
                }
            }
            return;
        }

        const size_t h = n / 2;
        const size_t k = n - h;
        i128 *sa = buf;
        i128 *mid = buf + k;
        i128 *tmp = buf + k * 3;
        for (size_t i = 0; i < k; ++i) sa[i] = a[h + i] + (i < h ? a[i] : 0);

        karatsuba_sq(a, h, res, tmp);
        karatsuba_sq(a + h, k, res + h * 2, tmp);
        karatsuba_sq(sa, k, mid, tmp);
        for (size_t i = 0; i < k * 2; ++i) {
            if (i < h * 2) mid[i] -= res[i];
            mid[i] -= res[h * 2 + i];
        }
        for (size_t i = 0; i < k * 2; ++i) res[h + i] += mid[i];
    }

    static Digits carry_base(const vector<u64> &a) {
        Digits res;
        res.reserve(a.size() + 1);
        u64 carry = 0;
        for (u64 x : a) {
            const u64 cur = x + carry;
            res.push_back(static_cast<u32>(cur % BASE));
            carry = cur / BASE;
        }
        while (carry != 0) {
            res.push_back(static_cast<u32>(carry % BASE));
            carry /= BASE;
        }
        trim(res);
        return res;
    }

    static Digits carry_base(const vector<i128> &a) {
        Digits res;
        res.reserve(a.size() + 1);
        i128 carry = 0;
        for (i128 x : a) {
            const i128 cur = x + carry;
            assert(cur >= 0);
            const u128 v = static_cast<u128>(cur);
            const u64 lo = static_cast<u64>(v);
            const u64 hi = static_cast<u64>(v >> 64);
            constexpr u64 q64 = numeric_limits<u64>::max() / BASE;
            constexpr u64 r64 = numeric_limits<u64>::max() % BASE + 1;
            if (hi == 0) {
                res.push_back(static_cast<u32>(lo % BASE));
                carry = lo / BASE;
            } else if (hi <= (numeric_limits<u64>::max() - (BASE - 1)) / r64) {
                const u64 q = lo / BASE;
                const u64 r = hi * r64 + lo % BASE;
                res.push_back(static_cast<u32>(r % BASE));
                carry = static_cast<i128>(u128(hi) * q64 + q + r / BASE);
            } else {
                res.push_back(static_cast<u32>(v % BASE));
                carry = static_cast<i128>(v / BASE);
            }
        }
        while (carry != 0) {
            res.push_back(static_cast<u32>(carry % BASE));
            carry /= BASE;
        }
        trim(res);
        return res;
    }

    static size_t kara_buf(size_t n, size_t c) {
        size_t s = 0;
        while (n > MUL_NAIVE) {
            n -= n / 2;
            s += c * n;
        }
        return s;
    }

    static Digits mul_karatsuba(const Digits &a, const Digits &b, bool sq) {
        const size_t n = max(a.size(), b.size());
        vector<i128> aa(n), c(n * 2), buf(kara_buf(n, sq ? 3 : 4));
        for (size_t i = 0; i < a.size(); ++i) aa[i] = a[i];
        if (sq) {
            karatsuba_sq(aa.data(), n, c.data(), buf.data());
        } else {
            vector<i128> bb(n);
            for (size_t i = 0; i < b.size(); ++i) bb[i] = b[i];
            karatsuba(aa.data(), bb.data(), n, c.data(), buf.data());
        }
        return carry_base(c);
    }

    static vector<int> split4(const Digits &a) {
        vector<int> res;
        res.reserve(a.size() * 2);
        for (u32 x : a) {
            res.push_back(static_cast<int>(x % BASE4));
            res.push_back(static_cast<int>(x / BASE4));
        }
        while (!res.empty() && res.back() == 0) res.pop_back();
        return res;
    }

    template <unsigned int MOD> static vector<int> conv_sq(const vector<int> &a) {
        using Mint = atcoder::static_modint<MOD>;
        const size_t len = a.size() * 2 - 1;
        size_t z = 1;
        while (z < len) z *= 2;
        vector<Mint> f(z);
        for (size_t i = 0; i < a.size(); ++i) f[i] = a[i];
        atcoder::internal::butterfly(f);
        for (Mint &x : f) x *= x;
        atcoder::internal::butterfly_inv(f);
        const Mint iz = Mint(z).inv();
        vector<int> res(len);
        for (size_t i = 0; i < len; ++i) res[i] = (f[i] * iz).val();
        return res;
    }

    static Digits mul_ntt(const Digits &a, const Digits &b, bool sq) {
        const vector<int> a4 = split4(a);
        vector<int> c1;
        vector<int> c2;
        if (sq) {
            assert(a4.size() * 2 - 1 <= NTT_MAX);
            c1 = conv_sq<NTT_MOD1>(a4);
            c2 = conv_sq<NTT_MOD2>(a4);
        } else {
            const vector<int> b4 = split4(b);
            assert(!a4.empty() && !b4.empty());
            assert(a4.size() + b4.size() - 1 <= NTT_MAX);
            c1 = atcoder::convolution<NTT_MOD1>(a4, b4);
            c2 = atcoder::convolution<NTT_MOD2>(a4, b4);
        }
        Digits res;
        res.reserve((c1.size() + 5) / 2);
        u64 carry = 0;
        u32 lo = 0;
        for (size_t i = 0; i < c1.size(); ++i) {
            const u64 r1 = static_cast<unsigned int>(c1[i]);
            const u64 r2 = static_cast<unsigned int>(c2[i]);
            const u64 diff = r2 >= r1 ? r2 - r1 : r2 + NTT_MOD2 - r1;
            const u64 t = diff * NTT_INV % NTT_MOD2;
            const u64 x = r1 + u64(NTT_MOD1) * t;
            const u64 cur = x + carry;
            const u32 d = static_cast<u32>(cur % BASE4);
            if ((i & 1) == 0) {
                lo = d;
            } else {
                res.push_back(lo + d * BASE4);
            }
            carry = cur / BASE4;
        }
        size_t i = c1.size();
        while (carry != 0) {
            const u32 d = static_cast<u32>(carry % BASE4);
            if ((i & 1) == 0) {
                lo = d;
            } else {
                res.push_back(lo + d * BASE4);
            }
            carry /= BASE4;
            ++i;
        }
        if ((i & 1) != 0) res.push_back(lo);
        trim(res);
        return res;
    }

    static void add_shifted(Digits &res, const Digits &a, size_t k) {
        if (a.empty()) return;
        if (res.size() < k + a.size()) res.resize(k + a.size());
        u64 carry = 0;
        size_t i = 0;
        for (; i < a.size(); ++i) {
            const u64 cur = u64(res[k + i]) + a[i] + carry;
            res[k + i] = static_cast<u32>(cur % BASE);
            carry = cur / BASE;
        }
        size_t p = k + i;
        while (carry != 0) {
            if (p == res.size()) res.push_back(0);
            const u64 cur = u64(res[p]) + carry;
            res[p] = static_cast<u32>(cur % BASE);
            carry = cur / BASE;
            ++p;
        }
    }

    static Digits mul_block(const Digits &a, const Digits &b) {
        const size_t len = a.size();
        Digits res(a.size() + b.size());
        for (size_t k = 0; k < b.size(); k += len) {
            const size_t r = min(b.size(), k + len);
            Digits v(b.begin() + k, b.begin() + r);
            trim(v);
            add_shifted(res, mul_abs(a, v), k);
        }
        trim(res);
        return res;
    }

    static Digits mul_split(const Digits &a, const Digits &b, size_t k, bool sq) {
        const Digits a0 = low(a, k);
        const Digits a1 = high(a, k);
        Digits res(a.size() + b.size());
        if (sq) {
            add_shifted(res, mul_abs(a0, a0), 0);
            const Digits z = mul_abs(a0, a1);
            add_shifted(res, z, k);
            add_shifted(res, z, k);
            add_shifted(res, mul_abs(a1, a1), k * 2);
        } else {
            const Digits b0 = low(b, k);
            const Digits b1 = high(b, k);
            add_shifted(res, mul_abs(a0, b0), 0);
            add_shifted(res, mul_abs(a0, b1), k);
            add_shifted(res, mul_abs(a1, b0), k);
            add_shifted(res, mul_abs(a1, b1), k * 2);
        }
        trim(res);
        return res;
    }

    static u64 kara_raw(size_t n, bool sq) {
        if (n <= MUL_NAIVE) return sq ? u64(n) * (n + 1) / 2 : u64(n) * n;
        const size_t h = n / 2;
        const size_t k = n - h;
        return kara_raw(h, sq) + kara_raw(k, sq) * 2;
    }

    static u64 kara_work(size_t n, bool sq = false) {
        if (n < (sq ? SQR_KARA : MUL_KARA)) return numeric_limits<u64>::max();
        const u64 w = kara_raw(n, sq);
        if (sq) return w * 8 / 5;
        if (n <= 512) {
            const u64 x = w * (82 * n + 1000) / (100 * n);
            const u64 y = w * 2 / 3 + u64(n) * 17;
            return max(x, y);
        }
        return w;
    }

    static u64 naive_work(size_t n, size_t m, bool sq = false) {
        if (n > MUL_ACC) return numeric_limits<u64>::max();
        if (sq) return (u64(n) * (n + 1) + 3) / 4;
        return (u64(n) * m + 1) / 2;
    }

    static u64 ntt_work(size_t n, size_t m, bool sq = false) {
        if (n > NTT_MAX / 2 || m > (NTT_MAX + 1) / 2 - n) return numeric_limits<u64>::max();
        const size_t len = n * 2 + m * 2 - 1;
        size_t z = 1;
        u64 lg = 0;
        while (z < len) {
            z *= 2;
            ++lg;
        }
        const u64 w = u64(z) * lg * NTT_COST;
        return sq ? w * 2 / 3 : w;
    }

    static u64 mul_work(size_t n, size_t m, bool sq = false) {
        if (n == 0) return 0;
        if (n > m) swap(n, m);
        u64 cost = min({naive_work(n, m, sq), kara_work(m, sq), ntt_work(n, m, sq)});
        if (n <= MUL_NAIVE || m == n) return cost;
        const size_t len = n;
        const size_t cnt = m / len;
        const size_t rem = m % len;
        u64 w = cnt * mul_work(n, len) + m * 2;
        if (rem != 0) w += mul_work(min(n, rem), max(n, rem));
        return min(cost, w);
    }

    static Digits mul_abs(const Digits &a, const Digits &b) {
        if (a.empty() || b.empty()) return {};
        if (a.size() == 1) return mul_small(b, a[0]);
        if (b.size() == 1) return mul_small(a, b[0]);
        const bool sq = &a == &b || a == b;
        const size_t n = min(a.size(), b.size());
        const size_t m = max(a.size(), b.size());
        if (n <= MUL_NAIVE) {
            if (sq && n >= SQR_MIN) return square_naive(a);
            return a.size() < b.size() ? mul_naive(a, b) : mul_naive(b, a);
        }
        int t = 0;
        u64 cost = naive_work(n, m);
        if (sq && n >= SQR_MIN) {
            cost = naive_work(n, n, true);
            t = 1;
        }
        u64 w = kara_work(m, sq);
        if (w <= cost) {
            cost = w;
            t = 2;
        }
        w = ntt_work(n, m, sq);
        if (w < cost) {
            cost = w;
            t = 3;
        }
        if (!sq && m > n) {
            const size_t len = n;
            const size_t cnt = m / len;
            const size_t rem = m % len;
            u64 bw = cnt * mul_work(n, len) + m * 2;
            if (rem != 0) bw += mul_work(min(n, rem), max(n, rem));
            if (bw < cost) {
                cost = bw;
                t = 4;
            }
        }
        size_t k = 1;
        while (k <= m / 2) k *= 2;
        const size_t h = m - k;
        if (h != 0 && h <= k / 4) {
            const size_t al = min(n, k);
            const size_t ah = n > k ? n - k : 0;
            u64 sw;
            if (sq) {
                sw = mul_work(k, k, true) + mul_work(h, k) + mul_work(h, h, true);
            } else {
                sw = mul_work(al, k) + mul_work(al, h) + mul_work(ah, k) + mul_work(ah, h);
            }
            sw += (n + m) * (sq ? 2 : 12);
            if (sw < cost) t = 5;
        }
        if (t == 1) return square_naive(a);
        if (t == 2) return mul_karatsuba(a, b, sq);
        if (t == 3) return mul_ntt(a, b, sq);
        if (t == 4) return a.size() < b.size() ? mul_block(a, b) : mul_block(b, a);
        if (t == 5) return mul_split(a, b, k, sq);
        return a.size() < b.size() ? mul_naive(a, b) : mul_naive(b, a);
    }

    static Digits low(const Digits &a, size_t n) {
        Digits res(a.begin(), a.begin() + min(a.size(), n));
        trim(res);
        return res;
    }

    static Digits high(const Digits &a, size_t n) {
        if (a.size() <= n) return {};
        Digits res(a.begin() + n, a.end());
        trim(res);
        return res;
    }

    static Digits block(const Digits &a, size_t k, size_t n) {
        if (k > numeric_limits<size_t>::max() / n) return {};
        const size_t l = k * n;
        if (l >= a.size()) return {};
        const size_t r = min(a.size(), l + n);
        Digits res(a.begin() + l, a.begin() + r);
        trim(res);
        return res;
    }

    // Returns hi * BASE^n + lo.
    static Digits concat(const Digits &hi, const Digits &lo, size_t n) {
        assert(lo.size() <= n);
        if (hi.empty()) return lo;
        Digits res(max(lo.size(), n + hi.size()));
        copy(lo.begin(), lo.end(), res.begin());
        copy(hi.begin(), hi.end(), res.begin() + n);
        trim(res);
        return res;
    }

    static Digits ones(size_t n) { return Digits(n, BASE - 1); }

    static void dec_abs(Digits &a) {
        assert(!a.empty());
        size_t i = 0;
        while (a[i] == 0) {
            a[i] = BASE - 1;
            ++i;
            assert(i < a.size());
        }
        --a[i];
        trim(a);
    }

    static pair<Digits, Digits> div_knuth(const Digits &a, const Digits &b) {
        assert(!b.empty());
        const int cmp = cmp_abs(a, b);
        if (cmp < 0) return {{}, a};
        if (cmp == 0) return {Digits{1}, {}};
        if (b.size() == 1) {
            auto [q, r] = div_small(a, b[0]);
            return {move(q), r == 0 ? Digits{} : Digits{r}};
        }

        const size_t n = b.size();
        const size_t m = a.size() - n;
        const u32 d = static_cast<u32>(BASE / (u64(b.back()) + 1));
        Digits vn;
        const Digits *vp = &b;
        if (d != 1) {
            vn = mul_small(b, d);
            vp = &vn;
        }
        const Digits &v = *vp;
        Digits u = d == 1 ? a : mul_small(a, d);
        assert(v.size() == n);
        u.resize(a.size() + 1);
        Digits q(m + 1);

        const u64 vt = v[n - 1];
        const u64 inv = numeric_limits<u64>::max() / vt + 1;
        for (size_t jj = m + 1; jj-- > 0;) {
            const size_t j = jj;
            assert(u[j + n] <= vt);
            u64 qhat;
            u64 rhat;
            if (u[j + n] == vt) {
                qhat = BASE - 1;
                rhat = u64(u[j + n - 1]) + vt;
            } else {
                const u64 x = u64(u[j + n]) * BASE + u[j + n - 1];
                qhat = div_inv(x, static_cast<u32>(vt), inv);
                rhat = x - qhat * vt;
            }
            while (rhat < BASE && qhat * v[n - 2] > rhat * BASE + u[j + n - 2]) {
                --qhat;
                rhat += vt;
            }

            u64 carry = 0;
            u64 borrow = 0;
            for (size_t i = 0; i < n; ++i) {
                const u64 p = qhat * v[i] + carry;
                carry = p / BASE;
                const u64 sub = p % BASE + borrow;
                if (u[j + i] < sub) {
                    u[j + i] = static_cast<u32>(u64(u[j + i]) + BASE - sub);
                    borrow = 1;
                } else {
                    u[j + i] = static_cast<u32>(u64(u[j + i]) - sub);
                    borrow = 0;
                }
            }
            const u64 top = carry + borrow;
            const bool neg = u[j + n] < top;
            if (neg) {
                u[j + n] = static_cast<u32>(u64(u[j + n]) + BASE - top);
            } else {
                u[j + n] = static_cast<u32>(u64(u[j + n]) - top);
            }

            if (neg) {
                --qhat;
                u64 c = 0;
                for (size_t i = 0; i < n; ++i) {
                    const u64 sum = u64(u[j + i]) + v[i] + c;
                    if (sum >= BASE) {
                        u[j + i] = static_cast<u32>(sum - BASE);
                        c = 1;
                    } else {
                        u[j + i] = static_cast<u32>(sum);
                        c = 0;
                    }
                }
                u[j + n] = static_cast<u32>((u64(u[j + n]) + c) % BASE);
            }
            q[j] = static_cast<u32>(qhat);
        }

        trim(q);
        u.resize(n);
        trim(u);
        if (d == 1) return {move(q), move(u)};
        auto [r, rem] = div_small(u, d);
        assert(rem == 0);
        return {move(q), move(r)};
    }

    static pair<Digits, Digits> div_digit(const Digits &a, const Digits &b) {
        assert(a.size() == b.size() && b.size() >= 2 && cmp_abs(a, b) >= 0);
        const size_t n = b.size();
        const u64 x = u64(a[n - 1]) * BASE + a[n - 2];
        const u64 y = u64(b[n - 1]) * BASE + b[n - 2];
        u32 q = static_cast<u32>(x / y);
        Digits r = a;
        u64 carry = 0;
        u64 borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            const u64 p = u64(q) * b[i] + carry;
            carry = p / BASE;
            const u64 sub = p % BASE + borrow;
            if (r[i] < sub) {
                r[i] = static_cast<u32>(u64(r[i]) + BASE - sub);
                borrow = 1;
            } else {
                r[i] = static_cast<u32>(u64(r[i]) - sub);
                borrow = 0;
            }
        }
        if (carry + borrow != 0) {
            --q;
            u64 c = 0;
            for (size_t i = 0; i < n; ++i) {
                const u64 sum = u64(r[i]) + b[i] + c;
                r[i] = static_cast<u32>(sum < BASE ? sum : sum - BASE);
                c = sum >= BASE;
            }
            assert(c == carry + borrow);
        } else if (cmp_abs(r, b) >= 0) {
            ++q;
            sub_to(r, b);
        }
        trim(r);
        assert(q < BASE && cmp_abs(r, b) < 0);
        return {Digits{q}, move(r)};
    }

    static pair<Digits, Digits> div_2n_1n(const Digits &a, const Digits &b);
    static pair<Digits, Digits> div_3n_2n(const Digits &a, const Digits &b);
    static pair<Digits, Digits> div_bz(const Digits &a, const Digits &b);
    static pair<Digits, Digits> divmod_abs(const Digits &a, const Digits &b);

    template <integral T> void assign(T x) {
        using U = make_unsigned_t<T>;
        U u;
        if constexpr (signed_integral<T>) {
            if (x < 0) {
                sign_ = -1;
                u = U(0) - static_cast<U>(x);
            } else {
                sign_ = x == 0 ? 0 : 1;
                u = static_cast<U>(x);
            }
        } else {
            sign_ = x == 0 ? 0 : 1;
            u = x;
        }
        if (u != 0) digits_.reserve((numeric_limits<U>::digits + 25) / 26);
        while (u != 0) {
            digits_.push_back(static_cast<u32>(u % BASE));
            u /= BASE;
        }
    }

    BigInt &add(const BigInt &b, int s) {
        const int bs = b.sign_ * s;
        if (bs == 0) return *this;
        if (sign_ == 0) {
            *this = b;
            sign_ = bs;
            return *this;
        }
        if (sign_ == bs) {
            add_to(digits_, b.digits_);
            return *this;
        }
        const int cmp = cmp_abs(digits_, b.digits_);
        if (cmp == 0) {
            digits_.clear();
            sign_ = 0;
        } else if (cmp > 0) {
            sub_to(digits_, b.digits_);
        } else {
            digits_ = sub_abs(b.digits_, digits_);
            sign_ = bs;
        }
        return *this;
    }

public:
    BigInt() noexcept = default;
    BigInt(const BigInt &) = default;
    BigInt &operator=(const BigInt &) = default;

    BigInt(BigInt &&a) noexcept : sign_(a.sign_), digits_(move(a.digits_)) {
        a.sign_ = 0;
        a.digits_.clear();
    }

    BigInt &operator=(BigInt &&a) noexcept {
        if (this == &a) return *this;
        sign_ = a.sign_;
        digits_ = move(a.digits_);
        a.sign_ = 0;
        a.digits_.clear();
        return *this;
    }

    template <integral T>
    requires(!same_as<remove_cv_t<T>, bool>) BigInt(T x) { assign(x); }

    explicit BigInt(string_view s) {
        if (s.empty()) throw invalid_argument("BigInt: empty string");
        size_t l = 0;
        if (s.front() == '+' || s.front() == '-') {
            sign_ = s.front() == '-' ? -1 : 1;
            l = 1;
        } else {
            sign_ = 1;
        }
        if (l == s.size()) throw invalid_argument("BigInt: sign without digits");
        digits_.reserve((s.size() - l + 7) / 8);
        size_t r = s.size();
        while (r > l) {
            const size_t p = r - l > 8 ? r - 8 : l;
            u32 x = 0;
            for (size_t i = p; i < r; ++i) {
                if (s[i] < '0' || s[i] > '9') throw invalid_argument("BigInt: invalid decimal string");
                x = x * 10 + static_cast<u32>(s[i] - '0');
            }
            digits_.push_back(x);
            r = p;
        }
        normalize();
    }

    [[nodiscard]] int sign() const noexcept { return sign_; }
    [[nodiscard]] bool is_zero() const noexcept { return sign_ == 0; }
    [[nodiscard]] bool is_one() const noexcept { return sign_ == 1 && digits_.size() == 1 && digits_[0] == 1; }
    [[nodiscard]] bool is_neg_one() const noexcept { return sign_ == -1 && digits_.size() == 1 && digits_[0] == 1; }
    [[nodiscard]] bool is_odd() const noexcept { return sign_ != 0 && (digits_[0] & 1) != 0; }
    explicit operator bool() const noexcept { return sign_ != 0; }

    [[nodiscard]] BigInt abs() const & {
        BigInt res = *this;
        if (res.sign_ < 0) res.sign_ = 1;
        return res;
    }

    [[nodiscard]] BigInt abs() && {
        if (sign_ < 0) sign_ = 1;
        return move(*this);
    }

    [[nodiscard]] string to_string() const {
        if (sign_ == 0) return "0";
        static constexpr char ds[] = "0001020304050607080910111213141516171819"
                                     "2021222324252627282930313233343536373839"
                                     "4041424344454647484950515253545556575859"
                                     "6061626364656667686970717273747576777879"
                                     "8081828384858687888990919293949596979899";
        string res;
        res.reserve(digits_.size() * 8 + (sign_ < 0));
        if (sign_ < 0) res.push_back('-');
        res += std::to_string(digits_.back());
        for (size_t i = digits_.size() - 1; i-- > 0;) {
            const size_t p = res.size();
            res.append(8, '0');
            u32 x = digits_[i];
            for (int j = 6; j >= 0; j -= 2) {
                const u32 q = x / 100;
                const u32 r = x - q * 100;
                res[p + j] = ds[r * 2];
                res[p + j + 1] = ds[r * 2 + 1];
                x = q;
            }
        }
        return res;
    }

    template <integral T>
    requires(!same_as<remove_cv_t<T>, bool>) [[nodiscard]] remove_cv_t<T> to_integral() const {
        using V = remove_cv_t<T>;
        using U = make_unsigned_t<V>;
        using W = conditional_t<(sizeof(U) < sizeof(u64)), u64, U>;
        if constexpr (unsigned_integral<V>) {
            if (sign_ < 0) throw overflow_error("BigInt: neg value for unsigned conversion");
        }
        W lim;
        if constexpr (signed_integral<V>) {
            lim = sign_ < 0 ? W(numeric_limits<V>::max()) + 1 : W(numeric_limits<V>::max());
        } else {
            lim = numeric_limits<V>::max();
        }
        W x = 0;
        for (size_t i = digits_.size(); i-- > 0;) {
            if (digits_[i] > lim) throw overflow_error("BigInt: integral conversion overflow");
            const W d = digits_[i];
            if (x > (lim - d) / BASE) throw overflow_error("BigInt: integral conversion overflow");
            x = x * BASE + d;
        }
        if constexpr (signed_integral<V>) {
            if (sign_ < 0) {
                const W mn = W(numeric_limits<V>::max()) + 1;
                if (x == mn) return numeric_limits<V>::min();
                return static_cast<V>(-static_cast<V>(x));
            }
        }
        return static_cast<V>(x);
    }

    BigInt operator+() const & { return *this; }

    BigInt operator+() && { return move(*this); }

    BigInt operator-() const & {
        BigInt res = *this;
        res.sign_ = -res.sign_;
        return res;
    }

    BigInt operator-() && {
        sign_ = -sign_;
        return move(*this);
    }

    BigInt &operator+=(const BigInt &b) { return add(b, 1); }

    BigInt &operator-=(const BigInt &b) { return add(b, -1); }

    BigInt &operator*=(const BigInt &b) {
        if (sign_ == 0 || b.sign_ == 0) {
            digits_.clear();
            sign_ = 0;
            return *this;
        }
        Digits res = mul_abs(digits_, b.digits_);
        sign_ *= b.sign_;
        digits_ = move(res);
        return *this;
    }

    BigInt &operator/=(const BigInt &b) {
        *this = divmod(*this, b).first;
        return *this;
    }

    BigInt &operator%=(const BigInt &b) {
        if (b.sign_ == 0) throw domain_error("BigInt: division by zero");
        if (sign_ == 0) return *this;
        const int cmp = cmp_abs(digits_, b.digits_);
        if (cmp < 0) return *this;
        if (cmp == 0) {
            digits_.clear();
            sign_ = 0;
            return *this;
        }
        if (b.digits_.size() == 1) {
            const u32 r = rem_small(digits_, b.digits_[0]);
            if (r == 0) {
                digits_.clear();
                sign_ = 0;
            } else {
                digits_.resize(1);
                digits_[0] = r;
            }
            return *this;
        }
        *this = divmod(*this, b).second;
        return *this;
    }

    BigInt &operator++() {
        if (sign_ < 0) {
            dec_abs(digits_);
            if (digits_.empty()) sign_ = 0;
            return *this;
        }
        if (sign_ == 0) {
            digits_.push_back(1);
            sign_ = 1;
            return *this;
        }
        size_t i = 0;
        while (i < digits_.size() && digits_[i] == BASE - 1) ++i;
        if (i == digits_.size()) digits_.reserve(digits_.size() + 1);
        fill(digits_.begin(), digits_.begin() + i, 0);
        if (i == digits_.size()) {
            digits_.push_back(1);
        } else {
            ++digits_[i];
        }
        return *this;
    }

    BigInt operator++(int) {
        BigInt old = *this;
        ++*this;
        return old;
    }

    BigInt &operator--() {
        if (sign_ > 0) {
            dec_abs(digits_);
            if (digits_.empty()) sign_ = 0;
            return *this;
        }
        if (sign_ == 0) {
            digits_.push_back(1);
            sign_ = -1;
            return *this;
        }
        size_t i = 0;
        while (i < digits_.size() && digits_[i] == BASE - 1) ++i;
        if (i == digits_.size()) digits_.reserve(digits_.size() + 1);
        fill(digits_.begin(), digits_.begin() + i, 0);
        if (i == digits_.size()) {
            digits_.push_back(1);
        } else {
            ++digits_[i];
        }
        return *this;
    }

    BigInt operator--(int) {
        BigInt old = *this;
        --*this;
        return old;
    }

    friend bool operator==(const BigInt &a, const BigInt &b) { return a.sign_ == b.sign_ && a.digits_ == b.digits_; }

    friend strong_ordering operator<=>(const BigInt &a, const BigInt &b) {
        if (a.sign_ != b.sign_) return a.sign_ < b.sign_ ? strong_ordering::less : strong_ordering::greater;
        if (a.sign_ == 0) return strong_ordering::equal;
        int cmp = cmp_abs(a.digits_, b.digits_);
        if (a.sign_ < 0) cmp = -cmp;
        if (cmp < 0) return strong_ordering::less;
        if (cmp > 0) return strong_ordering::greater;
        return strong_ordering::equal;
    }

    friend pair<BigInt, BigInt> divmod(const BigInt &a, const BigInt &b);
    friend BigInt gcd(BigInt a, BigInt b);

    friend BigInt operator+(const BigInt &a, const BigInt &b) {
        if (a.sign_ == 0) return b;
        if (b.sign_ == 0) return a;
        if (a.sign_ == b.sign_) return BigInt(a.sign_, add_abs(a.digits_, b.digits_));
        const int cmp = cmp_abs(a.digits_, b.digits_);
        if (cmp == 0) return {};
        if (cmp > 0) return BigInt(a.sign_, sub_abs(a.digits_, b.digits_));
        return BigInt(b.sign_, sub_abs(b.digits_, a.digits_));
    }

    friend BigInt operator-(const BigInt &a, const BigInt &b) {
        if (b.sign_ == 0) return a;
        if (a.sign_ == 0) return -b;
        if (a.sign_ != b.sign_) return BigInt(a.sign_, add_abs(a.digits_, b.digits_));
        const int cmp = cmp_abs(a.digits_, b.digits_);
        if (cmp == 0) return {};
        if (cmp > 0) return BigInt(a.sign_, sub_abs(a.digits_, b.digits_));
        return BigInt(-a.sign_, sub_abs(b.digits_, a.digits_));
    }

    friend BigInt operator*(const BigInt &a, const BigInt &b) {
        if (a.sign_ == 0 || b.sign_ == 0) return {};
        return BigInt(a.sign_ * b.sign_, mul_abs(a.digits_, b.digits_));
    }

    friend BigInt operator/(const BigInt &a, const BigInt &b) { return divmod(a, b).first; }
    friend BigInt operator%(const BigInt &a, const BigInt &b) {
        if (b.sign_ == 0) throw domain_error("BigInt: division by zero");
        if (a.sign_ == 0) return {};
        const int cmp = cmp_abs(a.digits_, b.digits_);
        if (cmp < 0) return a;
        if (cmp == 0) return {};
        if (b.digits_.size() == 1) {
            const u32 r = rem_small(a.digits_, b.digits_[0]);
            return r == 0 ? BigInt() : BigInt(a.sign_, Digits{r});
        }
        return divmod(a, b).second;
    }

    friend ostream &operator<<(ostream &os, const BigInt &a) { return os << a.to_string(); }

    friend istream &operator>>(istream &is, BigInt &a) {
        string s;
        if (!(is >> s)) return is;
        try {
            BigInt b(s);
            a = move(b);
        } catch (const invalid_argument &) {
            is.setstate(ios::failbit);
        }
        return is;
    }
};

inline pair<BigInt::Digits, BigInt::Digits> BigInt::div_2n_1n(const Digits &a, const Digits &b) {
    const size_t n = b.size();
    assert(n != 0 && a.size() <= n * 2);
    assert(cmp_abs(high(a, n), b) < 0);
    if ((n & 1) != 0 || n < DIV_BZ) {
        return div_knuth(a, b);
    }

    const size_t h = n / 2;
    const Digits a4 = low(a, h);
    const Digits hi = high(a, h);
    auto [q1, r1] = div_3n_2n(hi, b);
    const Digits x = concat(r1, a4, h);
    auto [q0, r] = div_3n_2n(x, b);
    Digits q = concat(q1, q0, h);
    trim(q);
    return {move(q), move(r)};
}

inline pair<BigInt::Digits, BigInt::Digits> BigInt::div_3n_2n(const Digits &a, const Digits &b) {
    assert((b.size() & 1) == 0);
    const size_t h = b.size() / 2;
    assert(a.size() <= h * 3);
    assert(cmp_abs(high(a, h), b) < 0);
    const Digits a3 = low(a, h);
    const Digits a12 = high(a, h);
    const Digits a1 = high(a, h * 2);
    const Digits a2 = low(a12, h);
    const Digits b2 = low(b, h);
    const Digits b1 = high(b, h);
    assert(b1.size() == h);

    Digits q;
    Digits r1;
    const int cmp = cmp_abs(a1, b1);
    if (cmp < 0) {
        auto qr = div_2n_1n(a12, b1);
        q = move(qr.first);
        r1 = move(qr.second);
    } else {
        assert(cmp == 0);
        q = ones(h);
        r1 = add_abs(a2, b1);
    }

    const Digits p = mul_abs(q, b2);
    Digits x = concat(r1, a3, h);
    int cnt = 0;
    while (cmp_abs(x, p) < 0) {
        x = add_abs(x, b);
        dec_abs(q);
        ++cnt;
        assert(cnt <= 2);
    }
    Digits r = sub_abs(x, p);
    assert(cmp_abs(r, b) < 0);
    return {move(q), move(r)};
}

inline pair<BigInt::Digits, BigInt::Digits> BigInt::div_bz(const Digits &a, const Digits &b) {
    const size_t s = b.size();
    size_t m = 1;
    while (m <= s / DIV_BZ) m *= 2;
    const size_t j = (s + m - 1) / m;
    const size_t n = j * m;
    assert((n & 1) == 0);

    const u32 d = static_cast<u32>(BASE / (u64(b.back()) + 1));
    Digits v = d == 1 ? b : mul_small(b, d);
    Digits u = d == 1 ? a : mul_small(a, d);
    assert(v.size() == s);
    const size_t pad = n - s;
    if (pad != 0) {
        v.insert(v.begin(), pad, 0);
        if (!u.empty()) u.insert(u.begin(), pad, 0);
    }
    assert(v.size() == n);

    size_t t = u.size() / n + 1;
    if (t < 2) t = 2;
    Digits z = concat(block(u, t - 1, n), block(u, t - 2, n), n);
    Digits q;
    for (size_t i = t - 2; i > 0; --i) {
        auto [qi, r] = div_2n_1n(z, v);
        add_shifted(q, qi, i * n);
        z = concat(r, block(u, i - 1, n), n);
    }
    auto [ql, nr] = div_2n_1n(z, v);
    add_shifted(q, ql, 0);
    trim(q);

    if (pad != 0 && !nr.empty()) {
        assert(nr.size() >= pad);
        for (size_t i = 0; i < pad; ++i) assert(nr[i] == 0);
        nr.erase(nr.begin(), nr.begin() + pad);
        trim(nr);
    }
    if (d == 1) return {move(q), move(nr)};
    auto [r, rem] = div_small(nr, d);
    assert(rem == 0);
    return {move(q), move(r)};
}

inline pair<BigInt::Digits, BigInt::Digits> BigInt::divmod_abs(const Digits &a, const Digits &b) {
    assert(!b.empty());
    const int cmp = cmp_abs(a, b);
    if (cmp < 0) return {{}, a};
    if (cmp == 0) return {Digits{1}, {}};
    if (b.size() == 1) {
        auto [q, r] = div_small(a, b[0]);
        return {move(q), r == 0 ? Digits{} : Digits{r}};
    }
    if (a.size() == b.size()) return div_digit(a, b);
    const size_t gap = a.size() - b.size();
    const size_t work = b.size() * gap;
    if (b.size() >= DIV_BZ && ((gap >= DIV_BZ_GAP && work >= DIV_BZ_WORK) || (gap >= 6 && work >= 1536))) {
        return div_bz(a, b);
    }
    return div_knuth(a, b);
}

inline pair<BigInt, BigInt> divmod(const BigInt &a, const BigInt &b) {
    if (b.sign_ == 0) throw domain_error("BigInt: division by zero");
    if (a.sign_ == 0) return {BigInt(), BigInt()};
    auto [qd, rd] = BigInt::divmod_abs(a.digits_, b.digits_);
    BigInt q(a.sign_ * b.sign_, move(qd));
    BigInt r(a.sign_, move(rd));
    return {move(q), move(r)};
}

inline BigInt abs(const BigInt &a) { return a.abs(); }

template <unsigned_integral T>
requires(!same_as<remove_cv_t<T>, bool>) inline BigInt pow(BigInt a, T n) {
    using U = make_unsigned_t<remove_cv_t<T>>;
    U u = n;
    BigInt res = 1;
    while (u != 0) {
        if ((u & 1) != 0) res *= a;
        u >>= 1;
        if (u != 0) a *= a;
    }
    return res;
}

template <signed_integral T> inline BigInt pow(BigInt a, T n) {
    if (n < 0) throw domain_error("BigInt: negative exponent");
    return pow(move(a), static_cast<make_unsigned_t<remove_cv_t<T>>>(n));
}

inline BigInt pow(BigInt a, BigInt n) {
    if (n.sign() < 0) throw domain_error("BigInt: negative exponent");
    if (n.is_zero()) return 1;
    if (a.is_zero()) return 0;
    if (a.is_one()) return 1;
    if (a.is_neg_one()) return n.is_odd() ? BigInt(-1) : BigInt(1);
    static const BigInt lim = numeric_limits<uint64_t>::max();
    if (n <= lim) return pow(move(a), n.to_integral<uint64_t>());
    BigInt res = 1;
    const BigInt two = 2;
    while (!n.is_zero()) {
        auto [q, r] = divmod(n, two);
        if (!r.is_zero()) res *= a;
        n = move(q);
        if (!n.is_zero()) a *= a;
    }
    return res;
}

inline BigInt gcd(BigInt a, BigInt b) {
    a = move(a).abs();
    b = move(b).abs();
    while (!b.is_zero()) {
        if (a < b) swap(a, b);
        if (b.is_zero()) break;
        if (b.digits_.size() == 1) {
            const BigInt::u32 r = BigInt::rem_small(a.digits_, b.digits_[0]);
            return BigInt(std::gcd(r, b.digits_[0]));
        }
        if (a.digits_.size() != b.digits_.size()) {
            a %= b;
            swap(a, b);
            continue;
        }

        const size_t n = a.digits_.size();
        BigInt::i128 x = BigInt::i128(a.digits_[n - 1]) * BigInt::BASE + a.digits_[n - 2];
        BigInt::i128 y = BigInt::i128(b.digits_[n - 1]) * BigInt::BASE + b.digits_[n - 2];
        BigInt::i128 aa = 1, ab = 0, ba = 0, bb = 1;
        while (y + ba != 0 && y + bb != 0) {
            const BigInt::i128 q = (x + aa) / (y + ba);
            if (q != (x + ab) / (y + bb)) break;
            const BigInt::i128 na = aa - q * ba;
            const BigInt::i128 nb = ab - q * bb;
            aa = ba;
            ab = bb;
            ba = na;
            bb = nb;
            const BigInt::i128 z = x - q * y;
            x = y;
            y = z;
        }
        if (ab == 0) {
            a %= b;
            swap(a, b);
        } else {
            BigInt c(1, BigInt::lin_abs(a.digits_, b.digits_, aa, ab));
            BigInt d(1, BigInt::lin_abs(a.digits_, b.digits_, ba, bb));
            a = move(c);
            b = move(d);
        }
    }
    return a;
}

template <class T, class U>
requires((same_as<remove_cv_t<T>, BigInt> && integral<U> && !same_as<remove_cv_t<U>, bool>) ||
         (integral<T> && !same_as<remove_cv_t<T>, bool> && same_as<remove_cv_t<U>, BigInt>)) inline BigInt
    gcd(T a, U b) {
    if constexpr (same_as<remove_cv_t<T>, BigInt>) return gcd(move(a), BigInt(b));
    return gcd(BigInt(a), move(b));
}

inline BigInt lcm(const BigInt &a, const BigInt &b) {
    if (a.is_zero() || b.is_zero()) return BigInt();
    return ((a / gcd(a, b)) * b).abs();
}

template <class T, class U>
requires((same_as<remove_cv_t<T>, BigInt> && integral<U> && !same_as<remove_cv_t<U>, bool>) ||
         (integral<T> && !same_as<remove_cv_t<T>, bool> && same_as<remove_cv_t<U>, BigInt>)) inline BigInt
    lcm(T a, U b) {
    if constexpr (same_as<remove_cv_t<T>, BigInt>) return lcm(a, BigInt(b));
    return lcm(BigInt(a), b);
}

} // namespace titan23
