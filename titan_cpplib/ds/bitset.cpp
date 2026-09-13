/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/bitset.cpp
#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#if defined(__AVX2__)
#include <immintrin.h>
#endif
using namespace std;

namespace titan23 {

/// @brief 計算方式 / Scalar でもコンパイラにより自動ベクトル化される場合がある
enum class BitsetBackend { Auto, Scalar, Avx2 };

namespace bitset_detail {
enum class Operation { And, Or, Xor };
struct Access;
}  // namespace bitset_detail

/**
 * @brief 固定長 Bitset / 64bit 単位で管理し、AVX2 に対応
 *
 * 各操作の計算量では W = ceil(N / 64)、K = 入力する Bitset の個数とする。
 * ビット i は words[i / 64] の下位から i % 64 番目に格納する。
 *
 * @tparam N ビット数
 * @tparam Backend 計算方式 / Auto はコンパイル時に有効な命令から選択する
 */
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
    storage_type words_{};

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
    size_t count_scalar(const Others &...others) const {
        size_t result = 0;
        for (size_t i = 0; i < word_count; ++i) {
            result += std::popcount(combined_word<Op>(i, others...));
        }
        return result;
    }

#if defined(__AVX2__)
    template <bitset_detail::Operation Op, class... Others>
    __m256i combined_vector(size_t i, const Others &...others) const {
        __m256i x = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(words_.data() + i));
        if constexpr (Op == bitset_detail::Operation::And) {
            ((x = _mm256_and_si256(x, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(others.words_.data() + i)))), ...);
        } else if constexpr (Op == bitset_detail::Operation::Or) {
            ((x = _mm256_or_si256(x, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(others.words_.data() + i)))), ...);
        } else {
            ((x = _mm256_xor_si256(x, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(others.words_.data() + i)))), ...);
        }
        return x;
    }

    template <bitset_detail::Operation Op, class... Others>
    size_t count_avx2(const Others &...others) const {
        const __m256i lookup = _mm256_setr_epi8(
            0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
            0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
        const __m256i mask = _mm256_set1_epi8(15);
        const __m256i zero = _mm256_setzero_si256();
        auto byte_counts = [&](size_t i) {
            const __m256i x = combined_vector<Op>(i, others...);
            const __m256i lo = _mm256_and_si256(x, mask);
            const __m256i hi = _mm256_and_si256(_mm256_srli_epi16(x, 4), mask);
            return _mm256_add_epi8(_mm256_shuffle_epi8(lookup, lo),
                                   _mm256_shuffle_epi8(lookup, hi));
        };

        __m256i total = zero;
        size_t i = 0;
        // 8 ベクトルごとに集計し、各バイトの累積値を 64 以下に抑える。
        for (; i + 32 <= word_count; i += 32) {
            __m256i bytes = zero;
            for (size_t j = 0; j < 32; j += 4) {
                bytes = _mm256_add_epi8(bytes, byte_counts(i + j));
            }
            total = _mm256_add_epi64(total, _mm256_sad_epu8(bytes, zero));
        }
        __m256i bytes = zero;
        for (; i + 4 <= word_count; i += 4) {
            bytes = _mm256_add_epi8(bytes, byte_counts(i));
        }
        total = _mm256_add_epi64(total, _mm256_sad_epu8(bytes, zero));
        array<word_type, 4> sums;
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(sums.data()), total);
        size_t result = sums[0] + sums[1] + sums[2] + sums[3];
        for (; i < word_count; ++i) {
            result += std::popcount(combined_word<Op>(i, others...));
        }
        return result;
    }

    static bool use_avx2_shift(size_t active_words) {
        if constexpr (Backend == BitsetBackend::Scalar) return false;
        if constexpr (Backend == BitsetBackend::Avx2) return true;
        return active_words >= 16;
    }
#endif

    template <bitset_detail::Operation Op, class... Others>
    size_t count_impl(const Others &...others) const {
#if defined(__AVX2__)
        if constexpr (Backend == BitsetBackend::Avx2 && word_count >= 4) {
            return count_avx2<Op>(others...);
        }
        // ベクトル popcount 命令が使える場合はコンパイラの自動ベクトル化に任せる。
#if !defined(__AVX512VPOPCNTDQ__)
        if constexpr (Backend == BitsetBackend::Auto && word_count >= 16) {
            return count_avx2<Op>(others...);
        }
#endif
#endif
        return count_scalar<Op>(others...);
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
#if defined(__AVX2__)
            if constexpr (word_count >= 4) {
                if (use_avx2_shift(word_count - q)) {
                    for (; i >= q + 4; i -= 4) {
                        const size_t dst = i - 4;
                        __m256i x = _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + dst - q));
                        if constexpr (Or) x = _mm256_or_si256(x, _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + dst)));
                        _mm256_storeu_si256(reinterpret_cast<__m256i *>(words_.data() + dst), x);
                    }
                }
            }
#endif
            for (; i > q; --i) {
                const word_type x = words_[i - 1 - q];
                if constexpr (Or) words_[i - 1] |= x;
                else words_[i - 1] = x;
            }
        } else {
#if defined(__AVX2__)
            if constexpr (word_count >= 5) {
                if (use_avx2_shift(word_count - q)) {
                    const __m128i left = _mm_cvtsi64_si128(r);
                    const __m128i right = _mm_cvtsi64_si128(64 - r);
                    // 読み書きが重なるため、両方の入力を読み込んでから書き込む。
                    for (; i >= q + 5; i -= 4) {
                        const size_t dst = i - 4;
                        const __m256i a = _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + dst - q));
                        const __m256i b = _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + dst - q - 1));
                        __m256i x = _mm256_or_si256(_mm256_sll_epi64(a, left),
                                                   _mm256_srl_epi64(b, right));
                        if constexpr (Or) x = _mm256_or_si256(x, _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + dst)));
                        _mm256_storeu_si256(reinterpret_cast<__m256i *>(words_.data() + dst), x);
                    }
                }
            }
#endif
            for (; i > q + 1; --i) {
                const word_type x = (words_[i - 1 - q] << r) | (words_[i - 2 - q] >> (64 - r));
                if constexpr (Or) words_[i - 1] |= x;
                else words_[i - 1] = x;
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
#if defined(__AVX2__)
            if constexpr (word_count >= 4) {
                if (use_avx2_shift(limit)) {
                    for (; i + 4 <= limit; i += 4) {
                        __m256i x = _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + i + q));
                        if constexpr (Or) x = _mm256_or_si256(x, _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + i)));
                        _mm256_storeu_si256(reinterpret_cast<__m256i *>(words_.data() + i), x);
                    }
                }
            }
#endif
            for (; i < limit; ++i) {
                const word_type x = words_[i + q];
                if constexpr (Or) words_[i] |= x;
                else words_[i] = x;
            }
        } else {
#if defined(__AVX2__)
            if constexpr (word_count >= 5) {
                if (use_avx2_shift(limit)) {
                    const __m128i right = _mm_cvtsi64_si128(r);
                    const __m128i left = _mm_cvtsi64_si128(64 - r);
                    for (; i + 4 < limit; i += 4) {
                        const __m256i a = _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + i + q));
                        const __m256i b = _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + i + q + 1));
                        __m256i x = _mm256_or_si256(_mm256_srl_epi64(a, right),
                                                   _mm256_sll_epi64(b, left));
                        if constexpr (Or) x = _mm256_or_si256(x, _mm256_loadu_si256(
                            reinterpret_cast<const __m256i *>(words_.data() + i)));
                        _mm256_storeu_si256(reinterpret_cast<__m256i *>(words_.data() + i), x);
                    }
                }
            }
#endif
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
    Bitset() = default;

    /// @brief 下位 min(N, 64) ビットを value で初期化する / O(W)
    explicit Bitset(const word_type value) {
        if constexpr (word_count != 0) words_[0] = value;
        trim();
    }

    /// @brief 64bit 単位の配列から構築する / N 以降のビットは除く / O(W)
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
        for (word_type &x : words_) x = ~x;
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
        for (word_type x : words_) if (x) return true;
        return false;
    }

    /// @brief 全ビットが 0 か返す / O(W)
    bool none() const {
        return !any();
    }

    /// @brief 全ビットが 1 か返す / 空なら true / O(W)
    bool all() const {
        for (size_t i = 0; i < N / 64; ++i) {
            if (words_[i] != ~word_type{0}) return false;
        }
        if constexpr (N % 64 != 0) return words_.back() == (word_type{1} << (N % 64)) - 1;
        return true;
    }

    /// @brief 自身と other に共通する 1 のビットが存在するか返す / O(W)
    bool intersects(const Bitset &other) const {
        for (size_t i = 0; i < word_count; ++i) {
            if (words_[i] & other.words_[i]) return true;
        }
        return false;
    }

    /// @brief 自身を other との AND に更新する / O(W)
    Bitset &operator&=(const Bitset &other) {
        for (size_t i = 0; i < word_count; ++i) words_[i] &= other.words_[i];
        return *this;
    }

    /// @brief 自身を other との OR に更新する / O(W)
    Bitset &operator|=(const Bitset &other) {
        for (size_t i = 0; i < word_count; ++i) words_[i] |= other.words_[i];
        return *this;
    }

    /// @brief 自身を other との XOR に更新する / O(W)
    Bitset &operator^=(const Bitset &other) {
        for (size_t i = 0; i < word_count; ++i) words_[i] ^= other.words_[i];
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
    friend Bitset operator&(Bitset a, const Bitset &b) {
        return a &= b;
    }

    /// @brief a と b の OR を返す / O(W)
    friend Bitset operator|(Bitset a, const Bitset &b) {
        return a |= b;
    }

    /// @brief a と b の XOR を返す / O(W)
    friend Bitset operator^(Bitset a, const Bitset &b) {
        return a ^= b;
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

// 自由関数で使う共通処理。count / any / all は中間 Bitset を作らずに計算する。
// 演算の種類と入力数はコンパイル時に決まる。
struct Access {
    template <Operation Op, class Bits, class... Others>
    static Bits combine(const Bits &first, const Others &...others) {
        Bits result;
        for (size_t i = 0; i < Bits::word_count; ++i) {
            result.words_[i] = first.template combined_word<Op>(i, others...);
        }
        return result;
    }

    template <Operation Op, class Bits, class... Others>
    static size_t count(const Bits &first, const Others &...others) {
        return first.template count_impl<Op>(others...);
    }

    template <Operation Op, bool All, class Bits, class... Others>
    static bool test(const Bits &first, const Others &...others) {
        using Word = typename Bits::word_type;
        for (size_t i = 0; i < Bits::word_count; ++i) {
            const Word x = first.template combined_word<Op>(i, others...);
            if constexpr (All) {
                Word mask = ~Word{0};
                if constexpr (Bits::size() % 64 != 0) {
                    if (i + 1 == Bits::word_count) mask = (Word{1} << (Bits::size() % 64)) - 1;
                }
                if (x != mask) return false;
            } else {
                if (x != 0) return true;
            }
        }
        return All; // 空の Bitset では any = false、all = true。
    }
};

}  // namespace bitset_detail

// 複数入力の演算は、同じ型の Bitset を 2 個以上受け取る。
// 以下では W = ceil(N / 64)、K = 入力する Bitset の個数とする。

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
