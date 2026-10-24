// Gcd・Affine・MaxSubSumについて同じ操作列を比較する補足ベンチ
// 操作生成・木の構築・独立参照との照合は計測外
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

#include "titan_cpplib/ds/dynamic_segment_tree_util.cpp"
#ifndef DST_BENCH_IMPLEMENTATION
#define DST_BENCH_IMPLEMENTATION "patricia"
#endif

using namespace std;

namespace {
using Index = long long;
using Clock = chrono::steady_clock;
namespace util = titan23::dynamic_segment_tree;

struct Options {
    Index n = 1000000000LL;
    size_t q = 500000;
    uint64_t seed = 23;
    string distribution = "wide", kind = "plain", monoid = "gcd";
    bool verify = false;
};

/// @brief 引数を読み取り固定条件の項目を検証する / O(argc)
Options parse_options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--help") {
            cout << "--n N --q Q --seed SEED --distribution wide|short|local\n"
                         "--kind plain --monoid gcd|affine|max_sub_sum --verify\n"
                         "Q/2 set and Q/2 prod, shuffled; unspecified points are the monoid identity\n";
            exit(0);
        }
        if (arg == "--verify") { opt.verify = true; continue; }
        if (++i == argc) throw invalid_argument("missing value for " + arg);
        const string value = argv[i];
        if (arg == "--n") opt.n = stoll(value);
        else if (arg == "--q") opt.q = stoull(value);
        else if (arg == "--seed") opt.seed = stoull(value);
        else if (arg == "--distribution") opt.distribution = value;
        else if (arg == "--kind") opt.kind = value;
        else if (arg == "--monoid") opt.monoid = value;
        else throw invalid_argument("unknown option " + arg);
    }
    if (opt.n <= 0 || opt.n > numeric_limits<Index>::max() / 9 ||
        opt.q == 0 || opt.q % 2) throw invalid_argument("require positive n, positive even q, and 9*n fitting int64");
    if (opt.distribution != "wide" && opt.distribution != "short" && opt.distribution != "local")
        throw invalid_argument("invalid distribution");
    if (opt.kind != "plain") throw invalid_argument("this supplemental benchmark uses plain trees only");
    if (opt.monoid != "gcd" && opt.monoid != "affine" && opt.monoid != "max_sub_sum")
        throw invalid_argument("invalid monoid");
    return opt;
}

struct Query { Index l, r; long long value; bool update; };

/// @brief 既存ベンチと同じ分布で区間を生成する / O(1)
pair<Index, Index> interval(const Options& opt, mt19937_64& rng) {
    if (opt.distribution == "short") {
        const Index l = uniform_int_distribution<Index>(0, opt.n - 1)(rng);
        const Index length = uniform_int_distribution<Index>(1, min<Index>(1024, opt.n))(rng);
        return {l, l + min(length, opt.n - l)};
    }
    const Index width = opt.distribution == "local" ? min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    Index l = uniform_int_distribution<Index>(0, width - 1)(rng);
    Index r = uniform_int_distribution<Index>(0, width - 1)(rng);
    if (l > r) swap(l, r);
    return {l + offset, r + offset + 1};
}

/// @brief 既存ベンチと同じ乱数列で操作を生成する / O(Q)
vector<Query> generate(const Options& opt) {
    mt19937_64 rng(opt.seed);
    vector<Query> queries;
    queries.reserve(opt.q);
    const Index width = opt.distribution == "local" ? min<Index>(1000000, opt.n) : opt.n;
    const Index offset = (opt.n - width) / 2;
    for (size_t i = 0; i < opt.q; ++i) {
        Index l, r;
        if (i < opt.q / 2) {
            l = offset + uniform_int_distribution<Index>(0, width - 1)(rng);
            r = l + 1;
        } else {
            const auto range = interval(opt, rng);
            l = range.first;
            r = range.second;
        }
        queries.push_back({l, r, uniform_int_distribution<long long>(1, 9)(rng), i < opt.q / 2});
    }
    shuffle(queries.begin(), queries.end(), rng);
    return queries;
}

/// @brief 結果の1フィールドを検査値へ追加する / O(1)
void checksum_add(uint64_t& checksum, long long value) {
    checksum ^= static_cast<uint64_t>(value) + UINT64_C(0x9e3779b97f4a7c15) +
                (checksum << 6) + (checksum >> 2);
}

/// @brief プロセスの最大常駐メモリを取得する / O(1)
long long peak_rss_bytes() {
#if defined(__linux__) || defined(__APPLE__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return -1;
#if defined(__APPLE__)
    return usage.ru_maxrss;
#else
    return static_cast<long long>(usage.ru_maxrss) * 1024;
#endif
#else
    return -1;
#endif
}


// Affineの係数型は64bitを使い、全ての演算をmod 1e9+7で行う
struct Modulo {
    static constexpr long long modulus = 1000000007;
    long long value;

    /// @brief 非負整数を剰余へ変換する / O(1)
    Modulo(long long x = 0) : value(x % modulus) {}

    /// @brief 2つの剰余を加算する / O(1)
    friend Modulo operator+(Modulo a, Modulo b) {
        const long long sum = a.value + b.value;
        Modulo result;
        result.value = sum < modulus ? sum : sum - modulus;
        return result;
    }

    /// @brief 2つの剰余を乗算する / O(1)
    friend Modulo operator*(Modulo a, Modulo b) {
        return Modulo(a.value * b.value);
    }
};

struct GcdMonoid {
    using Value = long long;
    using Tree = util::SegGcd<long long>;

    /// @brief 入力値を要素へ変換する / O(1)
    static Value make(long long value) { return value; }

    /// @brief 単位元を返す / O(1)
    static Value identity() { return util::e_gcd<long long>(); }

    /// @brief 座標順に2区間を結合する / O(logV)
    static Value combine(Value a, Value b) { return util::op_gcd(a, b); }

    /// @brief 結果の全フィールドを検査値へ追加する / O(1)
    static void checksum(uint64_t& result, Value value) { checksum_add(result, value); }
};

struct AffineMonoid {
    using Value = util::Affine<Modulo>;
    using Tree = util::SegAffine<Modulo>;

    /// @brief 入力値からvalue*x+1を作る / O(1)
    static Value make(long long value) { return {Modulo(value), Modulo(1)}; }

    /// @brief 単位元を返す / O(1)
    static Value identity() { return util::e_affine<Modulo>(); }

    /// @brief 左の関数の後に右の関数を適用する / O(1)
    static Value combine(Value a, Value b) { return util::op_affine(a, b); }

    /// @brief 結果の全フィールドを検査値へ追加する / O(1)
    static void checksum(uint64_t& result, Value value) {
        checksum_add(result, value.a.value);
        checksum_add(result, value.b.value);
    }
};

struct MaxSubSumMonoid {
    using Value = util::MaxSubSum<long long>;
    using Tree = util::SegMaxSubSum<long long>;

    /// @brief 入力値から1要素の集約値を作る / O(1)
    static Value make(long long value) { return Value(value); }

    /// @brief 単位元を返す / O(1)
    static Value identity() { return util::e_max_sub_sum<long long>(); }

    /// @brief 座標順に2区間を結合する / O(1)
    static Value combine(Value a, Value b) { return util::op_max_sub_sum(a, b); }

    /// @brief 結果の全フィールドを検査値へ追加する / O(1)
    static void checksum(uint64_t& result, Value value) {
        checksum_add(result, value.sum);
        checksum_add(result, value.pre);
        checksum_add(result, value.suf);
        checksum_add(result, value.ans);
    }
};

// 更新座標だけを圧縮した配列セグ木で、疎な木の分岐やノードを使わずに照合する
// 全ての未登録座標は単位元なので、その区間を省略しても積の順序は変わらない
/// @brief 独立した配列セグ木で全クエリの検査値を求める / O(QlogQ)
template<class Monoid>
uint64_t reference_checksum(const vector<Query>& queries) {
    using Value = typename Monoid::Value;
    vector<Index> xs;
    for (const auto& query : queries) if (query.update) xs.push_back(query.l);
    sort(xs.begin(), xs.end());
    xs.erase(unique(xs.begin(), xs.end()), xs.end());
    size_t size = 1;
    while (size < xs.size()) size *= 2;
    vector<Value> data(size * 2, Monoid::identity());
    uint64_t checksum = 0;
    for (const auto& query : queries) {
        if (query.update) {
            size_t at = size + static_cast<size_t>(lower_bound(xs.begin(), xs.end(), query.l) - xs.begin());
            data[at] = Monoid::make(query.value);
            while ((at /= 2) != 0) data[at] = Monoid::combine(data[at * 2], data[at * 2 + 1]);
        } else {
            size_t l = size + static_cast<size_t>(lower_bound(xs.begin(), xs.end(), query.l) - xs.begin());
            size_t r = size + static_cast<size_t>(lower_bound(xs.begin(), xs.end(), query.r) - xs.begin());
            Value left = Monoid::identity(), right = Monoid::identity();
            while (l < r) {
                if (l & 1) left = Monoid::combine(left, data[l++]);
                if (r & 1) right = Monoid::combine(data[--r], right);
                l /= 2;
                r /= 2;
            }
            Monoid::checksum(checksum, Monoid::combine(left, right));
        }
    }
    return checksum;
}

/// @brief 更新と区間集約だけを計測してJSONを出力する / O(QlogN)
template<class Monoid>
void run(const Options& opt, const vector<Query>& queries) {
    typename Monoid::Tree tree(opt.n);
    uint64_t checksum = 0;
    const auto start = Clock::now();
    for (const auto& query : queries) {
        if (query.update) tree.set(query.l, Monoid::make(query.value));
        else Monoid::checksum(checksum, tree.prod(query.l, query.r));
    }
    const auto stop = Clock::now();
    const auto rss = peak_rss_bytes();
    const auto count = tree.node_count();
    if (opt.verify && reference_checksum<Monoid>(queries) != checksum)
        throw runtime_error("independent coordinate-compressed segment tree reference disagrees");
    cout << fixed << setprecision(3)
         << "{\"implementation\":\"" << DST_BENCH_IMPLEMENTATION
         << "\",\"kind\":\"" << opt.kind << "\",\"monoid\":\"" << opt.monoid
         << "\",\"distribution\":\"" << opt.distribution
         << "\",\"n\":" << opt.n << ",\"q\":" << opt.q << ",\"seed\":" << opt.seed
         << ",\"initial\":\"identity\",\"elapsed_ms\":"
         << chrono::duration<double, milli>(stop - start).count()
         << ",\"node_count\":" << count << ",\"peak_rss_bytes\":" << rss
         << ",\"checksum\":" << checksum << ",\"verified\":" << (opt.verify ? "true" : "false") << "}\n";
}
}  // namespace

/// @brief モノイドを選択して固定操作列を評価する / O(QlogN)
int main(int argc, char** argv) {
    try {
        const auto opt = parse_options(argc, argv);
        const auto queries = generate(opt);
        if (opt.monoid == "gcd") run<GcdMonoid>(opt, queries);
        else if (opt.monoid == "affine") run<AffineMonoid>(opt, queries);
        else run<MaxSubSumMonoid>(opt, queries);
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
}
