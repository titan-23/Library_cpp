#pragma once
#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

template<typename T>
class Fraction {
public:
    T p, q;

    Fraction(T p_ = 0, T q_ = 1) : p(p_), q(q_) {
        if (q == 0) {
            if (p > 0) { p = 1; q = 0; }
            else if (p < 0) { p = -1; q = 0; }
            else { p = 0; q = 0; }
        } else {
            if (q < 0) { p = -p; q = -q; }
            simplify();
        }
    }

    constexpr T internal_gcd(T a, T b) {
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        while (b != 0) {
            T temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    void simplify() {
        if (p == 0) {
            q = 1;
        } else {
            T g = internal_gcd(p, q);
            p /= g;
            q /= g;
        }
    }

    Fraction operator-() const { return Fraction(-p, q); }
    Fraction abs() const { return Fraction(p < 0 ? -p : p, q); }
    Fraction operator+(const Fraction& other) const { return Fraction(p * other.q + other.p * q, q * other.q); }
    Fraction operator-(const Fraction& other) const { return Fraction(p * other.q - other.p * q, q * other.q); }
    Fraction operator*(const Fraction& other) const { return Fraction(p * other.p, q * other.q); }
    Fraction operator/(const Fraction& other) const { return Fraction(p * other.q, q * other.p); }
    Fraction& operator+=(const Fraction& other) { *this = *this + other; return *this; }
    Fraction& operator-=(const Fraction& other) { *this = *this - other; return *this; }
    Fraction& operator*=(const Fraction& other) { *this = *this * other; return *this; }
    Fraction& operator/=(const Fraction& other) { *this = *this / other; return *this; }
    bool operator<(const Fraction& other) const { return p * other.q < other.p * q; }
    bool operator>(const Fraction& other) const { return other < *this; }
    bool operator<=(const Fraction& other) const { return !(*this > other); }
    bool operator>=(const Fraction& other) const { return !(*this < other); }
    bool operator==(const Fraction& other) const { return p == other.p && q == other.q; }
    bool operator!=(const Fraction& other) const { return !(*this == other); }
    explicit operator double() const { return (double)p / q; }
    explicit operator long double() const { return (long double)p / q; }
    friend ostream& operator<<(ostream& os, const Fraction& f) {
        os << f.p << "/" << f.q;
        return os;
    }
    string to_string() const { return to_string(p) + "/" + to_string(q); }
    Fraction inv() const { return Fraction(q, p); }
    Fraction pow(long long k) const {
        if (k < 0) return inv().pow(-k);
        Fraction res(1, 1), base = *this;
        while (k > 0) {
            if (k & 1) res *= base;
            base *= base;
            k >>= 1;
        }
        return res;
    }

    friend Fraction operator+(const Fraction& f, T v) { return Fraction(f.p + v * f.q, f.q); }
    friend Fraction operator+(T v, const Fraction& f) { return Fraction(v * f.q + f.p, f.q); }
    friend Fraction operator-(const Fraction& f, T v) { return Fraction(f.p - v * f.q, f.q); }
    friend Fraction operator-(T v, const Fraction& f) { return Fraction(v * f.q - f.p, f.q); }
    friend Fraction operator*(const Fraction& f, T v) { return Fraction(f.p * v, f.q); }
    friend Fraction operator*(T v, const Fraction& f) { return Fraction(v * f.p, f.q); }
    friend Fraction operator/(const Fraction& f, T v) { return Fraction(f.p, f.q * v); }
    friend Fraction operator/(T v, const Fraction& f) { return Fraction(v * f.q, f.p); }
    Fraction& operator+=(T v) { *this = *this + v; return *this; }
    Fraction& operator-=(T v) { *this = *this - v; return *this; }
    Fraction& operator*=(T v) { *this = *this * v; return *this; }
    Fraction& operator/=(T v) { *this = *this / v; return *this; }
    size_t hash() const {
        std::hash<T> hasher;
        size_t seed = 0;
        size_t h_p = hasher(p);
        seed ^= h_p + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        size_t h_q = hasher(q);
        seed ^= h_q + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace titan23

template<typename T>
struct hash<titan23::Fraction<T>> {
    size_t operator()(const titan23::Fraction<T>& f) const {
        return f.hash();
    }
};
