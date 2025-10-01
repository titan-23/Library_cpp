#pragma once
#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// BitVector
namespace titan23 {

class BitVector {
private:
    using u64 = unsigned long long;
    int n, bsize;
    vector<u64> bit, acc;

public:
    BitVector() {}
    BitVector(const int n) :
        n(n), bsize((n+63)>>6), bit(bsize+1, 0), acc(bsize+1, 0) {}

    void set(const int k) {
        bit[k>>6] |= (1ull) << (k & 63);
    }

    void build() {
        for (int i = 0; i < bsize; ++i) {
            acc[i+1] += acc[i] + __builtin_popcountll(bit[i]);
        }
    }

    bool access(const int k) const {
        return (bit[k>>6] >> (k&63)) & 1;
    }

    int rank0(const int r) const {
        return r - (acc[r>>6] + __builtin_popcountll(bit[r>>6] & ((1ull << (r & 63)) - 1)));
    }

    int rank1(const int r) const {
        return acc[r>>6] + __builtin_popcountll(bit[r>>6] & ((1ull << (r & 63)) - 1));
    }

    int rank(const int r, const bool v) const {
        return v ? rank1(r) : rank0(r);
    }

    int select0(int k) const {
        if (k < 0 or rank0(n) <= k) return -1;
        int l = 0, r = bsize+1;
        while (r - l > 1) {
            const int m = (l + r) >> 1;
            if (m*32 - acc[m] > k) {
                r = m;
            } else {
                l = m;
            }
        }
        int indx = 32 * l;
        k = k - (l*32 - acc[l]) + rank0(indx);
        l = indx; r = indx+32;
        while (r - l > 1) {
            const int m = (l + r) >> 1;
            if (rank0(m) > k) {
                r = m;
            } else {
                l = m;
            }
        }
        return l;
    }

    int select1(int k) const {
        if (k < 0 || rank1(n) <= k) return -1;
        int l = 0, r = bsize+1;
        while (r - l > 1) {
            const int m = (l + r) >> 1;
            if (acc[m] > k) {
                r = m;
            } else {
                l = m;
            }
        }
        int indx = 32 * l;
        k = k - acc[l] + rank1(indx);
        l = indx; r = indx+32;
        while (r - l > 1) {
            const int m = (l + r) >> 1;
            if (rank1(m) > k) {
                r = m;
            } else {
                l = m;
            }
        }
        return l;
    }

    int select(const int k, const int v) const {
        return v ? select1(k) : select0(k);
    }

    int len() const {
        return n;
    }

    void print() const {
        cout << "[";
        for (int i = 0; i < len()-1; ++i) {
            cout << access(i) << ", ";
        }
        if (len() > 0) {
            cout << access(len()-1);
        }
        cout << "]\n";
    }
};
}  // namespace titan23
