/// https://github.com/titan-23/Library_cpp/blob/main/test/math/big_int_boost_benchmark.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/integer.hpp>
#include <boost/version.hpp>

#include "titan_cpplib/math/big_int.cpp"

using namespace std;
using namespace titan23;
using boost::multiprecision::cpp_int;

#ifndef BIG_INT_CMP_MS
#define BIG_INT_CMP_MS 20.0
#endif

#ifndef BIG_INT_CMP_MAX_MS
#define BIG_INT_CMP_MAX_MS 200.0
#endif

#ifndef BIG_INT_CMP_ROUNDS
#define BIG_INT_CMP_ROUNDS 5
#endif

#ifndef BIG_INT_CMP_REPS
#define BIG_INT_CMP_REPS 262144
#endif

#ifndef BIG_INT_CMP_MEM
#define BIG_INT_CMP_MEM (size_t{8} << 20)
#endif

using Clk = chrono::steady_clock;
using u64 = uint64_t;

enum class Op { Add, Mul, Sqr, Div, Mod, Qr, Gcd, Parse, Str };

struct Case {
    const char *tag;
    Op op;
    size_t n;
    size_t m;
};

struct Data {
    vector<string> sa, sb;
    vector<BigInt> a, b;
    vector<cpp_int> x, y;
};

struct Res {
    size_t reps;
    double us_a;
    double us_b;
    u64 hash;
};

constexpr size_t P = 8;

string make_num(size_t n, u64 salt, size_t id) {
    mt19937_64 rng(0x243f6a8885a308d3ULL ^ (n * 0x9e3779b97f4a7c15ULL) ^ salt ^ id);
    string s(n, '0');
    s[0] = static_cast<char>('1' + rng() % 9);
    for (size_t i = 1; i < n; ++i) s[i] = static_cast<char>('0' + rng() % 10);
    return s;
}

string dec(const BigInt &a) { return a.to_string(); }

string dec(const cpp_int &a) { return a.str(); }

u64 eat(u64 h, string_view s) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    h ^= s.size();
    return h * 1099511628211ULL;
}

bool is_div(Op op) { return op == Op::Div || op == Op::Mod || op == Op::Qr; }

Data make_data(const Case &c) {
    Data d;
    d.sa.reserve(P);
    d.sb.reserve(P);
    d.a.reserve(P);
    d.b.reserve(P);
    d.x.reserve(P);
    d.y.reserve(P);
    for (size_t i = 0; i < P; ++i) {
        string s = make_num(c.n, 0, i);
        string t = c.m == 0 ? "1" : make_num(c.m, 0x94d049bb133111ebULL, i);
        if (c.op == Op::Mul && s == t) t.back() = t.back() == '0' ? '1' : '0';
        if (is_div(c.op) && s.size() == t.size() && s < t) swap(s, t);
        d.sa.push_back(s);
        d.sb.push_back(t);
        d.a.emplace_back(s);
        d.b.emplace_back(t);
        d.x.emplace_back(s);
        d.y.emplace_back(t);
        if (dec(d.a.back()) != s || dec(d.x.back()) != s) throw runtime_error("input parse mismatch");
        if (c.m != 0 && (dec(d.b.back()) != t || dec(d.y.back()) != t)) {
            throw runtime_error("input parse mismatch");
        }
    }
    return d;
}

template <Op op, class T> T calc(const T &a, const T &b) {
    if constexpr (op == Op::Add) return T(a + b);
    if constexpr (op == Op::Mul) return T(a * b);
    if constexpr (op == Op::Sqr) return T(a * a);
    if constexpr (op == Op::Div) return T(a / b);
    if constexpr (op == Op::Mod) return T(a % b);
    if constexpr (op == Op::Gcd) {
        if constexpr (is_same_v<T, BigInt>) {
            return titan23::gcd(a, b);
        } else {
            return boost::multiprecision::gcd(a, b);
        }
    }
}

pair<BigInt, BigInt> qr(const BigInt &a, const BigInt &b) { return titan23::divmod(a, b); }

pair<cpp_int, cpp_int> qr(const cpp_int &a, const cpp_int &b) {
    pair<cpp_int, cpp_int> z;
    boost::multiprecision::divide_qr(a, b, z.first, z.second);
    return z;
}

template <class T> void use(const vector<T> &v) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(v.data()), "g"(v.size()) : "memory");
#else
    volatile const T *p = v.data();
    (void)p;
#endif
}

template <class T, class F> double lap(vector<T> &out, size_t reps, F f) {
    out.clear();
    const auto st = Clk::now();
    for (size_t i = 0; i < reps; ++i) out.emplace_back(f(i));
    const double us = chrono::duration<double, micro>(Clk::now() - st).count();
    use(out);
    return us;
}

template <class T, class F> double probe(F f, size_t cap) {
    size_t reps = 1;
    vector<T> out;
    while (true) {
        out.clear();
        out.reserve(reps);
        lap(out, reps, f);
        const double us = lap(out, reps, f);
        if (us >= 500.0 || reps == cap) return us / reps;
        const double mul = 500.0 / max(us, 0.01);
        const size_t k = clamp<size_t>(static_cast<size_t>(ceil(mul)), 2, 32);
        reps = min(cap, reps * k);
    }
}

size_t pick_reps(double a, double b, size_t cap) {
    const double lo = max(min(a, b), 0.001);
    const double hi = max(max(a, b), 0.001);
    size_t reps = static_cast<size_t>(ceil(BIG_INT_CMP_MS * 1000.0 / lo));
    const size_t lim = max<size_t>(1, static_cast<size_t>(BIG_INT_CMP_MAX_MS * 1000.0 / hi));
    reps = clamp(reps, size_t{1}, min(cap, lim));
    const size_t all = cap / P * P;
    if (all >= P) reps = min(all, max(P, (reps + P - 1) / P * P));
    return reps;
}

double med(vector<double> v) {
    sort(v.begin(), v.end());
    return v[v.size() / 2];
}

u64 check(const Case &c, const vector<BigInt> &a, const vector<cpp_int> &b) {
    if (a.size() != b.size()) throw runtime_error("result size mismatch");
    u64 h = 1469598103934665603ULL;
    for (size_t i = 0; i < a.size(); ++i) {
        const string x = dec(a[i]);
        const string y = dec(b[i]);
        if (x != y) throw runtime_error(string("result mismatch: ") + c.tag);
        h = eat(h, x);
    }
    return h;
}

u64 check(const Case &c, const vector<pair<BigInt, BigInt>> &a, const vector<pair<cpp_int, cpp_int>> &b) {
    if (a.size() != b.size()) throw runtime_error("result size mismatch");
    u64 h = 1469598103934665603ULL;
    for (size_t i = 0; i < a.size(); ++i) {
        const string aq = dec(a[i].first), ar = dec(a[i].second);
        const string bq = dec(b[i].first), br = dec(b[i].second);
        if (aq != bq || ar != br) throw runtime_error(string("result mismatch: ") + c.tag);
        h = eat(eat(h, aq), ar);
    }
    return h;
}

u64 check(const Case &c, const vector<string> &a, const vector<string> &b) {
    if (a.size() != b.size()) throw runtime_error("result size mismatch");
    u64 h = 1469598103934665603ULL;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) throw runtime_error(string("result mismatch: ") + c.tag);
        h = eat(h, a[i]);
    }
    return h;
}

size_t out_bytes(const Case &c) {
    size_t n = c.n;
    if (c.op == Op::Mul || c.op == Op::Sqr) n += c.op == Op::Mul ? c.m : c.n;
    if (c.op == Op::Div) n = max<size_t>(1, c.n - c.m + 1);
    if (c.op == Op::Mod) n = c.m;
    if (c.op == Op::Qr) n = max<size_t>(128, c.n + 64);
    if (c.op == Op::Gcd) n = min(c.n, c.m);
    return max<size_t>(64, n);
}

template <class A, class B, class FA, class FB> Res run(const Case &c, size_t cap, FA fa, FB fb) {
    vector<A> ca;
    vector<B> cb;
    ca.reserve(P);
    cb.reserve(P);
    for (size_t i = 0; i < P; ++i) {
        ca.emplace_back(fa(i));
        cb.emplace_back(fb(i));
    }
    const u64 h = check(c, ca, cb);
    const double pa = probe<A>(fa, cap);
    const double pb = probe<B>(fb, cap);
    const size_t reps = pick_reps(pa, pb, cap);
    vector<A> a;
    vector<B> b;
    a.reserve(reps);
    b.reserve(reps);
    lap(a, reps, fa);
    lap(b, reps, fb);
    vector<double> ta, tb;
    ta.reserve(BIG_INT_CMP_ROUNDS);
    tb.reserve(BIG_INT_CMP_ROUNDS);
    for (int r = 0; r < BIG_INT_CMP_ROUNDS; ++r) {
        if ((r & 1) == 0) {
            ta.push_back(lap(a, reps, fa));
            tb.push_back(lap(b, reps, fb));
        } else {
            tb.push_back(lap(b, reps, fb));
            ta.push_back(lap(a, reps, fa));
        }
    }
    return {reps, med(ta) / reps, med(tb) / reps, h};
}

template <Op op> Res run_num(const Case &c, const Data &d, size_t cap) {
    auto fa = [&](size_t i) { return calc<op>(d.a[i % P], d.b[i % P]); };
    auto fb = [&](size_t i) { return calc<op>(d.x[i % P], d.y[i % P]); };
    return run<BigInt, cpp_int>(c, cap, fa, fb);
}

Res run_qr(const Case &c, const Data &d, size_t cap) {
    auto fa = [&](size_t i) { return qr(d.a[i % P], d.b[i % P]); };
    auto fb = [&](size_t i) { return qr(d.x[i % P], d.y[i % P]); };
    return run<pair<BigInt, BigInt>, pair<cpp_int, cpp_int>>(c, cap, fa, fb);
}

Res run_parse(const Case &c, const Data &d, size_t cap) {
    auto fa = [&](size_t i) { return BigInt(d.sa[i % P]); };
    auto fb = [&](size_t i) { return cpp_int(d.sa[i % P]); };
    return run<BigInt, cpp_int>(c, cap, fa, fb);
}

Res run_str(const Case &c, const Data &d, size_t cap) {
    auto fa = [&](size_t i) { return dec(d.a[i % P]); };
    auto fb = [&](size_t i) { return dec(d.x[i % P]); };
    return run<string, string>(c, cap, fa, fb);
}

Res run_case(const Case &c) {
    const Data d = make_data(c);
    const size_t cap = min<size_t>(BIG_INT_CMP_REPS, max<size_t>(1, BIG_INT_CMP_MEM / out_bytes(c)));
    switch (c.op) {
        case Op::Add: return run_num<Op::Add>(c, d, cap);
        case Op::Mul: return run_num<Op::Mul>(c, d, cap);
        case Op::Sqr: return run_num<Op::Sqr>(c, d, cap);
        case Op::Div: return run_num<Op::Div>(c, d, cap);
        case Op::Mod: return run_num<Op::Mod>(c, d, cap);
        case Op::Qr: return run_qr(c, d, cap);
        case Op::Gcd: return run_num<Op::Gcd>(c, d, cap);
        case Op::Parse: return run_parse(c, d, cap);
        case Op::Str: return run_str(c, d, cap);
    }
    throw runtime_error("unknown benchmark operation");
}

void add_cases(vector<Case> &cs) {
    for (size_t n : {64, 1024, 16384, 65536}) cs.push_back({"add", Op::Add, n, n});
    for (size_t n : {64, 256, 752, 1024, 4096, 16384, 32768, 65536}) {
        cs.push_back({"mul", Op::Mul, n, n});
        cs.push_back({"sqr", Op::Sqr, n, 0});
    }
    cs.push_back({"mul-asym", Op::Mul, 1024, 4096});
    cs.push_back({"mul-asym", Op::Mul, 4096, 65536});
    const vector<pair<size_t, size_t>> ds = {
        {512, 8}, {512, 512}, {576, 512}, {1024, 512}, {8192, 4096}, {32768, 4096},
    };
    for (auto [n, m] : ds) {
        cs.push_back({"div", Op::Div, n, m});
        cs.push_back({"mod", Op::Mod, n, m});
        cs.push_back({"divmod", Op::Qr, n, m});
    }
    for (size_t n : {32, 256, 2048, 8192}) cs.push_back({"gcd-rand", Op::Gcd, n, n});
    for (size_t n : {8, 128, 2048, 32768}) {
        cs.push_back({"parse", Op::Parse, n, 0});
        cs.push_back({"str", Op::Str, n, 0});
    }
}

void check_signed_div() {
    for (int a : {-7, 7}) {
        for (int b : {-3, 3}) {
            const auto [q, r] = titan23::divmod(BigInt(a), BigInt(b));
            cpp_int x = a, y = b, zq, zr;
            boost::multiprecision::divide_qr(x, y, zq, zr);
            if (q.to_string() != zq.str() || r.to_string() != zr.str()) throw runtime_error("signed div mismatch");
        }
    }
}

int main(int argc, char **argv) {
    check_signed_div();
    vector<Case> cs;
    add_cases(cs);
    if (argc > 1) {
        const string_view f = argv[1];
        erase_if(cs, [&](const Case &c) { return c.tag != f; });
        if (cs.empty()) throw invalid_argument("unknown operation filter");
    }
    cout << "Boost " << BOOST_LIB_VERSION << ", ";
#ifdef __clang__
    cout << "Clang " << __clang_major__ << '.' << __clang_minor__ << '.' << __clang_patchlevel__;
#else
    cout << "GCC " << __GNUC__ << '.' << __GNUC_MINOR__ << '.' << __GNUC_PATCHLEVEL__;
#endif
    cout << ", rounds " << BIG_INT_CMP_ROUNDS << ", ratio = Boost / BigInt (>1: BigInt faster)\n";
    cout << left << setw(10) << "op" << right << setw(11) << "lhs-digit" << setw(11) << "rhs-digit";
    cout << setw(9) << "reps";
    cout << setw(14) << "BigInt-us" << setw(14) << "Boost-us" << setw(11) << "ratio" << '\n';
    u64 h = 0;
    for (const Case &c : cs) {
        const Res r = run_case(c);
        const double ratio = r.us_b / r.us_a;
        h ^= r.hash + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        cout << left << setw(10) << c.tag << right << setw(11) << c.n;
        if (c.m == 0) {
            cout << setw(11) << '-';
        } else {
            cout << setw(11) << c.m;
        }
        cout << setw(9) << r.reps << setw(14) << fixed << setprecision(3) << r.us_a;
        cout << setw(14) << r.us_b << setw(11) << setprecision(2) << ratio << '\n';
    }
    cout << "checksum " << hex << h << dec << '\n';
}
