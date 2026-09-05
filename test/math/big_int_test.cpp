#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef TITAN23_TEST_HAS_BOOST_CPP_INT
#if __has_include(<boost/multiprecision/cpp_int.hpp>)
#define TITAN23_TEST_HAS_BOOST_CPP_INT 1
#else
#define TITAN23_TEST_HAS_BOOST_CPP_INT 0
#endif
#endif

#if TITAN23_TEST_HAS_BOOST_CPP_INT
#include <boost/multiprecision/cpp_int.hpp>
#endif

#include "titan_cpplib/math/big_int.cpp"

using titan23::BigInt;
using namespace std;

namespace {

BigInt bigint(string_view value) { return BigInt(value); }

template <class T>
concept PowArg = requires(BigInt a, T n) {
    titan23::pow(a, n);
};

static_assert(PowArg<int>);
static_assert(PowArg<BigInt>);
static_assert(!PowArg<bool>);
static_assert(!PowArg<double>);

void expect_string(const BigInt &value, string_view expected) { assert(value.to_string() == expected); }

template <class Exception, class Function> void expect_exception(Function &&function) {
    bool thrown = false;
    try {
        function();
    } catch (const Exception &) {
        thrown = true;
    }
    assert(thrown);
}

string to_string_i128(__int128 value) {
    if (value == 0) return "0";
    bool negative = value < 0;
    unsigned __int128 magnitude;
    if (negative) {
        magnitude = static_cast<unsigned __int128>(-(value + 1));
        ++magnitude;
    } else {
        magnitude = static_cast<unsigned __int128>(value);
    }
    string result;
    while (magnitude != 0) {
        result.push_back(static_cast<char>('0' + magnitude % 10));
        magnitude /= 10;
    }
    if (negative) result.push_back('-');
    reverse(result.begin(), result.end());
    return result;
}

string power_of_ten(int n) { return "1" + string(n, '0'); }

string power_of_ten_plus_one(int n) {
    assert(n >= 1);
    return "1" + string(n - 1, '0') + "1";
}

string all_nines(int digits) {
    assert(digits >= 1);
    return string(digits, '9');
}

// (10^a - 1) (10^b - 1), written without using BigInt itself.
string nines_product(int a, int b) {
    if (a > b) swap(a, b);
    assert(a >= 1);
    return string(a - 1, '9') + "8" + string(b - a, '9') + string(a - 1, '0') + "1";
}

string decimal_schoolbook_product(string_view a, string_view b) {
    bool negative_a = !a.empty() && a.front() == '-';
    bool negative_b = !b.empty() && b.front() == '-';
    if (negative_a || (!a.empty() && a.front() == '+')) a.remove_prefix(1);
    if (negative_b || (!b.empty() && b.front() == '+')) b.remove_prefix(1);
    vector<int> product(a.size() + b.size());
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
        for (int j = static_cast<int>(b.size()) - 1; j >= 0; --j) {
            product[i + j + 1] += (a[i] - '0') * (b[j] - '0');
        }
    }
    for (int i = static_cast<int>(product.size()) - 1; i > 0; --i) {
        product[i - 1] += product[i] / 10;
        product[i] %= 10;
    }
    size_t first = 0;
    while (first + 1 < product.size() && product[first] == 0) ++first;
    string result;
    if (negative_a != negative_b && product[first] != 0) result.push_back('-');
    for (; first < product.size(); ++first) {
        result.push_back(static_cast<char>('0' + product[first]));
    }
    return result;
}

void test_construction_and_formatting() {
    BigInt zero;
    expect_string(zero, "0");
    assert(zero.sign() == 0);
    assert(zero.is_zero());
    expect_string(zero.abs(), "0");

    const vector<pair<string, string>> cases = {
        {"0", "0"},
        {"-0", "0"},
        {"+0", "0"},
        {"000000000000", "0"},
        {"-000000000000", "0"},
        {"1", "1"},
        {"+1", "1"},
        {"000001", "1"},
        {"-000001", "-1"},
        {"99999999", "99999999"},
        {"100000000", "100000000"},
        {"100000001", "100000001"},
        {"-123456789012345678901234567890", "-123456789012345678901234567890"},
    };
    for (const auto &[input, expected] : cases) {
        BigInt value(input);
        expect_string(value, expected);
        ostringstream output;
        output << value;
        assert(output.str() == expected);
    }

    assert(bigint("123").sign() == 1);
    assert(bigint("-123").sign() == -1);
    assert(!bigint("123").is_zero());
    expect_string(bigint("-123").abs(), "123");
    expect_string(-bigint("123"), "-123");
    expect_string(-bigint("-123"), "123");
    expect_string(-bigint("0"), "0");

    expect_string(BigInt(numeric_limits<int>::min()), to_string(numeric_limits<int>::min()));
    expect_string(BigInt(numeric_limits<int>::max()), to_string(numeric_limits<int>::max()));
    expect_string(BigInt(numeric_limits<long long>::min()), to_string(numeric_limits<long long>::min()));
    expect_string(BigInt(numeric_limits<long long>::max()), to_string(numeric_limits<long long>::max()));
    expect_string(BigInt(numeric_limits<unsigned long long>::max()),
                  to_string(numeric_limits<unsigned long long>::max()));

    for (string input : {"", "+", "-", "12a3", " 1", "1 ", "++1", "--1", "+-1", "1_000"}) {
        expect_exception<invalid_argument>([&] { (void)BigInt(input); });
    }
}

void test_streams() {
    istringstream input("00012 -0034 +56 0 999999999999999999999999999999");
    BigInt a, b, c, d, e;
    input >> a >> b >> c >> d >> e;
    assert(input);
    expect_string(a, "12");
    expect_string(b, "-34");
    expect_string(c, "56");
    expect_string(d, "0");
    expect_string(e, "999999999999999999999999999999");

    ostringstream output;
    output << a << ' ' << b << ' ' << c << ' ' << d << ' ' << e;
    assert(output.str() == "12 -34 56 0 999999999999999999999999999999");
}

template <class T> void test_ext_int() {
    if constexpr (integral<T>) {
        T x = (T(1) << 100) + 123456789;
        assert(BigInt(x).template to_integral<T>() == x);
        assert(BigInt(-x).template to_integral<T>() == -x);
        T lo = numeric_limits<T>::min();
        assert(BigInt(lo).template to_integral<T>() == lo);
        using U = make_unsigned_t<T>;
        U u = numeric_limits<U>::max();
        assert(BigInt(u).template to_integral<U>() == u);
        expect_string(titan23::pow(BigInt(0), T(1) << 100), "0");
    }
}

void test_move() {
    BigInt a("12345678901234567890");
    BigInt b(move(a));
    expect_string(a, "0");
    expect_string(b, "12345678901234567890");
    assert(a == BigInt(0));

    BigInt c(-7);
    c = move(b);
    expect_string(b, "0");
    expect_string(c, "12345678901234567890");
    assert(b == BigInt(0));

    c = move(c);
    expect_string(c, "12345678901234567890");
}

void test_integral_conversion() {
    static_assert(same_as<decltype(BigInt(0).to_integral<const uint64_t>()), uint64_t>);
    static_assert(same_as<decltype(BigInt(0).to_integral<volatile int>()), int>);
    assert(BigInt(0).to_integral<int>() == 0);
    assert(BigInt(42).to_integral<const uint64_t>() == 42);
    assert(BigInt(-128).to_integral<int8_t>() == numeric_limits<int8_t>::min());
    assert(BigInt(127).to_integral<int8_t>() == numeric_limits<int8_t>::max());
    assert(BigInt(255).to_integral<uint8_t>() == numeric_limits<uint8_t>::max());
    assert(BigInt(numeric_limits<long long>::min()).to_integral<long long>() == numeric_limits<long long>::min());
    assert(BigInt(numeric_limits<long long>::max()).to_integral<long long>() == numeric_limits<long long>::max());
    assert(BigInt(numeric_limits<unsigned long long>::max()).to_integral<unsigned long long>() ==
           numeric_limits<unsigned long long>::max());
    expect_exception<overflow_error>([] { (void)BigInt(-1).to_integral<unsigned int>(); });
    expect_exception<overflow_error>([] { (void)BigInt(-129).to_integral<int8_t>(); });
    expect_exception<overflow_error>([] { (void)BigInt(128).to_integral<int8_t>(); });
    expect_exception<overflow_error>([] { (void)BigInt(256).to_integral<uint8_t>(); });
    expect_exception<overflow_error>([] { (void)BigInt("18446744073709551616").to_integral<unsigned long long>(); });

    test_ext_int<__int128>();
}

void check_small_pair(long long a, long long b) {
    BigInt x(a), y(b);
    __int128 aa = a;
    __int128 bb = b;
    expect_string(x + y, to_string_i128(aa + bb));
    expect_string(x - y, to_string_i128(aa - bb));
    expect_string(x * y, to_string_i128(aa * bb));

    assert((x == y) == (a == b));
    assert((x != y) == (a != b));
    assert((x < y) == (a < b));
    assert((x <= y) == (a <= b));
    assert((x > y) == (a > b));
    assert((x >= y) == (a >= b));

    BigInt compound = x;
    compound += y;
    expect_string(compound, to_string_i128(aa + bb));
    compound = x;
    compound -= y;
    expect_string(compound, to_string_i128(aa - bb));
    compound = x;
    compound *= y;
    expect_string(compound, to_string_i128(aa * bb));

    if (b != 0) {
        __int128 quotient = aa / bb;
        __int128 remainder = aa % bb;
        expect_string(x / y, to_string_i128(quotient));
        expect_string(x % y, to_string_i128(remainder));
        auto [q, r] = titan23::divmod(x, y);
        expect_string(q, to_string_i128(quotient));
        expect_string(r, to_string_i128(remainder));
        assert(y * q + r == x);

        compound = x;
        compound /= y;
        expect_string(compound, to_string_i128(quotient));
        compound = x;
        compound %= y;
        expect_string(compound, to_string_i128(remainder));
    }
}

void test_small_arithmetic() {
    const vector<long long> edge_values = {
        numeric_limits<long long>::min(),
        numeric_limits<long long>::min() + 1,
        -100000001,
        -100000000,
        -99999999,
        -2,
        -1,
        0,
        1,
        2,
        99999999,
        100000000,
        100000001,
        numeric_limits<long long>::max() - 1,
        numeric_limits<long long>::max(),
    };
    for (long long a : edge_values) {
        for (long long b : edge_values) check_small_pair(a, b);
    }

    mt19937_64 random(0x23b19a74d6ULL);
    uniform_int_distribution<long long> distribution(numeric_limits<long long>::min(),
                                                     numeric_limits<long long>::max());
    for (int iteration = 0; iteration < 5000; ++iteration) {
        check_small_pair(distribution(random), distribution(random));
    }
}

void test_increment_and_predicates() {
    BigInt a(0);
    assert(!a.is_one() && !a.is_neg_one() && !a.is_odd());
    expect_string(++a, "1");
    assert(a.is_one() && a.is_odd());
    expect_string(--a, "0");
    expect_string(--a, "-1");
    assert(a.is_neg_one() && a.is_odd());
    expect_string(++a, "0");

    a = BigInt("99999999");
    BigInt old = a++;
    expect_string(old, "99999999");
    expect_string(a, "100000000");
    old = a--;
    expect_string(old, "100000000");
    expect_string(a, "99999999");

    a = BigInt("-99999999");
    old = a--;
    expect_string(old, "-99999999");
    expect_string(a, "-100000000");
    old = a++;
    expect_string(old, "-100000000");
    expect_string(a, "-99999999");

    const string nines(128, '9');
    a = BigInt(nines);
    expect_string(++a, "1" + string(128, '0'));
    expect_string(--a, nines);
    a = -a;
    expect_string(--a, "-1" + string(128, '0'));
    expect_string(++a, "-" + nines);
}

void test_large_known_values() {
    const vector<int> digit_counts = {
        1,   7,   8,   9,   15,  16,  17,  31,  32,  33,   63,   64,   65,   127,  128,   129,   255,
        256, 257, 280, 288, 511, 512, 513, 600, 608, 1024, 2048, 4096, 8192, 8193, 10240, 16384,
    };

    for (int digits : digit_counts) {
        string nines = all_nines(digits);
        BigInt a(nines);
        BigInt one(1);
        expect_string(a + one, power_of_ten(digits));
        expect_string((a + one) - one, nines);
        assert(a + (-a) == BigInt(0));
        expect_string(a * a, nines_product(digits, digits));

        BigInt square(nines_product(digits, digits));
        auto [quotient, remainder] = titan23::divmod(square, a);
        assert(quotient == a);
        assert(remainder.is_zero());
        assert(square / a == a);
        assert((square % a).is_zero());

        expect_string(-a * a, "-" + nines_product(digits, digits));
        expect_string(a * -a, "-" + nines_product(digits, digits));
        expect_string(-a * -a, nines_product(digits, digits));
    }

    for (auto [short_digits, long_digits] : vector<pair<int, int>>{{1, 4096},
                                                                   {7, 2048},
                                                                   {8, 4096},
                                                                   {17, 3073},
                                                                   {129, 4096},
                                                                   {513, 2048},
                                                                   {1024, 3072},
                                                                   {2048, 6144},
                                                                   {8200, 8300},
                                                                   {16383, 16384},
                                                                   {65535, 65536},
                                                                   {65536, 65537}}) {
        BigInt a(all_nines(short_digits));
        BigInt b(all_nines(long_digits));
        string expected = nines_product(short_digits, long_digits);
        expect_string(a * b, expected);
        expect_string(b * a, expected);
    }

    for (int digits : {32760, 32768, 32769, 65536, 65537}) {
        const string s = all_nines(digits);
        expect_string(BigInt(s) * BigInt(s), nines_product(digits, digits));
    }

    for (int digits : {3, 8, 64, 257, 1024, 4096}) {
        BigInt divisor(all_nines(digits));
        BigInt dividend("1" + string(2 * digits - 3, '0') + "123");
        BigInt expected_quotient(power_of_ten_plus_one(digits));

        for (int dividend_sign : {-1, 1}) {
            for (int divisor_sign : {-1, 1}) {
                BigInt signed_dividend = dividend_sign < 0 ? -dividend : dividend;
                BigInt signed_divisor = divisor_sign < 0 ? -divisor : divisor;
                BigInt q = dividend_sign == divisor_sign ? expected_quotient : -expected_quotient;
                BigInt r = dividend_sign < 0 ? BigInt(-124) : BigInt(124);
                auto [actual_q, actual_r] = titan23::divmod(signed_dividend, signed_divisor);
                assert(actual_q == q);
                assert(actual_r == r);
                assert(signed_dividend / signed_divisor == q);
                assert(signed_dividend % signed_divisor == r);
                assert(signed_divisor * actual_q + actual_r == signed_dividend);
                assert(actual_r.abs() < signed_divisor.abs());
            }
        }
    }

    for (int digits : {1, 8, 65, 512, 4096}) {
        BigInt a(all_nines(digits));
        expect_string(a / BigInt(3), string(digits, '3'));
        assert((a % BigInt(3)).is_zero());
    }
}

string random_integer_string(mt19937_64 &random, int digits, bool may_be_negative = true) {
    assert(digits >= 1);
    string result;
    if (may_be_negative && (random() & 1)) result.push_back('-');
    result.push_back(static_cast<char>('1' + random() % 9));
    for (int i = 1; i < digits; ++i) {
        result.push_back(static_cast<char>('0' + random() % 10));
    }
    return result;
}

void test_random_multiplication_against_decimal_schoolbook() {
    mt19937_64 random(0x2f169c84b7ULL);
    const vector<pair<int, int>> sizes = {
        {1, 4096},  {7, 2048},    {31, 4096},   {127, 129},   {255, 257},
        {511, 513}, {1023, 1025}, {2048, 2048}, {3072, 4096}, {8201, 8303},
    };
    for (auto [a_digits, b_digits] : sizes) {
        string a_text = random_integer_string(random, a_digits);
        string b_text = random_integer_string(random, b_digits);
        string expected = decimal_schoolbook_product(a_text, b_text);
        BigInt a(a_text), b(b_text);
        expect_string(a * b, expected);
        BigInt compound = a;
        compound *= b;
        expect_string(compound, expected);

        BigInt product(expected);
        assert(product / a == b);
        assert((product % a).is_zero());
        assert(product / b == a);
        assert((product % b).is_zero());
    }
}

void test_large_algebraic_properties() {
    mt19937_64 random(0xca7f8d3251ULL);
    for (int iteration = 0; iteration < 100; ++iteration) {
        int a_digits = 1 + static_cast<int>(random() % 1200);
        int b_digits = 1 + static_cast<int>(random() % 1200);
        BigInt a(random_integer_string(random, a_digits));
        BigInt b(random_integer_string(random, b_digits));
        assert((a + b) - b == a);
        assert((a - b) + b == a);
        assert(a * b == b * a);
        assert(a * (b + BigInt(1)) == a * b + a);

        auto [q, r] = titan23::divmod(a, b);
        assert(b * q + r == a);
        assert(r.abs() < b.abs());
        assert(r.is_zero() || r.sign() == a.sign());
        assert(q == a / b);
        assert(r == a % b);
    }
}

string random_chunk_integer(mt19937_64 &random, int limbs, uint32_t top_limb) {
    assert(limbs >= 1);
    assert(1 <= top_limb && top_limb < 100000000);
    ostringstream output;
    output << top_limb;
    for (int i = 1; i < limbs; ++i) {
        output << setw(8) << setfill('0') << static_cast<uint32_t>(random() % 100000000);
    }
    return output.str();
}

void check_constructed_division(const BigInt &divisor, const BigInt &quotient, const BigInt &remainder) {
    assert(!divisor.is_zero());
    assert(remainder.sign() >= 0);
    assert(remainder < divisor.abs());
    BigInt dividend = divisor.abs() * quotient.abs() + remainder;
    for (int dividend_sign : {-1, 1}) {
        for (int divisor_sign : {-1, 1}) {
            BigInt signed_dividend = dividend_sign < 0 ? -dividend : dividend;
            BigInt signed_divisor = divisor_sign < 0 ? -divisor.abs() : divisor.abs();
            BigInt expected_q = dividend_sign == divisor_sign ? quotient.abs() : -quotient.abs();
            BigInt expected_r = dividend_sign < 0 ? -remainder : remainder;
            auto [q, r] = titan23::divmod(signed_dividend, signed_divisor);
            assert(q == expected_q);
            assert(r == expected_r);
            assert(q == signed_dividend / signed_divisor);
            assert(r == signed_dividend % signed_divisor);
            assert(signed_divisor * q + r == signed_dividend);
            assert(r.abs() < signed_divisor.abs());
            assert(r.is_zero() || r.sign() == signed_dividend.sign());
        }
    }
}

void test_division_boundaries_and_aliasing() {
    mt19937_64 random(0x713bf9842dULL);

    // Knuth D needs its add-back step here after the initial quotient estimate.
    BigInt add_back_dividend("1000000000000000000000000");
    BigInt add_back_divisor("500000000000000000000001");
    auto [add_back_q, add_back_r] = titan23::divmod(add_back_dividend, add_back_divisor);
    expect_string(add_back_q, "1");
    expect_string(add_back_r, "499999999999999999999999");
    assert(add_back_divisor * add_back_q + add_back_r == add_back_dividend);

    // The raw quotient estimate is base + 1 and must be capped/corrected.
    BigInt qhat_cap_dividend("1000000019999999700000000");
    BigInt qhat_cap_divisor("5000000099999999");
    auto [qhat_cap_q, qhat_cap_r] = titan23::divmod(qhat_cap_dividend, qhat_cap_divisor);
    expect_string(qhat_cap_q, "199999999");
    expect_string(qhat_cap_r, "4999999999999999");
    assert(qhat_cap_divisor * qhat_cap_q + qhat_cap_r == qhat_cap_dividend);

    const vector<uint32_t> normalized_top_limbs = {
        1, 2, 49999999, 50000000, 50000001, 99999998, 99999999,
    };
    const vector<int> limb_counts = {
        2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256,
    };

    for (int iteration = 0; iteration < 160; ++iteration) {
        int divisor_limbs = limb_counts[random() % limb_counts.size()];
        int quotient_limbs;
        switch (iteration % 5) {
        case 0:
            quotient_limbs = 1;
            break;
        case 1:
            quotient_limbs = max(1, divisor_limbs - 1);
            break;
        case 2:
            quotient_limbs = divisor_limbs;
            break;
        case 3:
            quotient_limbs = divisor_limbs + 1;
            break;
        default:
            quotient_limbs = 2 * divisor_limbs;
            break;
        }
        uint32_t divisor_top = normalized_top_limbs[iteration % normalized_top_limbs.size()];
        uint32_t quotient_top = 1 + static_cast<uint32_t>(random() % 99999999);
        BigInt divisor(random_chunk_integer(random, divisor_limbs, divisor_top));
        BigInt quotient(random_chunk_integer(random, quotient_limbs, quotient_top));

        BigInt remainder;
        switch (iteration % 5) {
        case 0:
            remainder = BigInt(0);
            break;
        case 1:
            remainder = BigInt(1);
            break;
        case 2:
            remainder = divisor - BigInt(1);
            break;
        case 3:
            if (divisor_top == 1) {
                remainder = divisor / BigInt(2);
            } else {
                uint32_t remainder_top = 1 + static_cast<uint32_t>(random() % (divisor_top - 1));
                remainder = BigInt(random_chunk_integer(random, divisor_limbs, remainder_top));
            }
            break;
        default:
            remainder = BigInt(static_cast<unsigned long long>(random() % 100000000));
            break;
        }
        check_constructed_division(divisor, quotient, remainder);
    }

    for (auto [n, k] : vector<pair<int, int>>{{96, 64}, {128, 48}, {192, 32}, {384, 16}, {768, 8}}) {
        BigInt b(random_chunk_integer(random, n, 50000000));
        BigInt q(random_chunk_integer(random, k, 12345678));
        check_constructed_division(b, q, b - 1);
    }

    BigInt value(random_chunk_integer(random, 260, 50000000));
    BigInt copy = value;
    copy /= copy;
    assert(copy == BigInt(1));
    copy = value;
    copy %= copy;
    assert(copy.is_zero());
    copy = value;
    copy += copy;
    assert(copy == value * BigInt(2));
    copy = value;
    copy -= copy;
    assert(copy.is_zero());
    copy = value;
    copy *= copy;
    assert(copy == value * value);
}

void test_number_theory_helpers() {
    expect_string(titan23::pow(BigInt(0), uint64_t(0)), "1");
    expect_string(titan23::pow(BigInt(0), uint64_t(17)), "0");
    expect_string(titan23::pow(BigInt(-12), uint64_t(5)), "-248832");
    expect_string(titan23::pow(BigInt(-12), uint64_t(6)), "2985984");
    expect_string(titan23::pow(BigInt(10), uint64_t(4096)), power_of_ten(4096));
    expect_string(titan23::pow<const unsigned int>(BigInt(2), 3), "8");
    expect_string(titan23::pow<const int>(BigInt(2), 3), "8");
    expect_exception<domain_error>([] { (void)titan23::pow(BigInt(2), -1); });

    expect_string(titan23::pow(BigInt(0), BigInt(0)), "1");
    expect_string(titan23::pow(BigInt(0), BigInt(17)), "0");
    expect_string(titan23::pow(BigInt(-12), BigInt(5)), "-248832");
    expect_string(titan23::pow(BigInt(-12), BigInt(6)), "2985984");
    expect_string(titan23::pow(BigInt(10), BigInt(4096)), power_of_ten(4096));

    BigInt huge_even(power_of_ten(100));
    BigInt huge_odd = huge_even + BigInt(1);
    expect_string(titan23::pow(BigInt(0), huge_even), "0");
    expect_string(titan23::pow(BigInt(1), huge_even), "1");
    expect_string(titan23::pow(BigInt(1), huge_odd), "1");
    expect_string(titan23::pow(BigInt(-1), huge_even), "1");
    expect_string(titan23::pow(BigInt(-1), huge_odd), "-1");

    expect_exception<domain_error>([] { (void)titan23::pow(BigInt(2), BigInt(-1)); });
    expect_exception<domain_error>([] { (void)titan23::pow(BigInt(0), BigInt(-7)); });
    expect_exception<domain_error>([&] { (void)titan23::pow(BigInt(-1), -huge_odd); });

    expect_string(titan23::gcd(BigInt(0), BigInt(0)), "0");
    expect_string(titan23::gcd(BigInt(0), BigInt(-42)), "42");
    expect_string(titan23::gcd(BigInt(-48), BigInt(18)), "6");
    expect_string(titan23::gcd(6, 9), "3");
    expect_string(gcd(BigInt(-48), 18), "6");
    expect_string(gcd(-48, BigInt(18)), "6");
    expect_string(titan23::lcm(BigInt(0), BigInt(-42)), "0");
    expect_string(titan23::lcm(BigInt(-21), BigInt(6)), "42");
    expect_string(titan23::lcm(6, 9), "18");
    expect_string(lcm(BigInt(-21), 6), "42");
    expect_string(lcm(-21, BigInt(6)), "42");

    BigInt a(all_nines(840));
    BigInt b(all_nines(360));
    expect_string(titan23::gcd(a, b), all_nines(120));
    BigInt factor(all_nines(120));
    assert(titan23::lcm(a, factor) == a);
    assert(titan23::lcm(-a, factor) == a);
}

void test_errors() {
    BigInt value("12345678901234567890");
    BigInt zero(0);
    expect_exception<domain_error>([&] { (void)(value / zero); });
    expect_exception<domain_error>([&] { (void)(value % zero); });
    expect_exception<domain_error>([&] { (void)titan23::divmod(value, zero); });

    BigInt compound = value;
    expect_exception<domain_error>([&] { compound /= zero; });
    compound = value;
    expect_exception<domain_error>([&] { compound %= zero; });
}

#if TITAN23_TEST_HAS_BOOST_CPP_INT
using boost::multiprecision::cpp_int;

cpp_int parse_reference(string_view text) {
    bool negative = !text.empty() && text.front() == '-';
    size_t begin = negative || (!text.empty() && text.front() == '+');
    cpp_int result = 0;
    for (size_t i = begin; i < text.size(); ++i) {
        result *= 10;
        result += text[i] - '0';
    }
    if (negative) result = -result;
    return result;
}

void expect_reference(const BigInt &actual, const cpp_int &expected) { assert(actual.to_string() == expected.str()); }

void check_boost_pair(const string &a_text, const string &b_text) {
    BigInt a(a_text), b(b_text);
    cpp_int aa = parse_reference(a_text);
    cpp_int bb = parse_reference(b_text);

    expect_reference(a + b, aa + bb);
    expect_reference(a - b, aa - bb);
    expect_reference(a * b, aa * bb);
    assert((a == b) == static_cast<bool>(aa == bb));
    assert((a < b) == static_cast<bool>(aa < bb));
    assert((a <= b) == static_cast<bool>(aa <= bb));
    assert((a > b) == static_cast<bool>(aa > bb));
    assert((a >= b) == static_cast<bool>(aa >= bb));

    if (bb != 0) {
        expect_reference(a / b, aa / bb);
        expect_reference(a % b, aa % bb);
        auto [q, r] = titan23::divmod(a, b);
        expect_reference(q, aa / bb);
        expect_reference(r, aa % bb);
    }
}

void test_boost_differential() {
    mt19937_64 random(0x8d41b73a29ULL);

    for (int iteration = 0; iteration < 300; ++iteration) {
        int a_digits = 1 + static_cast<int>(random() % 400);
        int b_digits = 1 + static_cast<int>(random() % 400);
        check_boost_pair(random_integer_string(random, a_digits), random_integer_string(random, b_digits));
    }
    for (int iteration = 0; iteration < 20; ++iteration) {
        int a_digits = 800 + static_cast<int>(random() % 3201);
        int b_digits = 800 + static_cast<int>(random() % 3201);
        check_boost_pair(random_integer_string(random, a_digits), random_integer_string(random, b_digits));
    }

    const vector<int> boundaries = {
        1,   7,   8,   9,   31,  32,   33,   63,   64,   65,   127,   128,   129,   255,
        256, 257, 511, 512, 513, 1024, 2048, 4096, 8192, 8193, 10240, 12288, 16384, 16385,
    };
    for (int digits : boundaries) {
        check_boost_pair(random_integer_string(random, digits), random_integer_string(random, max(1, digits - 1)));
        check_boost_pair(random_integer_string(random, digits), random_integer_string(random, digits));
    }
}
#endif

} // namespace

int main() {
    test_construction_and_formatting();
    test_streams();
    test_move();
    test_integral_conversion();
    test_small_arithmetic();
    test_increment_and_predicates();
    test_large_known_values();
    test_random_multiplication_against_decimal_schoolbook();
    test_large_algebraic_properties();
    test_division_boundaries_and_aliasing();
    test_number_theory_helpers();
    test_errors();
#if TITAN23_TEST_HAS_BOOST_CPP_INT
    test_boost_differential();
#endif
    cout << "big_int_test: OK";
#if TITAN23_TEST_HAS_BOOST_CPP_INT
    cout << " (Boost cpp_int differential enabled)";
#endif
    cout << '\n';
}
