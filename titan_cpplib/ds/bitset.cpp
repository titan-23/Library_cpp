/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/bitset.cpp
#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include "titan_cpplib/others/simd_int.cpp"
using namespace std;

namespace titan23 {

/// @brief Bitset の計算方式 / Auto は SimdBits の SIMD 実装を使い、Scalar は使わず、Avx2 は常に AVX2 を使う
enum class BitsetBackend { Auto, Scalar, Avx2 };

namespace bitset_detail {
enum class Operation { And, Or, Xor };
struct Access;
}  // namespace bitset_detail

/// @brief N ビットの固定長ビット列 / 64bit 単位で持ち、SimdBits で処理する / 計算量の W は ceil(N / 64)、K は入力する Bitset の個数
template <size_t N, BitsetBackend Backend = BitsetBackend::Auto>
class Bitset {
public:
    using word_type = uint64_t;
    static constexpr size_t word_count = N / 64 + (N % 64 != 0);
    using storage_type = array<word_type, word_count>;

private:
    friend struct bitset_detail::Access;
#if !defined(__AVX2__)
    static_assert(Backend != BitsetBackend::Avx2,
                  "BitsetBackend::Avx2 requires an AVX2 compiler target");
#endif
    storage_type words_;  // 初期値は各構築子で与える

    // 全語を書き込む前提で、内部配列を初期化せずに構築するための目印
    struct uninitialized_tag {};
    explicit Bitset(uninitialized_tag) {}

    using Simd = SimdBits;
    using vector_type = typename Simd::Vec;
    static constexpr size_t lanes = Simd::LANES;
    // SimdBits で処理するか。Scalar は使わず、Avx2 は常に使う
    static constexpr bool simd_enabled =
        Backend == BitsetBackend::Avx2 || (Backend == BitsetBackend::Auto && Simd::VECTORIZED);
    // Vec で処理できる語数。残りの語は 1 語ずつ処理する
    static constexpr size_t vector_words = simd_enabled ? word_count / lanes * lanes : 0;
    // Auto で count と count_* を SimdBits で処理する最小の語数
#if defined(__AVX512VPOPCNTDQ__)
    // 通常のループがベクトルの popcount になるため、コンパイラに任せる
    static constexpr size_t simd_count_min = ~size_t{0};
#elif defined(__AVX2__)
    // 16 語未満では AVX2 を使わない
    static constexpr size_t simd_count_min = 16;
#elif defined(__POPCNT__)
    // SSE2 のビット演算による数え上げは POPCNT 命令より遅い
    static constexpr size_t simd_count_min = ~size_t{0};
#else
    // POPCNT 命令がないと std::popcount は関数呼び出しになるため、Vec 1 本分から SIMD を使う
    static constexpr size_t simd_count_min = lanes;
#endif
    static constexpr bool simd_count =
        simd_enabled && word_count >= lanes && (Backend == BitsetBackend::Avx2 || word_count >= simd_count_min);

    void trim() {
        if constexpr (N % 64 != 0) words_.back() &= (word_type{1} << (N % 64)) - 1;
    }

    template <bitset_detail::Operation Op, class... Others>
    word_type combined_word(size_t i, const Others &...others) const {
        word_type x = words_[i];
        if constexpr (Op == bitset_detail::Operation::And) ((x &= others.words_[i]), ...);
        else if constexpr (Op == bitset_detail::Operation::Or) ((x |= others.words_[i]), ...);
        else ((x ^= others.words_[i]), ...);
        return x;
    }

    template <bitset_detail::Operation Op, class... Others>
    vector_type combined_vector(size_t i, const Others &...others) const {
        vector_type x = Simd::loadu(words_.data() + i);
        if constexpr (Op == bitset_detail::Operation::And) ((x = Simd::bit_and(x, Simd::loadu(others.words_.data() + i))), ...);
        else if constexpr (Op == bitset_detail::Operation::Or) ((x = Simd::bit_or(x, Simd::loadu(others.words_.data() + i))), ...);
        else ((x = Simd::bit_xor(x, Simd::loadu(others.words_.data() + i))), ...);
        return x;
    }

    // 自身と others を Op で合成した結果を out に書く / out は自身でもよい
    template <bitset_detail::Operation Op, class... Others>
    void combine_into(Bitset &out, const Others &...others) const {
        size_t i = 0;
        if constexpr (vector_words > 0) {
            for (; i < vector_words; i += lanes) Simd::storeu(out.words_.data() + i, combined_vector<Op>(i, others...));
        }
        for (; i < word_count; ++i) out.words_[i] = combined_word<Op>(i, others...);
    }

    template <bitset_detail::Operation Op, class... Others>
    size_t count_impl(const Others &...others) const {
        size_t result = 0, i = 0;
        if constexpr (simd_count) {
            result = simd_bits_popcount(word_count / lanes, [&](size_t j) { return combined_vector<Op>(j * lanes, others...); });
            i = word_count / lanes * lanes;
        }
        for (; i < word_count; ++i) result += std::popcount(combined_word<Op>(i, others...));
        return result;
    }

    // 自身と others を Op で合成し、All なら全ビットが 1 か、そうでなければ 1 があるかを返す
    template <bitset_detail::Operation Op, bool All, class... Others>
    bool test_impl(const Others &...others) const {
        // All では N 以降のビットを含む末尾の語を除き、全ビットが有効な語だけを Vec で調べる
        constexpr size_t simd_words = simd_enabled ? (All ? N / 64 : word_count) / lanes * lanes : 0;
        size_t i = 0;
        if constexpr (simd_words > 0) {
            for (; i < simd_words; i += lanes) {
                const vector_type x = combined_vector<Op>(i, others...);
                if constexpr (All) {
                    if (Simd::nonzero(Simd::bit_xor(x, Simd::ones()))) return false;
                } else {
                    if (Simd::nonzero(x)) return true;
                }
            }
        }
        for (; i < word_count; ++i) {
            const word_type x = combined_word<Op>(i, others...);
            if constexpr (All) {
                word_type mask = ~word_type{0};
                if constexpr (N % 64 != 0) {
                    if (i + 1 == word_count) mask = (word_type{1} << (N % 64)) - 1;
                }
                if (x != mask) return false;
            } else {
                if (x != 0) return true;
            }
        }
        return All;  // 空の Bitset では any = false、all = true
    }

    // シフトを SimdBits で処理するか / active_words はシフト後に値が入る語数
    static bool use_simd_shift(size_t active_words) {
        if constexpr (Backend == BitsetBackend::Avx2) return true;
        else if constexpr (simd_enabled) return active_words >= 16;
        else return false;
    }

    template <bool Or>
    Bitset &shift_left(size_t k) {
        if (k == 0) return *this;
        if (k >= N) {
            if constexpr (!Or) reset();
            return *this;
        }
        const size_t q = k / 64;
        const unsigned r = k % 64;
        size_t i = word_count;
        if (r == 0) {
            if constexpr (simd_enabled) {
                if (use_simd_shift(word_count - q)) {
                    for (; i >= q + lanes; i -= lanes) {
                        const size_t dst = i - lanes;
                        vector_type x = Simd::loadu(words_.data() + dst - q);
                        if constexpr (Or) x = Simd::bit_or(x, Simd::loadu(words_.data() + dst));
                        Simd::storeu(words_.data() + dst, x);
                    }
                }
            }
            for (; i > q; --i) {
                const word_type x = words_[i - 1 - q];
                if constexpr (Or) words_[i - 1] |= x;
                else words_[i - 1] = x;
            }
        } else {
            if constexpr (simd_enabled) {
                if (use_simd_shift(word_count - q)) {
                    // 読み書きが重なるため、両方の入力を読み込んでから書き込む。上位側から処理する
                    for (; i >= q + lanes + 1; i -= lanes) {
                        const size_t dst = i - lanes;
                        const vector_type a = Simd::loadu(words_.data() + dst - q);
                        const vector_type b = Simd::loadu(words_.data() + dst - q - 1);
                        vector_type x = Simd::bit_or(Simd::shl(a, r), Simd::shr(b, 64 - r));
                        if constexpr (Or) x = Simd::bit_or(x, Simd::loadu(words_.data() + dst));
                        Simd::storeu(words_.data() + dst, x);
                    }
                }
            }
            // 書き込む語を dst とする。i から q + 1 まで下る
            for (size_t dst = i; dst-- > q + 1;) {
                const word_type x = (words_[dst - q] << r) | (words_[dst - q - 1] >> (64 - r));
                if constexpr (Or) words_[dst] |= x;
                else words_[dst] = x;
            }
            const word_type x = words_[0] << r;
            if constexpr (Or) words_[q] |= x;
            else words_[q] = x;
        }
        if constexpr (!Or) {
            for (size_t j = 0; j < q; ++j) words_[j] = 0;
        }
        trim();
        return *this;
    }

    template <bool Or>
    Bitset &shift_right(size_t k) {
        if (k == 0) return *this;
        if (k >= N) {
            if constexpr (!Or) reset();
            return *this;
        }
        const size_t q = k / 64;
        const unsigned r = k % 64;
        const size_t limit = word_count - q;
        size_t i = 0;
        if (r == 0) {
            if constexpr (simd_enabled) {
                if (use_simd_shift(limit)) {
                    for (; i + lanes <= limit; i += lanes) {
                        vector_type x = Simd::loadu(words_.data() + i + q);
                        if constexpr (Or) x = Simd::bit_or(x, Simd::loadu(words_.data() + i));
                        Simd::storeu(words_.data() + i, x);
                    }
                }
            }
            for (; i < limit; ++i) {
                const word_type x = words_[i + q];
                if constexpr (Or) words_[i] |= x;
                else words_[i] = x;
            }
        } else {
            if constexpr (simd_enabled) {
                if (use_simd_shift(limit)) {
                    // 読み書きが重なるため、両方の入力を読み込んでから書き込む。下位側から処理する
                    for (; i + lanes < limit; i += lanes) {
                        const vector_type a = Simd::loadu(words_.data() + i + q);
                        const vector_type b = Simd::loadu(words_.data() + i + q + 1);
                        vector_type x = Simd::bit_or(Simd::shr(a, r), Simd::shl(b, 64 - r));
                        if constexpr (Or) x = Simd::bit_or(x, Simd::loadu(words_.data() + i));
                        Simd::storeu(words_.data() + i, x);
                    }
                }
            }
            for (; i + 1 < limit; ++i) {
                const word_type x = (words_[i + q] >> r) | (words_[i + q + 1] << (64 - r));
                if constexpr (Or) words_[i] |= x;
                else words_[i] = x;
            }
            const word_type x = words_[word_count - 1] >> r;
            if constexpr (Or) words_[i] |= x;
            else words_[i] = x;
        }
        if constexpr (!Or) {
            for (size_t j = limit; j < word_count; ++j) words_[j] = 0;
        }
        return *this;
    }

public:
    /// @brief 全ビットを 0 で初期化する / O(W)
    Bitset() : words_{} {}

    /// @brief 下位 min(N, 64) ビットを value で初期化する / O(W)
    explicit Bitset(const word_type value) : words_{} {
        if constexpr (word_count != 0) words_[0] = value;
        trim();
    }

    /// @brief 64bit 単位の配列から構築する / ビット i は words[i / 64] の下位から i % 64 番目 / N 以降のビットは除く / O(W)
    static Bitset from_words(const storage_type &words) {
        Bitset result;
        result.words_ = words;
        result.trim();
        return result;
    }

    /// @brief 内部配列への読み取り専用参照を返す / O(1)
    const storage_type &words() const {
        return words_;
    }

    /// @brief ビット数を返す / O(1)
    static constexpr size_t size() {
        return N;
    }

    /// @brief ビット数を返す / O(1)
    static constexpr size_t len() {
        return N;
    }

    /// @brief ビット i の値を返す / O(1)
    bool test(const size_t i) const {
        assert(i < N);
        return (words_[i / 64] >> (i % 64)) & 1;
    }

    /// @brief ビット i の値を返す / O(1)
    bool access(const size_t i) const {
        return test(i);
    }

    /// @brief ビット i を value に更新する / O(1)
    Bitset &set(const size_t i, const bool value = true) {
        assert(i < N);
        const word_type mask = word_type{1} << (i % 64);
        if (value) words_[i / 64] |= mask;
        else words_[i / 64] &= ~mask;
        return *this;
    }

    /// @brief 全ビットを 1 にする / O(W)
    Bitset &set() {
        words_.fill(~word_type{0});
        trim();
        return *this;
    }

    /// @brief ビット i を 0 にする / O(1)
    Bitset &reset(const size_t i) {
        return set(i, false);
    }

    /// @brief 全ビットを 0 にする / O(W)
    Bitset &reset() {
        words_.fill(0);
        return *this;
    }

    /// @brief ビット i を反転する / O(1)
    Bitset &flip(const size_t i) {
        assert(i < N);
        words_[i / 64] ^= word_type{1} << (i % 64);
        return *this;
    }

    /// @brief 全ビットを反転する / O(W)
    Bitset &flip() {
        size_t i = 0;
        if constexpr (vector_words > 0) {
            for (; i < vector_words; i += lanes) {
                Simd::storeu(words_.data() + i, Simd::bit_xor(Simd::loadu(words_.data() + i), Simd::ones()));
            }
        }
        for (; i < word_count; ++i) words_[i] = ~words_[i];
        trim();
        return *this;
    }

    /// @brief operator[] でビットを読み書きするための参照
    class reference {
    private:
        Bitset &owner_;
        size_t index_;

    public:
        reference(Bitset &owner, const size_t index) : owner_(owner), index_(index) {}

        reference &operator=(const bool value) {
            owner_.set(index_, value);
            return *this;
        }

        reference &operator=(const reference &other) {
            return *this = static_cast<bool>(other);
        }

        operator bool() const {
            return owner_.test(index_);
        }

        bool operator~() const {
            return !static_cast<bool>(*this);
        }

        reference &flip() {
            owner_.flip(index_);
            return *this;
        }
    };

    /// @brief ビット i の値を返す / O(1)
    bool operator[](const size_t i) const {
        return test(i);
    }

    /// @brief ビット i を読み書きする参照を返す / O(1)
    reference operator[](const size_t i) {
        assert(i < N);
        return reference(*this, i);
    }

    /// @brief 1 のビット数を返す / O(W)
    size_t count() const {
        return count_impl<bitset_detail::Operation::And>();
    }

    /// @brief 自身と引数の AND に含まれる 1 のビット数を返す / 自身を含めて K 個 / O(KW)
    template <class... Others>
        requires (same_as<Bitset, Others> && ...)
    size_t count_and(const Bitset &other, const Others &...others) const {
        return count_impl<bitset_detail::Operation::And>(other, others...);
    }

    /// @brief 1 のビットが存在するか返す / O(W)
    bool any() const {
        return test_impl<bitset_detail::Operation::Or, false>();
    }

    /// @brief 全ビットが 0 か返す / O(W)
    bool none() const {
        return !any();
    }

    /// @brief 全ビットが 1 か返す / 空なら true / O(W)
    bool all() const {
        return test_impl<bitset_detail::Operation::Or, true>();
    }

    /// @brief 自身と other に共通する 1 のビットが存在するか返す / O(W)
    bool intersects(const Bitset &other) const {
        return test_impl<bitset_detail::Operation::And, false>(other);
    }

    /// @brief 自身を other との AND に更新する / O(W)
    Bitset &operator&=(const Bitset &other) {
        combine_into<bitset_detail::Operation::And>(*this, other);
        return *this;
    }

    /// @brief 自身を other との OR に更新する / O(W)
    Bitset &operator|=(const Bitset &other) {
        combine_into<bitset_detail::Operation::Or>(*this, other);
        return *this;
    }

    /// @brief 自身を other との XOR に更新する / O(W)
    Bitset &operator^=(const Bitset &other) {
        combine_into<bitset_detail::Operation::Xor>(*this, other);
        return *this;
    }

    /// @brief 自身を k ビット左シフトする / k >= N なら全ビットを 0 にする / O(W)
    Bitset &operator<<=(const size_t k) {
        return shift_left<false>(k);
    }

    /// @brief 自身を k ビット右シフトする / k >= N なら全ビットを 0 にする / O(W)
    Bitset &operator>>=(const size_t k) {
        return shift_right<false>(k);
    }

    /// @brief a |= a << k を中間 Bitset を作らずに行う / 更新前のビットで計算する / O(W)
    Bitset &or_shift_left(const size_t k) {
        return shift_left<true>(k);
    }

    /// @brief a |= a >> k を中間 Bitset を作らずに行う / 更新前のビットで計算する / O(W)
    Bitset &or_shift_right(const size_t k) {
        return shift_right<true>(k);
    }

    /// @brief a と b の AND を返す / O(W)
    friend Bitset operator&(const Bitset &a, const Bitset &b) {
        Bitset result{uninitialized_tag{}};
        a.combine_into<bitset_detail::Operation::And>(result, b);
        return result;
    }

    /// @brief a と b の OR を返す / O(W)
    friend Bitset operator|(const Bitset &a, const Bitset &b) {
        Bitset result{uninitialized_tag{}};
        a.combine_into<bitset_detail::Operation::Or>(result, b);
        return result;
    }

    /// @brief a と b の XOR を返す / O(W)
    friend Bitset operator^(const Bitset &a, const Bitset &b) {
        Bitset result{uninitialized_tag{}};
        a.combine_into<bitset_detail::Operation::Xor>(result, b);
        return result;
    }

    /// @brief 全ビットを反転した結果を返す / O(W)
    friend Bitset operator~(Bitset a) {
        return a.flip();
    }

    /// @brief k ビット左シフトした結果を返す / O(W)
    friend Bitset operator<<(Bitset a, const size_t k) {
        return a <<= k;
    }

    /// @brief k ビット右シフトした結果を返す / O(W)
    friend Bitset operator>>(Bitset a, const size_t k) {
        return a >>= k;
    }

    /// @brief 全ビットが一致するか返す / O(W)
    bool operator==(const Bitset &other) const {
        return words_ == other.words_;
    }

    /// @brief 異なるビットが存在するか返す / O(W)
    bool operator!=(const Bitset &other) const {
        return !(*this == other);
    }
};

namespace bitset_detail {

// 自由関数で使う共通処理 / count、any、all は中間 Bitset を作らない
struct Access {
    template <Operation Op, class Bits, class... Others>
    static Bits combine(const Bits &first, const Others &...others) {
        Bits result{typename Bits::uninitialized_tag{}};
        first.template combine_into<Op>(result, others...);
        return result;
    }

    template <Operation Op, class Bits, class... Others>
    static size_t count(const Bits &first, const Others &...others) {
        return first.template count_impl<Op>(others...);
    }

    template <Operation Op, bool All, class Bits, class... Others>
    static bool test(const Bits &first, const Others &...others) {
        return first.template test_impl<Op, All>(others...);
    }
};

}  // namespace bitset_detail

// 以下の複数入力の演算は同じ型の Bitset を 2 個以上受け取る / W と K の意味は Bitset の説明と同じ

/// @brief 全入力の AND を返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
Bitset<N, B> bit_and(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::combine<bitset_detail::Operation::And>(first, second, others...);
}

/// @brief 全入力の OR を返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
Bitset<N, B> bit_or(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::combine<bitset_detail::Operation::Or>(first, second, others...);
}

/// @brief 全入力の XOR を返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
Bitset<N, B> bit_xor(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::combine<bitset_detail::Operation::Xor>(first, second, others...);
}

/// @brief 全入力の AND に含まれる 1 のビット数を返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
size_t count_and(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::count<bitset_detail::Operation::And>(first, second, others...);
}

/// @brief 全入力の OR に含まれる 1 のビット数を返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
size_t count_or(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::count<bitset_detail::Operation::Or>(first, second, others...);
}

/// @brief 全入力の XOR に含まれる 1 のビット数を返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
size_t count_xor(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::count<bitset_detail::Operation::Xor>(first, second, others...);
}

/// @brief 全入力の AND に 1 のビットが存在するか返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
bool any_and(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::test<bitset_detail::Operation::And, false>(first, second, others...);
}

/// @brief 全入力の OR に 1 のビットが存在するか返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
bool any_or(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::test<bitset_detail::Operation::Or, false>(first, second, others...);
}

/// @brief 全入力の XOR に 1 のビットが存在するか返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
bool any_xor(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::test<bitset_detail::Operation::Xor, false>(first, second, others...);
}

/// @brief 全入力の AND の全ビットが 1 か返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
bool all_and(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::test<bitset_detail::Operation::And, true>(first, second, others...);
}

/// @brief 全入力の OR の全ビットが 1 か返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
bool all_or(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::test<bitset_detail::Operation::Or, true>(first, second, others...);
}

/// @brief 全入力の XOR の全ビットが 1 か返す / O(KW)
template <size_t N, BitsetBackend B, class... Others>
    requires (same_as<Bitset<N, B>, Others> && ...)
bool all_xor(const Bitset<N, B> &first, const Bitset<N, B> &second, const Others &...others) {
    return bitset_detail::Access::test<bitset_detail::Operation::Xor, true>(first, second, others...);
}

/// @brief 1 のビット数を返す / O(W)
template <size_t N, BitsetBackend B>
size_t count(const Bitset<N, B> &bits) {
    return bits.count();
}

/// @brief 1 のビットが存在するか返す / O(W)
template <size_t N, BitsetBackend B>
bool any(const Bitset<N, B> &bits) {
    return bits.any();
}

/// @brief 全ビットが 1 か返す / 空なら true / O(W)
template <size_t N, BitsetBackend B>
bool all(const Bitset<N, B> &bits) {
    return bits.all();
}

/// @brief 全ビットが 0 か返す / O(W)
template <size_t N, BitsetBackend B>
bool none(const Bitset<N, B> &bits) {
    return bits.none();
}

}  // namespace titan23
