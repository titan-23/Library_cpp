/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/simd_int.cpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#if (defined(__AVX512F__) && defined(__AVX512VL__)) || defined(__AVX2__)
#include <immintrin.h>
#endif

namespace titan23 {

// 32 bit / 64 bit 符号付き整数の SIMD 演算を、命令セットによらない名前でまとめる。
// コンパイル時に AVX-512 (F, VL) > AVX2 > 逐次 の順で実装を選ぶ。
//
// - T の配列は Vec 1 本 (LANES 要素) ずつ扱う。int_* は T の配列と同じ要素数の int 配列を扱う。
// - Mask は要素 i を bit i に対応させた整数。lane_mask(rest) は先頭 min(rest, LANES) 要素を表す。
// - load / store / int_store / int_nonzero は Mask の要素だけを読み書きする。load は他の要素を 0 にする。
// - add は 2 の補数で巡回する (あふれた要素は使わない前提)。
//
// 例: using V = SimdInt<long long>;
//     for (int j = 0; j < n; j += V::LANES) { auto m = V::lane_mask(n - j); auto x = V::load(m, a + j); ... }
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
    static Vec set1(T x) { return _mm512_set1_epi64(x); }
    static Vec loadu(const T *p) { return _mm512_loadu_si512(p); }
    static void storeu(T *p, Vec v) { _mm512_storeu_si512(p, v); }
    static Vec load(Mask k, const T *p) { return _mm512_maskz_loadu_epi64(k, p); }
    static void store(T *p, Mask k, Vec v) { _mm512_mask_storeu_epi64(p, k, v); }
    static Vec add(Vec a, Vec b) { return _mm512_add_epi64(a, b); }
    static Vec min(Vec a, Vec b) { return _mm512_min_epi64(a, b); }
    static Vec max(Vec a, Vec b) { return _mm512_max_epi64(a, b); }
    static Mask cmpeq(Vec a, Vec b) { return _mm512_cmpeq_epi64_mask(a, b); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return _mm512_mask_cmpneq_epi64_mask(k, a, b); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return _mm512_mask_cmpgt_epi64_mask(k, a, b); }
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
    static Vec set1(T x) { return _mm512_set1_epi32(x); }
    static Vec loadu(const T *p) { return _mm512_loadu_si512(p); }
    static void storeu(T *p, Vec v) { _mm512_storeu_si512(p, v); }
    static Vec load(Mask k, const T *p) { return _mm512_maskz_loadu_epi32(k, p); }
    static void store(T *p, Mask k, Vec v) { _mm512_mask_storeu_epi32(p, k, v); }
    static Vec add(Vec a, Vec b) { return _mm512_add_epi32(a, b); }
    static Vec min(Vec a, Vec b) { return _mm512_min_epi32(a, b); }
    static Vec max(Vec a, Vec b) { return _mm512_max_epi32(a, b); }
    static Mask cmpeq(Vec a, Vec b) { return _mm512_cmpeq_epi32_mask(a, b); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return _mm512_mask_cmpneq_epi32_mask(k, a, b); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return _mm512_mask_cmpgt_epi32_mask(k, a, b); }
    static Vec blend(Vec src, Mask k, Vec a) { return _mm512_mask_mov_epi32(src, k, a); }
    static IntVec int_set1(int x) { return _mm512_set1_epi32(x); }
    static void int_store(int *p, Mask k, IntVec v) { _mm512_mask_storeu_epi32(p, k, v); }
    static Mask int_nonzero(Mask k, const int *p) {
        return _mm512_mask_cmpneq_epi32_mask(k, _mm512_maskz_loadu_epi32(k, p), _mm512_setzero_si512());
    }
};

#elif defined(__AVX2__)

// AVX2 には比較結果を入れるマスクレジスタがないため、比較は movemask でビット列に変換し、
// マスク付きの読み書きではビット列を要素ごとの全ビット 1/0 に戻して maskload / maskstore を使う。
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
    static Vec min(Vec a, Vec b) { return _mm256_blendv_epi8(a, b, _mm256_cmpgt_epi64(a, b)); }
    static Vec max(Vec a, Vec b) { return _mm256_blendv_epi8(b, a, _mm256_cmpgt_epi64(a, b)); }
    static Mask cmpeq(Vec a, Vec b) { return to_mask(_mm256_cmpeq_epi64(a, b)); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return (Mask)(k & ~cmpeq(a, b)); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return (Mask)(k & to_mask(_mm256_cmpgt_epi64(a, b))); }
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
    static Vec min(Vec a, Vec b) { return _mm256_min_epi32(a, b); }
    static Vec max(Vec a, Vec b) { return _mm256_max_epi32(a, b); }
    static Mask cmpeq(Vec a, Vec b) { return to_mask(_mm256_cmpeq_epi32(a, b)); }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return (Mask)(k & ~cmpeq(a, b)); }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return (Mask)(k & to_mask(_mm256_cmpgt_epi32(a, b))); }
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

// SIMD 命令がない場合は 1 要素ずつ処理する
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
    static Vec set1(T x) { return x; }
    static Vec loadu(const T *p) { return *p; }
    static void storeu(T *p, Vec v) { *p = v; }
    static Vec load(Mask k, const T *p) { return k ? *p : T(0); }
    static void store(T *p, Mask k, Vec v) { if (k) *p = v; }
    static Vec add(Vec a, Vec b) {
        using U = std::make_unsigned_t<T>;
        return (T)((U)a + (U)b);
    }
    static Vec min(Vec a, Vec b) { return std::min(a, b); }
    static Vec max(Vec a, Vec b) { return std::max(a, b); }
    static Mask cmpeq(Vec a, Vec b) { return a == b; }
    static Mask cmpneq(Mask k, Vec a, Vec b) { return k && a != b; }
    static Mask cmpgt(Mask k, Vec a, Vec b) { return k && a > b; }
    static Vec blend(Vec src, Mask k, Vec a) { return k ? a : src; }
    static IntVec int_set1(int x) { return x; }
    static void int_store(int *p, Mask k, IntVec v) { if (k) *p = v; }
    static Mask int_nonzero(Mask k, const int *p) { return k && *p != 0; }
};

#endif

}  // namespace titan23
