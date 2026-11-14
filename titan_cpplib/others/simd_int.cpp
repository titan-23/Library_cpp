/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/simd_int.cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#if (defined(__AVX512F__) && defined(__AVX512VL__)) || defined(__AVX2__) || defined(__SSE2__)
#include <immintrin.h>
#endif

namespace titan23 {

/// @brief 32 bit と 64 bit の符号付き整数の SIMD 演算を、命令セットによらない名前でまとめる / 実装は AVX-512, AVX2, SSE2, 逐次の順に選ぶ
/// Mask の bit i が要素 i を表す / load と store は Mask の要素だけを読み書きし、load は他の要素を 0 にする / 加減算は 2 の補数で巡回する
template<class T, size_t Size = sizeof(T)>
struct SimdInt;

#if defined(__AVX512F__) && defined(__AVX512VL__)

template<class T>
struct SimdInt<T, 8> {
    static constexpr const char *NAME = "avx512";
    static constexpr bool VECTORIZED = true;
    using Vec = __m512i;
    using Mask = __mmask8;
    using IntVec = __m256i;
    static constexpr int LANES = 8;
    static constexpr Mask ALL = 0xFF;
    static Mask lane_mask(int rest) { return rest >= LANES ? ALL : (Mask)((1u << rest) - 1); }
    static Vec zero() { return _mm512_setzero_si512(); }
    static Vec set1(T x) { return _mm512_set1_epi64(x); }
    static Vec loadu(const T *p) { return _mm512_loadu_si512(p); }
    static void storeu(T *p, Vec v) { _mm512_storeu_si512(p, v); }
    static Vec load(Mask k, const T *p) { return _mm512_maskz_loadu_epi64(k, p); }
    static void store(T *p, Mask k, Vec v) { _mm512_mask_storeu_epi64(p, k, v); }
    static Vec add(Vec a, Vec b) { return _mm512_add_epi64(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm512_sub_epi64(a, b); }
    static Vec min(Vec a, Vec b) { return _mm512_min_epi64(a, b); }
    static Vec max(Vec a, Vec b) { return _mm512_max_epi64(a, b); }
    static T reduce_add(Vec v) { return (T)_mm512_reduce_add_epi64(v); }
    static Mask cmpeq(Vec a, Vec b) { return _mm512_cmpeq_epi64_mask(a, b); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return _mm512_mask_cmpneq_epi64_mask(k, a, b); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return _mm512_mask_cmpgt_epi64_mask(k, a, b); }
    static unsigned cmpgt_bits(Vec a, Vec b) { return _mm512_cmpgt_epi64_mask(a, b); }
    static Vec blend(Vec src, Mask k, Vec a) { return _mm512_mask_mov_epi64(src, k, a); }
    static IntVec int_set1(int x) { return _mm256_set1_epi32(x); }
    static void int_store(int *p, Mask k, IntVec v) { _mm256_mask_storeu_epi32(p, k, v); }
    static Mask int_nonzero(Mask k, const int *p) {
        return _mm256_mask_cmpneq_epi32_mask(k, _mm256_maskz_loadu_epi32(k, p), _mm256_setzero_si256());
    }
};

template<class T>
struct SimdInt<T, 4> {
    static constexpr const char *NAME = "avx512";
    static constexpr bool VECTORIZED = true;
    using Vec = __m512i;
    using Mask = __mmask16;
    using IntVec = __m512i;
    static constexpr int LANES = 16;
    static constexpr Mask ALL = 0xFFFF;
    static Mask lane_mask(int rest) { return rest >= LANES ? ALL : (Mask)((1u << rest) - 1); }
    static Vec zero() { return _mm512_setzero_si512(); }
    static Vec set1(T x) { return _mm512_set1_epi32(x); }
    static Vec loadu(const T *p) { return _mm512_loadu_si512(p); }
    static void storeu(T *p, Vec v) { _mm512_storeu_si512(p, v); }
    static Vec load(Mask k, const T *p) { return _mm512_maskz_loadu_epi32(k, p); }
    static void store(T *p, Mask k, Vec v) { _mm512_mask_storeu_epi32(p, k, v); }
    static Vec add(Vec a, Vec b) { return _mm512_add_epi32(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm512_sub_epi32(a, b); }
    static Vec min(Vec a, Vec b) { return _mm512_min_epi32(a, b); }
    static Vec max(Vec a, Vec b) { return _mm512_max_epi32(a, b); }
    static T reduce_add(Vec v) { return (T)_mm512_reduce_add_epi32(v); }
    static Mask cmpeq(Vec a, Vec b) { return _mm512_cmpeq_epi32_mask(a, b); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return _mm512_mask_cmpneq_epi32_mask(k, a, b); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return _mm512_mask_cmpgt_epi32_mask(k, a, b); }
    static unsigned cmpgt_bits(Vec a, Vec b) { return _mm512_cmpgt_epi32_mask(a, b); }
    static Vec blend(Vec src, Mask k, Vec a) { return _mm512_mask_mov_epi32(src, k, a); }
    static IntVec int_set1(int x) { return _mm512_set1_epi32(x); }
    static void int_store(int *p, Mask k, IntVec v) { _mm512_mask_storeu_epi32(p, k, v); }
    static Mask int_nonzero(Mask k, const int *p) {
        return _mm512_mask_cmpneq_epi32_mask(k, _mm512_maskz_loadu_epi32(k, p), _mm512_setzero_si512());
    }
};

#elif defined(__AVX2__)

// AVX2 段。比較の結果は movemask で bit 列にし、マスク付きの読み書きでは要素ごとのマスクに戻す
template<class T>
struct SimdInt<T, 8> {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    using Vec = __m256i;
    using Mask = uint8_t;
    using IntVec = __m128i;
    static constexpr int LANES = 4;
    static constexpr Mask ALL = 0xF;
    static Mask lane_mask(int rest) { return rest >= LANES ? ALL : (Mask)((1u << rest) - 1); }
    static __m256i expand(Mask k) {
        const __m256i bits = _mm256_setr_epi64x(1, 2, 4, 8);
        return _mm256_cmpeq_epi64(_mm256_and_si256(_mm256_set1_epi64x(k), bits), bits);
    }
    static __m128i expand_int(Mask k) {
        const __m128i bits = _mm_setr_epi32(1, 2, 4, 8);
        return _mm_cmpeq_epi32(_mm_and_si128(_mm_set1_epi32(k), bits), bits);
    }
    static Mask to_mask(__m256i v) { return (Mask)_mm256_movemask_pd(_mm256_castsi256_pd(v)); }
    static Vec zero() { return _mm256_setzero_si256(); }
    static Vec set1(T x) { return _mm256_set1_epi64x(x); }
    static Vec loadu(const T *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void storeu(T *p, Vec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static Vec load(Mask k, const T *p) {
        if (k == ALL) return loadu(p);
        return _mm256_maskload_epi64(reinterpret_cast<const long long *>(p), expand(k));
    }
    static void store(T *p, Mask k, Vec v) {
        if (k == ALL) storeu(p, v);
        else _mm256_maskstore_epi64(reinterpret_cast<long long *>(p), expand(k), v);
    }
    static Vec add(Vec a, Vec b) { return _mm256_add_epi64(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm256_sub_epi64(a, b); }
    static Vec min(Vec a, Vec b) { return _mm256_blendv_epi8(a, b, _mm256_cmpgt_epi64(a, b)); }
    static Vec max(Vec a, Vec b) { return _mm256_blendv_epi8(b, a, _mm256_cmpgt_epi64(a, b)); }
    static T reduce_add(Vec v) {
        const __m128i x = _mm_add_epi64(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
        return (T)_mm_cvtsi128_si64(_mm_add_epi64(x, _mm_unpackhi_epi64(x, x)));
    }
    static Mask cmpeq(Vec a, Vec b) { return to_mask(_mm256_cmpeq_epi64(a, b)); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return (Mask)(k & ~cmpeq(a, b)); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return (Mask)(k & to_mask(_mm256_cmpgt_epi64(a, b))); }
    static unsigned cmpgt_bits(Vec a, Vec b) { return _mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(a, b))); }
    static Vec blend(Vec src, Mask k, Vec a) { return _mm256_blendv_epi8(src, a, expand(k)); }
    static IntVec int_set1(int x) { return _mm_set1_epi32(x); }
    static void int_store(int *p, Mask k, IntVec v) {
        if (k == ALL) _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v);
        else _mm_maskstore_epi32(p, expand_int(k), v);
    }
    static Mask int_nonzero(Mask k, const int *p) {
        __m128i x = k == ALL ? _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)) : _mm_maskload_epi32(p, expand_int(k));
        Mask zero = (Mask)_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(x, _mm_setzero_si128())));
        return (Mask)(k & ~zero);
    }
};

template<class T>
struct SimdInt<T, 4> {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    using Vec = __m256i;
    using Mask = uint8_t;
    using IntVec = __m256i;
    static constexpr int LANES = 8;
    static constexpr Mask ALL = 0xFF;
    static Mask lane_mask(int rest) { return rest >= LANES ? ALL : (Mask)((1u << rest) - 1); }
    static __m256i expand(Mask k) {
        const __m256i bits = _mm256_setr_epi32(1, 2, 4, 8, 16, 32, 64, 128);
        return _mm256_cmpeq_epi32(_mm256_and_si256(_mm256_set1_epi32(k), bits), bits);
    }
    static Mask to_mask(__m256i v) { return (Mask)_mm256_movemask_ps(_mm256_castsi256_ps(v)); }
    static Vec zero() { return _mm256_setzero_si256(); }
    static Vec set1(T x) { return _mm256_set1_epi32(x); }
    static Vec loadu(const T *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void storeu(T *p, Vec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static Vec load(Mask k, const T *p) {
        if (k == ALL) return loadu(p);
        return _mm256_maskload_epi32(reinterpret_cast<const int *>(p), expand(k));
    }
    static void store(T *p, Mask k, Vec v) {
        if (k == ALL) storeu(p, v);
        else _mm256_maskstore_epi32(reinterpret_cast<int *>(p), expand(k), v);
    }
    static Vec add(Vec a, Vec b) { return _mm256_add_epi32(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm256_sub_epi32(a, b); }
    static Vec min(Vec a, Vec b) { return _mm256_min_epi32(a, b); }
    static Vec max(Vec a, Vec b) { return _mm256_max_epi32(a, b); }
    static T reduce_add(Vec v) {
        __m128i x = _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
        x = _mm_add_epi32(x, _mm_shuffle_epi32(x, 0x4E));
        x = _mm_add_epi32(x, _mm_shuffle_epi32(x, 0xB1));
        return (T)_mm_cvtsi128_si32(x);
    }
    static Mask cmpeq(Vec a, Vec b) { return to_mask(_mm256_cmpeq_epi32(a, b)); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return (Mask)(k & ~cmpeq(a, b)); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return (Mask)(k & to_mask(_mm256_cmpgt_epi32(a, b))); }
    static unsigned cmpgt_bits(Vec a, Vec b) { return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(a, b))); }
    static Vec blend(Vec src, Mask k, Vec a) { return _mm256_blendv_epi8(src, a, expand(k)); }
    static IntVec int_set1(int x) { return _mm256_set1_epi32(x); }
    static void int_store(int *p, Mask k, IntVec v) {
        if (k == ALL) _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v);
        else _mm256_maskstore_epi32(p, expand(k), v);
    }
    static Mask int_nonzero(Mask k, const int *p) {
        __m256i x = k == ALL ? _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)) : _mm256_maskload_epi32(p, expand(k));
        return (Mask)(k & ~to_mask(_mm256_cmpeq_epi32(x, _mm256_setzero_si256())));
    }
};

#else

// SIMD 命令がない場合は 1 要素ずつ処理する。SSE2 がある場合も 64 bit 整数はこれを使う
template<class T, size_t Size>
struct SimdInt {
    static constexpr const char *NAME = "scalar";
    static constexpr bool VECTORIZED = false;
    using Vec = T;
    using Mask = uint8_t;
    using IntVec = int;
    static constexpr int LANES = 1;
    static constexpr Mask ALL = 1;
    static Mask lane_mask(int rest) { return rest >= 1 ? 1 : 0; }
    static Vec zero() { return T(0); }
    static Vec set1(T x) { return x; }
    static Vec loadu(const T *p) { return *p; }
    static void storeu(T *p, Vec v) { *p = v; }
    static Vec load(Mask k, const T *p) { return k ? *p : T(0); }
    static void store(T *p, Mask k, Vec v) { if (k) *p = v; }
    static Vec add(Vec a, Vec b) {
        using U = std::make_unsigned_t<T>;
        return (T)((U)a + (U)b);
    }
    static Vec sub(Vec a, Vec b) {
        using U = std::make_unsigned_t<T>;
        return (T)((U)a - (U)b);
    }
    static Vec min(Vec a, Vec b) { return std::min(a, b); }
    static Vec max(Vec a, Vec b) { return std::max(a, b); }
    static T reduce_add(Vec v) { return v; }
    static Mask cmpeq(Vec a, Vec b) { return a == b; }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return k && a != b; }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return k && a > b; }
    static unsigned cmpgt_bits(Vec a, Vec b) { return a > b; }
    static Vec blend(Vec src, Mask k, Vec a) { return k ? a : src; }
    static IntVec int_set1(int x) { return x; }
    static void int_store(int *p, Mask k, IntVec v) { if (k) *p = v; }
    static Mask int_nonzero(Mask k, const int *p) { return k && *p != 0; }
};

#if defined(__SSE2__)

// SSE2 段。min, max は比較と論理演算で組み、一部の要素だけの読み書きは一時配列を経由する
template<class T>
struct SimdInt<T, 4> {
    static constexpr const char *NAME = "sse2";
    static constexpr bool VECTORIZED = true;
    using Vec = __m128i;
    using Mask = uint8_t;
    using IntVec = __m128i;
    static constexpr int LANES = 4;
    static constexpr Mask ALL = 0xF;
    static Mask lane_mask(int rest) { return rest >= LANES ? ALL : (Mask)((1u << rest) - 1); }
    static __m128i expand(Mask k) {
        const __m128i bits = _mm_setr_epi32(1, 2, 4, 8);
        return _mm_cmpeq_epi32(_mm_and_si128(_mm_set1_epi32(k), bits), bits);
    }
    static Mask to_mask(__m128i v) { return (Mask)_mm_movemask_ps(_mm_castsi128_ps(v)); }
    // m の要素が全ビット 1 なら a、0 なら b を選ぶ
    static __m128i select(__m128i m, __m128i a, __m128i b) {
        return _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b));
    }
    template<class U>
    static __m128i load_partial(Mask k, const U *p) {
        alignas(16) U buf[4] = {};
        for (int i = 0; i < 4; ++i) if ((k >> i) & 1) buf[i] = p[i];
        return _mm_load_si128(reinterpret_cast<const __m128i *>(buf));
    }
    template<class U>
    static void store_partial(U *p, Mask k, __m128i v) {
        alignas(16) U buf[4];
        _mm_store_si128(reinterpret_cast<__m128i *>(buf), v);
        for (int i = 0; i < 4; ++i) if ((k >> i) & 1) p[i] = buf[i];
    }
    static Vec zero() { return _mm_setzero_si128(); }
    static Vec set1(T x) { return _mm_set1_epi32(x); }
    static Vec loadu(const T *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
    static void storeu(T *p, Vec v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
    static Vec load(Mask k, const T *p) { return k == ALL ? loadu(p) : load_partial(k, p); }
    static void store(T *p, Mask k, Vec v) {
        if (k == ALL) storeu(p, v);
        else store_partial(p, k, v);
    }
    static Vec add(Vec a, Vec b) { return _mm_add_epi32(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm_sub_epi32(a, b); }
    static Vec min(Vec a, Vec b) { return select(_mm_cmpgt_epi32(a, b), b, a); }
    static Vec max(Vec a, Vec b) { return select(_mm_cmpgt_epi32(a, b), a, b); }
    static T reduce_add(Vec v) {
        v = _mm_add_epi32(v, _mm_shuffle_epi32(v, 0x4E));
        v = _mm_add_epi32(v, _mm_shuffle_epi32(v, 0xB1));
        return (T)_mm_cvtsi128_si32(v);
    }
    static Mask cmpeq(Vec a, Vec b) { return to_mask(_mm_cmpeq_epi32(a, b)); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return (Mask)(k & ~cmpeq(a, b)); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return (Mask)(k & to_mask(_mm_cmpgt_epi32(a, b))); }
    static unsigned cmpgt_bits(Vec a, Vec b) { return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(a, b))); }
    static Vec blend(Vec src, Mask k, Vec a) { return select(expand(k), a, src); }
    static IntVec int_set1(int x) { return _mm_set1_epi32(x); }
    static void int_store(int *p, Mask k, IntVec v) {
        if (k == ALL) _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v);
        else store_partial(p, k, v);
    }
    static Mask int_nonzero(Mask k, const int *p) {
        const __m128i x = k == ALL ? _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)) : load_partial(k, p);
        return (Mask)(k & ~to_mask(_mm_cmpeq_epi32(x, _mm_setzero_si128())));
    }
};

#endif

#endif

/// @brief SimdInt で扱える型かを表す / 比較を使う関数はさらに符号付きを要求する
template<class T>
inline constexpr bool simd_int_supported_v =
    std::is_integral_v<T> && !std::is_same_v<T, bool> && (sizeof(T) == 4 || sizeof(T) == 8);

namespace simd_int_detail {

template<class T, class = void>
struct vectorized : std::false_type {};

template<class T>
struct vectorized<T, std::enable_if_t<simd_int_supported_v<T>>> : std::bool_constant<SimdInt<T>::VECTORIZED> {};

template<class S>
constexpr typename S::Mask full_mask(const int rest) {
    return rest >= S::LANES ? S::ALL : (typename S::Mask)((1u << rest) - 1);
}

// f(integral_constant<int, 0>), ..., f(integral_constant<int, COUNT-1>) を展開して呼ぶ
template<class F, size_t... I>
inline void unroll(F &&f, std::index_sequence<I...>) {
    (f(std::integral_constant<int, (int)I>{}), ...);
}

}  // namespace simd_int_detail

/// @brief T で SimdInt の SIMD 実装が使えるかを表す
template<class T>
inline constexpr bool simd_int_vectorized_v = simd_int_detail::vectorized<T>::value;

namespace simd_int_detail {

// N 要素の bit 列を入れる整数型。N <= 31 なら番兵の bit N も 32 bit に収まる
template<int N>
using lane_bits_t = std::conditional_t<(N < 32), uint32_t, uint64_t>;

// a[0, N) で a[i] - b[i] > key (DIFF が false なら a[i] > key) となる要素を bit 列で返す
template<int N, bool DIFF, class T>
inline lane_bits_t<N> greater_bits(const T *a, const T *b, const T key) {
    using S = SimdInt<T>;
    using Bits = lane_bits_t<N>;
    const auto k = S::set1(key);
    Bits bits = 0;
    unroll([&](auto chunk) {
        constexpr int i = decltype(chunk)::value * S::LANES;
        constexpr auto valid = full_mask<S>(N - i);
        auto x = S::load(valid, a + i);
        if constexpr (DIFF) x = S::sub(x, S::load(valid, b + i));
        // 全要素を比べる場合は、Mask 型への切り詰めがない cmpgt_bits を使う
        if constexpr (valid == S::ALL) bits |= Bits(S::cmpgt_bits(x, k)) << i;
        else bits |= Bits(S::cmpgt(valid, x, k)) << i;
    }, std::make_index_sequence<(N + S::LANES - 1) / S::LANES>{});
    return bits;
}

template<class Bits>
inline int countr_zero_bits(const Bits x) {
    if constexpr (sizeof(Bits) == 4) return __builtin_ctz(x);
    else return __builtin_ctzll(x);
}

template<int N, bool DIFF, bool CHECKED, class T>
inline int find_first_greater(const T *a, const T *b, const T key) {
    static_assert(0 < N && N <= 63);
    if constexpr (simd_int_vectorized_v<T> && std::is_signed_v<T>) {
        using Bits = lane_bits_t<N>;
        const Bits bits = greater_bits<N, DIFF>(a, b, key);
        // 番兵の bit N を立てると、該当がないとき N になる
        if constexpr (CHECKED) return countr_zero_bits(Bits(bits | (Bits(1) << N)));
        else return countr_zero_bits(bits);
    } else {
        for (int i = 0; i < N; ++i) {
            const T x = DIFF ? T(a[i] - b[i]) : a[i];
            if (x > key) return i;
        }
        return N;
    }
}

}  // namespace simd_int_detail

/// @brief a[0, N) で a[i] > key となる最小の i を返す / なければ N / 0 < N <= 63
template<int N, class T>
inline int simd_find_first_greater(const T *a, const T key) {
    return simd_int_detail::find_first_greater<N, false, true>(a, a, key);
}

/// @brief a[0, N) で a[i] > key となる最小の i を返す / そのような i が必ずある前提で番兵を省く / 0 < N <= 63
template<int N, class T>
inline int simd_find_first_greater_unchecked(const T *a, const T key) {
    return simd_int_detail::find_first_greater<N, false, false>(a, a, key);
}

/// @brief a[0, N), b[0, N) で a[i] - b[i] > key となる最小の i を返す / なければ N / 0 < N <= 63 / 差は T に収まる前提
template<int N, class T>
inline int simd_find_first_greater_diff(const T *a, const T *b, const T key) {
    return simd_int_detail::find_first_greater<N, true, true>(a, b, key);
}

/// @brief a[0, N), b[0, N) で a[i] - b[i] > key となる最小の i を返す / そのような i が必ずある前提で番兵を省く / 0 < N <= 63 / 差は T に収まる前提
template<int N, class T>
inline int simd_find_first_greater_diff_unchecked(const T *a, const T *b, const T key) {
    return simd_int_detail::find_first_greater<N, true, false>(a, b, key);
}

/// @brief mask の bit i が立つ a[i] (0 <= i < N) の和を返す / 0 < N <= 63
template<int N, class T>
inline T simd_masked_sum(const T *a, uint64_t mask) {
    static_assert(0 < N && N <= 63);
    mask &= (uint64_t(1) << N) - 1;
    if constexpr (simd_int_vectorized_v<T>) {
        using S = SimdInt<T>;
        using Mask = typename S::Mask;
        auto sum = S::zero();
        for (int i = 0; i < N && (mask >> i) != 0; i += S::LANES) {
            sum = S::add(sum, S::load((Mask)((mask >> i) & S::ALL), a + i));
        }
        return S::reduce_add(sum);
    } else {
        T sum = T(0);
        for (; mask != 0; mask &= mask - 1) sum += a[__builtin_ctzll(mask)];
        return sum;
    }
}

namespace simd_int_detail {

// SSE2 だけが使えるか。SimdInt の SSE2 段は 32 bit 整数だけを扱うので、64 bit 整数の一致判定はここで別に組む
#if defined(__SSE2__) && !defined(__AVX2__)
inline constexpr bool SSE2_ONLY = true;
#else
inline constexpr bool SSE2_ONLY = false;
#endif

}  // namespace simd_int_detail

/// @brief simd_equal_bits が SIMD 命令で動くかを表す / SSE2 だけの環境でも 64 bit 整数を扱える
template<class T>
inline constexpr bool simd_equal_bits_vectorized_v =
    simd_int_vectorized_v<T> || (simd_int_detail::SSE2_ONLY && simd_int_supported_v<T> && sizeof(T) == 8);

/// @brief a[0, N) で a[i] == key となる要素を bit 列で返す / 0 < N <= 63
/// 符号の有無は問わない / a[0, N) はすべて読める必要がある
template<int N, class T>
inline simd_int_detail::lane_bits_t<N> simd_equal_bits(const T *a, const T key) {
    static_assert(simd_int_supported_v<T>);
    static_assert(0 < N && N <= 63);
    using Bits = simd_int_detail::lane_bits_t<N>;
    Bits bits = 0;
    if constexpr (simd_int_vectorized_v<T>) {
        using S = SimdInt<T>;
        const auto k = S::set1(key);
        simd_int_detail::unroll([&](auto chunk) {
            constexpr int i = decltype(chunk)::value * S::LANES;
            constexpr auto valid = simd_int_detail::full_mask<S>(N - i);
            const auto eq = S::cmpeq(S::load(valid, a + i), k);
            // load は valid の外を 0 にするので、key が 0 でも valid の外を拾わないよう絞る
            if constexpr (valid == S::ALL) bits |= Bits(eq) << i;
            else bits |= Bits(eq & valid) << i;
        }, std::make_index_sequence<(N + S::LANES - 1) / S::LANES>{});
#if defined(__SSE2__) && !defined(__AVX2__)
    } else if constexpr (sizeof(T) == 8) {
        // 32 bit ずつ比べ、上下の半分がともに一致した 64 bit 要素を一致とする
        const __m128i k = _mm_set1_epi64x((long long)key);
        simd_int_detail::unroll([&](auto chunk) {
            constexpr int i = decltype(chunk)::value * 2;
            if constexpr (i + 1 < N) {
                __m128i e = _mm_cmpeq_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i *>(a + i)), k);
                e = _mm_and_si128(e, _mm_shuffle_epi32(e, _MM_SHUFFLE(2, 3, 0, 1)));
                bits |= Bits(_mm_movemask_pd(_mm_castsi128_pd(e))) << i;
            } else {
                bits |= Bits(a[i] == key) << i;
            }
        }, std::make_index_sequence<(N + 1) / 2>{});
#endif
    } else {
        for (int i = 0; i < N; ++i) bits |= Bits(a[i] == key) << i;
    }
    return bits;
}

/// @brief 64 bit 語の配列をビット演算で扱う / 実装は AVX2, SSE2, 逐次の順に選ぶ
/// shl と shr は各語を r (0 <= r < 64) ビットずらし、語の外へ出たビットは捨てる / 1 の数え上げには simd_bits_popcount を使う
#if defined(__AVX2__)

struct SimdBits {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    static constexpr bool HAS_POPCOUNT64 = false;
    using Vec = __m256i;
    static constexpr int LANES = 4;
    static Vec zero() { return _mm256_setzero_si256(); }
    static Vec ones() { return _mm256_set1_epi64x(-1); }
    static Vec loadu(const uint64_t *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void storeu(uint64_t *p, Vec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static Vec bit_and(Vec a, Vec b) { return _mm256_and_si256(a, b); }
    static Vec bit_or(Vec a, Vec b) { return _mm256_or_si256(a, b); }
    static Vec bit_xor(Vec a, Vec b) { return _mm256_xor_si256(a, b); }
    static Vec shl(Vec a, unsigned r) { return _mm256_sll_epi64(a, _mm_cvtsi32_si128(int(r))); }
    static Vec shr(Vec a, unsigned r) { return _mm256_srl_epi64(a, _mm_cvtsi32_si128(int(r))); }
    static bool nonzero(Vec a) { return !_mm256_testz_si256(a, a); }
    // 4 bit ごとに表を引く
    static Vec byte_popcount(Vec x) {
        const __m256i table = _mm256_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
                                               0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
        const __m256i low = _mm256_set1_epi8(15);
        const __m256i lo = _mm256_and_si256(x, low);
        const __m256i hi = _mm256_and_si256(_mm256_srli_epi16(x, 4), low);
        return _mm256_add_epi8(_mm256_shuffle_epi8(table, lo), _mm256_shuffle_epi8(table, hi));
    }
    static Vec popcount64(Vec x) { return _mm256_sad_epu8(byte_popcount(x), zero()); }
    static Vec add_bytes(Vec a, Vec b) { return _mm256_add_epi8(a, b); }
    static Vec sum_bytes(Vec a) { return _mm256_sad_epu8(a, zero()); }
    static Vec add64(Vec a, Vec b) { return _mm256_add_epi64(a, b); }
    static uint64_t reduce_add64(Vec v) {
        const __m128i x = _mm_add_epi64(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
        return (uint64_t)_mm_cvtsi128_si64(_mm_add_epi64(x, _mm_unpackhi_epi64(x, x)));
    }
};

#elif defined(__SSE2__)

// SSE2 段。1 の数え上げはビット演算で、非 0 判定は比較で行う
struct SimdBits {
    static constexpr const char *NAME = "sse2";
    static constexpr bool VECTORIZED = true;
    static constexpr bool HAS_POPCOUNT64 = false;
    using Vec = __m128i;
    static constexpr int LANES = 2;
    static Vec zero() { return _mm_setzero_si128(); }
    static Vec ones() { return _mm_set1_epi64x(-1); }
    static Vec loadu(const uint64_t *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
    static void storeu(uint64_t *p, Vec v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
    static Vec bit_and(Vec a, Vec b) { return _mm_and_si128(a, b); }
    static Vec bit_or(Vec a, Vec b) { return _mm_or_si128(a, b); }
    static Vec bit_xor(Vec a, Vec b) { return _mm_xor_si128(a, b); }
    static Vec shl(Vec a, unsigned r) { return _mm_sll_epi64(a, _mm_cvtsi32_si128(int(r))); }
    static Vec shr(Vec a, unsigned r) { return _mm_srl_epi64(a, _mm_cvtsi32_si128(int(r))); }
    static bool nonzero(Vec a) { return _mm_movemask_epi8(_mm_cmpeq_epi8(a, _mm_setzero_si128())) != 0xFFFF; }
    // 2 bit、4 bit、8 bit ごとの和を順に作る
    static Vec byte_popcount(Vec x) {
        const __m128i m1 = _mm_set1_epi8(0x55), m2 = _mm_set1_epi8(0x33), m4 = _mm_set1_epi8(0x0F);
        x = _mm_sub_epi8(x, _mm_and_si128(_mm_srli_epi64(x, 1), m1));
        x = _mm_add_epi8(_mm_and_si128(x, m2), _mm_and_si128(_mm_srli_epi64(x, 2), m2));
        return _mm_and_si128(_mm_add_epi8(x, _mm_srli_epi64(x, 4)), m4);
    }
    static Vec popcount64(Vec x) { return _mm_sad_epu8(byte_popcount(x), zero()); }
    static Vec add_bytes(Vec a, Vec b) { return _mm_add_epi8(a, b); }
    static Vec sum_bytes(Vec a) { return _mm_sad_epu8(a, zero()); }
    static Vec add64(Vec a, Vec b) { return _mm_add_epi64(a, b); }
    static uint64_t reduce_add64(Vec v) {
        return (uint64_t)_mm_cvtsi128_si64(_mm_add_epi64(v, _mm_unpackhi_epi64(v, v)));
    }
};

#else

// SIMD 命令がない場合は 1 語ずつ処理する
struct SimdBits {
    static constexpr const char *NAME = "scalar";
    static constexpr bool VECTORIZED = false;
    static constexpr bool HAS_POPCOUNT64 = true;
    using Vec = uint64_t;
    static constexpr int LANES = 1;
    static Vec zero() { return 0; }
    static Vec ones() { return ~uint64_t(0); }
    static Vec loadu(const uint64_t *p) { return *p; }
    static void storeu(uint64_t *p, Vec v) { *p = v; }
    static Vec bit_and(Vec a, Vec b) { return a & b; }
    static Vec bit_or(Vec a, Vec b) { return a | b; }
    static Vec bit_xor(Vec a, Vec b) { return a ^ b; }
    static Vec shl(Vec a, unsigned r) { return a << r; }
    static Vec shr(Vec a, unsigned r) { return a >> r; }
    static bool nonzero(Vec a) { return a != 0; }
    static Vec popcount64(Vec x) { return (Vec)__builtin_popcountll(x); }
    static Vec add64(Vec a, Vec b) { return a + b; }
    static uint64_t reduce_add64(Vec v) { return v; }
};

#endif

/// @brief load(j) が返す Vec (0 <= j < vectors) に含まれる 1 のビット数の和を返す / S は既定のまま使う
template <class Load, class S = SimdBits>
inline uint64_t simd_bits_popcount(const size_t vectors, Load &&load) {
    auto total = S::zero();
    size_t j = 0;
    if constexpr (S::HAS_POPCOUNT64) {
        for (; j < vectors; ++j) total = S::add64(total, S::popcount64(load(j)));
    } else {
        // 1 本で各バイトは 8 以下なので、8 本ずつバイトのまま足しても 64 以下に収まる
        const size_t groups = vectors / 8;
        for (size_t g = 0; g < groups; ++g) {
            auto bytes = S::zero();
            for (size_t k = 0; k < 8; ++k) bytes = S::add_bytes(bytes, S::byte_popcount(load(g * 8 + k)));
            total = S::add64(total, S::sum_bytes(bytes));
        }
        auto bytes = S::zero();
        for (j = groups * 8; j < vectors; ++j) bytes = S::add_bytes(bytes, S::byte_popcount(load(j)));
        total = S::add64(total, S::sum_bytes(bytes));
    }
    return S::reduce_add64(total);
}

namespace simd_vec_detail {

template<class T>
inline constexpr bool signed_int_v = std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<T, bool>;

template<class T, size_t Size>
using if_signed_int_t = std::enable_if_t<signed_int_v<T> && sizeof(T) == Size>;

template<class T>
using index_t = std::conditional_t<sizeof(T) == 8, int64_t, int32_t>;

template<class T, class Idx, int N>
inline std::pair<T, Idx> reduce_argmin(const T (&v)[N], const Idx (&id)[N]) {
    T best = v[0];
    Idx best_id = id[0];
    for (int i = 1; i < N; ++i) {
        if (v[i] < best || (v[i] == best && id[i] < best_id)) best = v[i], best_id = id[i];
    }
    return {best, best_id};
}

// 積を続く演算と融合させない
template<class V>
inline V round_product(V x) { return x; }
#if defined(__SSE2__)
inline double round_product(double x) { asm("" : "+x"(x)); return x; }
inline float round_product(float x) { asm("" : "+x"(x)); return x; }
inline __m128d round_product(__m128d x) { asm("" : "+x"(x)); return x; }
inline __m128 round_product(__m128 x) { asm("" : "+x"(x)); return x; }
#endif
#if defined(__AVX2__)
inline __m256d round_product(__m256d x) { asm("" : "+x"(x)); return x; }
inline __m256 round_product(__m256 x) { asm("" : "+x"(x)); return x; }
#endif

template<class T>
struct Scalar {
    static constexpr const char *NAME = "scalar";
    static constexpr bool VECTORIZED = false;
    static constexpr int LANES = 1;
    using Idx = index_t<T>;
    using Vec = T;
    using IVec = Idx;
    using Mask = bool;
    static Vec set1(T x) { return x; }
    static Vec loadu(const T *p) { return *p; }
    static void storeu(T *p, Vec v) { *p = v; }
    static Vec add(Vec a, Vec b) {
        if constexpr (signed_int_v<T>) return (T)((std::make_unsigned_t<T>)a + (std::make_unsigned_t<T>)b);
        else return a + b;
    }
    static Vec sub(Vec a, Vec b) {
        if constexpr (signed_int_v<T>) return (T)((std::make_unsigned_t<T>)a - (std::make_unsigned_t<T>)b);
        else return a - b;
    }
    static Vec mul_rounded(Vec a, Vec b) { return round_product(a * b); }
    static Vec div(Vec a, Vec b) { return a / b; }
    static Vec sqrt(Vec a) { return std::sqrt(a); }
    static Mask lt(Vec a, Vec b) { return a < b; }
    static Mask mask_and(Mask a, Mask b) { return a && b; }
    static bool any(Mask m) { return m; }
    static Mask load_mask(const Idx *p) { return *p != 0; }
    static Vec select(Mask m, Vec a, Vec b) { return m ? a : b; }
    static IVec iset1(Idx x) { return x; }
    static IVec iota() { return 0; }
    static IVec iloadu(const Idx *p) { return *p; }
    static void istoreu(Idx *p, IVec v) { *p = v; }
    static IVec iadd(IVec a, IVec b) { return a + b; }
    static Mask ieq(IVec a, IVec b) { return a == b; }
    static IVec iselect(Mask m, IVec a, IVec b) { return m ? a : b; }
    static std::pair<T, Idx> argmin(Vec v, IVec id) { return {v, id}; }
};

}  // namespace simd_vec_detail

/// @brief 32 bit と 64 bit の符号付き整数と float, double の SIMD 演算 / 比較の結果は要素ごとのマスクで返す
/// lt は NaN を含む要素で偽になる / mul_rounded は FMA に融合せず逐次と同じ値を返す / argmin は同じ値なら番号が最小の要素を返す
template<class T, class = void>
struct SimdVec : simd_vec_detail::Scalar<T> {};

#if defined(__AVX2__)

template<class T>
struct SimdVec<T, simd_vec_detail::if_signed_int_t<T, 8>> {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 4;
    using Idx = int64_t;
    using Vec = __m256i;
    using IVec = __m256i;
    using Mask = __m256i;
    static Vec set1(T x) { return _mm256_set1_epi64x(x); }
    static Vec loadu(const T *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void storeu(T *p, Vec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static Vec add(Vec a, Vec b) { return _mm256_add_epi64(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm256_sub_epi64(a, b); }
    static Mask lt(Vec a, Vec b) { return _mm256_cmpgt_epi64(b, a); }
    static Mask mask_and(Mask a, Mask b) { return _mm256_and_si256(a, b); }
    static bool any(Mask m) { return !_mm256_testz_si256(m, m); }
    static Mask load_mask(const Idx *p) { return iloadu(p); }
    static Vec select(Mask m, Vec a, Vec b) { return _mm256_blendv_epi8(b, a, m); }
    static IVec iset1(Idx x) { return _mm256_set1_epi64x(x); }
    static IVec iota() { return _mm256_setr_epi64x(0, 1, 2, 3); }
    static IVec iloadu(const Idx *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm256_add_epi64(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm256_cmpeq_epi64(a, b); }
    static IVec iselect(Mask m, IVec a, IVec b) { return _mm256_blendv_epi8(b, a, m); }
    static std::pair<T, Idx> argmin(Vec v, IVec id) {
        alignas(32) T a[4];
        alignas(32) Idx b[4];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

template<class T>
struct SimdVec<T, simd_vec_detail::if_signed_int_t<T, 4>> {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 8;
    using Idx = int32_t;
    using Vec = __m256i;
    using IVec = __m256i;
    using Mask = __m256i;
    static Vec set1(T x) { return _mm256_set1_epi32(x); }
    static Vec loadu(const T *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void storeu(T *p, Vec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static Vec add(Vec a, Vec b) { return _mm256_add_epi32(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm256_sub_epi32(a, b); }
    static Mask lt(Vec a, Vec b) { return _mm256_cmpgt_epi32(b, a); }
    static Mask mask_and(Mask a, Mask b) { return _mm256_and_si256(a, b); }
    static bool any(Mask m) { return !_mm256_testz_si256(m, m); }
    static Mask load_mask(const Idx *p) { return iloadu(p); }
    static Vec select(Mask m, Vec a, Vec b) { return _mm256_blendv_epi8(b, a, m); }
    static IVec iset1(Idx x) { return _mm256_set1_epi32(x); }
    static IVec iota() { return _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7); }
    static IVec iloadu(const Idx *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm256_add_epi32(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm256_cmpeq_epi32(a, b); }
    static IVec iselect(Mask m, IVec a, IVec b) { return _mm256_blendv_epi8(b, a, m); }
    static std::pair<T, Idx> argmin(Vec v, IVec id) {
        alignas(32) T a[8];
        alignas(32) Idx b[8];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

template<>
struct SimdVec<double> {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 4;
    using Idx = int64_t;
    using Vec = __m256d;
    using IVec = __m256i;
    using Mask = __m256d;
    static Vec set1(double x) { return _mm256_set1_pd(x); }
    static Vec loadu(const double *p) { return _mm256_loadu_pd(p); }
    static void storeu(double *p, Vec v) { _mm256_storeu_pd(p, v); }
    static Vec add(Vec a, Vec b) { return _mm256_add_pd(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm256_sub_pd(a, b); }
    static Vec mul_rounded(Vec a, Vec b) { return simd_vec_detail::round_product(_mm256_mul_pd(a, b)); }
    static Vec div(Vec a, Vec b) { return _mm256_div_pd(a, b); }
    static Vec sqrt(Vec a) { return _mm256_sqrt_pd(a); }
    static Mask lt(Vec a, Vec b) { return _mm256_cmp_pd(a, b, _CMP_LT_OQ); }
    static Mask mask_and(Mask a, Mask b) { return _mm256_and_pd(a, b); }
    static bool any(Mask m) { return _mm256_movemask_pd(m) != 0; }
    static Mask load_mask(const Idx *p) { return _mm256_castsi256_pd(iloadu(p)); }
    static Vec select(Mask m, Vec a, Vec b) { return _mm256_blendv_pd(b, a, m); }
    static IVec iset1(Idx x) { return _mm256_set1_epi64x(x); }
    static IVec iota() { return _mm256_setr_epi64x(0, 1, 2, 3); }
    static IVec iloadu(const Idx *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm256_add_epi64(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm256_castsi256_pd(_mm256_cmpeq_epi64(a, b)); }
    static IVec iselect(Mask m, IVec a, IVec b) {
        return _mm256_castpd_si256(_mm256_blendv_pd(_mm256_castsi256_pd(b), _mm256_castsi256_pd(a), m));
    }
    static std::pair<double, Idx> argmin(Vec v, IVec id) {
        alignas(32) double a[4];
        alignas(32) Idx b[4];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

template<>
struct SimdVec<float> {
    static constexpr const char *NAME = "avx2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 8;
    using Idx = int32_t;
    using Vec = __m256;
    using IVec = __m256i;
    using Mask = __m256;
    static Vec set1(float x) { return _mm256_set1_ps(x); }
    static Vec loadu(const float *p) { return _mm256_loadu_ps(p); }
    static void storeu(float *p, Vec v) { _mm256_storeu_ps(p, v); }
    static Vec add(Vec a, Vec b) { return _mm256_add_ps(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm256_sub_ps(a, b); }
    static Vec mul_rounded(Vec a, Vec b) { return simd_vec_detail::round_product(_mm256_mul_ps(a, b)); }
    static Vec div(Vec a, Vec b) { return _mm256_div_ps(a, b); }
    static Vec sqrt(Vec a) { return _mm256_sqrt_ps(a); }
    static Mask lt(Vec a, Vec b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
    static Mask mask_and(Mask a, Mask b) { return _mm256_and_ps(a, b); }
    static bool any(Mask m) { return _mm256_movemask_ps(m) != 0; }
    static Mask load_mask(const Idx *p) { return _mm256_castsi256_ps(iloadu(p)); }
    static Vec select(Mask m, Vec a, Vec b) { return _mm256_blendv_ps(b, a, m); }
    static IVec iset1(Idx x) { return _mm256_set1_epi32(x); }
    static IVec iota() { return _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7); }
    static IVec iloadu(const Idx *p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm256_add_epi32(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm256_castsi256_ps(_mm256_cmpeq_epi32(a, b)); }
    static IVec iselect(Mask m, IVec a, IVec b) {
        return _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(b), _mm256_castsi256_ps(a), m));
    }
    static std::pair<float, Idx> argmin(Vec v, IVec id) {
        alignas(32) float a[8];
        alignas(32) Idx b[8];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

#elif defined(__SSE2__)

// SSE2 段。選択は論理演算で組み、64 bit の番号の一致は 32 bit ずつ比べる
namespace simd_vec_detail {
inline __m128i select128(__m128i m, __m128i a, __m128i b) { return _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b)); }
inline __m128i cmpeq64(__m128i a, __m128i b) {
    const __m128i e = _mm_cmpeq_epi32(a, b);
    return _mm_and_si128(e, _mm_shuffle_epi32(e, 0xB1));
}
}  // namespace simd_vec_detail

template<class T>
struct SimdVec<T, simd_vec_detail::if_signed_int_t<T, 4>> {
    static constexpr const char *NAME = "sse2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 4;
    using Idx = int32_t;
    using Vec = __m128i;
    using IVec = __m128i;
    using Mask = __m128i;
    static Vec set1(T x) { return _mm_set1_epi32(x); }
    static Vec loadu(const T *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
    static void storeu(T *p, Vec v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
    static Vec add(Vec a, Vec b) { return _mm_add_epi32(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm_sub_epi32(a, b); }
    static Mask lt(Vec a, Vec b) { return _mm_cmplt_epi32(a, b); }
    static Mask mask_and(Mask a, Mask b) { return _mm_and_si128(a, b); }
    static bool any(Mask m) { return _mm_movemask_epi8(m) != 0; }
    static Mask load_mask(const Idx *p) { return iloadu(p); }
    static Vec select(Mask m, Vec a, Vec b) { return simd_vec_detail::select128(m, a, b); }
    static IVec iset1(Idx x) { return _mm_set1_epi32(x); }
    static IVec iota() { return _mm_setr_epi32(0, 1, 2, 3); }
    static IVec iloadu(const Idx *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm_add_epi32(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm_cmpeq_epi32(a, b); }
    static IVec iselect(Mask m, IVec a, IVec b) { return simd_vec_detail::select128(m, a, b); }
    static std::pair<T, Idx> argmin(Vec v, IVec id) {
        alignas(16) T a[4];
        alignas(16) Idx b[4];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

template<>
struct SimdVec<double> {
    static constexpr const char *NAME = "sse2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 2;
    using Idx = int64_t;
    using Vec = __m128d;
    using IVec = __m128i;
    using Mask = __m128d;
    static Vec set1(double x) { return _mm_set1_pd(x); }
    static Vec loadu(const double *p) { return _mm_loadu_pd(p); }
    static void storeu(double *p, Vec v) { _mm_storeu_pd(p, v); }
    static Vec add(Vec a, Vec b) { return _mm_add_pd(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm_sub_pd(a, b); }
    static Vec mul_rounded(Vec a, Vec b) { return simd_vec_detail::round_product(_mm_mul_pd(a, b)); }
    static Vec div(Vec a, Vec b) { return _mm_div_pd(a, b); }
    static Vec sqrt(Vec a) { return _mm_sqrt_pd(a); }
    static Mask lt(Vec a, Vec b) { return _mm_cmplt_pd(a, b); }
    static Mask mask_and(Mask a, Mask b) { return _mm_and_pd(a, b); }
    static bool any(Mask m) { return _mm_movemask_pd(m) != 0; }
    static Mask load_mask(const Idx *p) { return _mm_castsi128_pd(iloadu(p)); }
    static Vec select(Mask m, Vec a, Vec b) { return _mm_or_pd(_mm_and_pd(m, a), _mm_andnot_pd(m, b)); }
    static IVec iset1(Idx x) { return _mm_set1_epi64x(x); }
    static IVec iota() { return _mm_set_epi64x(1, 0); }
    static IVec iloadu(const Idx *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm_add_epi64(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm_castsi128_pd(simd_vec_detail::cmpeq64(a, b)); }
    static IVec iselect(Mask m, IVec a, IVec b) { return simd_vec_detail::select128(_mm_castpd_si128(m), a, b); }
    static std::pair<double, Idx> argmin(Vec v, IVec id) {
        alignas(16) double a[2];
        alignas(16) Idx b[2];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

template<>
struct SimdVec<float> {
    static constexpr const char *NAME = "sse2";
    static constexpr bool VECTORIZED = true;
    static constexpr int LANES = 4;
    using Idx = int32_t;
    using Vec = __m128;
    using IVec = __m128i;
    using Mask = __m128;
    static Vec set1(float x) { return _mm_set1_ps(x); }
    static Vec loadu(const float *p) { return _mm_loadu_ps(p); }
    static void storeu(float *p, Vec v) { _mm_storeu_ps(p, v); }
    static Vec add(Vec a, Vec b) { return _mm_add_ps(a, b); }
    static Vec sub(Vec a, Vec b) { return _mm_sub_ps(a, b); }
    static Vec mul_rounded(Vec a, Vec b) { return simd_vec_detail::round_product(_mm_mul_ps(a, b)); }
    static Vec div(Vec a, Vec b) { return _mm_div_ps(a, b); }
    static Vec sqrt(Vec a) { return _mm_sqrt_ps(a); }
    static Mask lt(Vec a, Vec b) { return _mm_cmplt_ps(a, b); }
    static Mask mask_and(Mask a, Mask b) { return _mm_and_ps(a, b); }
    static bool any(Mask m) { return _mm_movemask_ps(m) != 0; }
    static Mask load_mask(const Idx *p) { return _mm_castsi128_ps(iloadu(p)); }
    static Vec select(Mask m, Vec a, Vec b) { return _mm_or_ps(_mm_and_ps(m, a), _mm_andnot_ps(m, b)); }
    static IVec iset1(Idx x) { return _mm_set1_epi32(x); }
    static IVec iota() { return _mm_setr_epi32(0, 1, 2, 3); }
    static IVec iloadu(const Idx *p) { return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p)); }
    static void istoreu(Idx *p, IVec v) { _mm_storeu_si128(reinterpret_cast<__m128i *>(p), v); }
    static IVec iadd(IVec a, IVec b) { return _mm_add_epi32(a, b); }
    static Mask ieq(IVec a, IVec b) { return _mm_castsi128_ps(_mm_cmpeq_epi32(a, b)); }
    static IVec iselect(Mask m, IVec a, IVec b) { return simd_vec_detail::select128(_mm_castps_si128(m), a, b); }
    static std::pair<float, Idx> argmin(Vec v, IVec id) {
        alignas(16) float a[4];
        alignas(16) Idx b[4];
        storeu(a, v), istoreu(b, id);
        return simd_vec_detail::reduce_argmin(a, b);
    }
};

#endif

}  // namespace titan23
