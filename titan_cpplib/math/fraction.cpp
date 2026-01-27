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
};
} // namespace titan23
