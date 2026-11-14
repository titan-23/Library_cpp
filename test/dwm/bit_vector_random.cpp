#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "titan_cpplib/ds/bit_vector.cpp"

using namespace std;

namespace {

// 長さと 1 の密度を変えながら、rank / access / select を素朴な計算と比べる
void run(const uint64_t seed, const int max_n, const int iterations) {
    mt19937_64 rng(seed);
    for (int it = 0; it < iterations; ++it) {
        const int n = rng() % (max_n + 1);
        const int density = rng() % 101;
        vector<int> a(n);
        for (int &x : a) x = int(rng() % 100) < density;

        // 関数から作る版と、set と build で作る版の両方を確かめる
        titan23::BitVector from_function(n, [&](const int i) { return a[i]; });
        titan23::BitVector from_set(n);
        for (int i = 0; i < n; ++i) {
            if (a[i]) from_set.set(i);
        }
        from_set.build();

        vector<int> zeros, ones;
        for (int i = 0; i < n; ++i) (a[i] ? ones : zeros).push_back(i);
        for (const titan23::BitVector *bv : {&from_function, &from_set}) {
            assert(bv->len() == n);
            int count = 0;
            for (int r = 0; r <= n; ++r) {
                assert(bv->rank1(r) == count);
                assert(bv->rank0(r) == r - count);
                if (r < n) {
                    assert(bv->access(r) == a[r]);
                    count += a[r];
                }
            }
            for (int k = -1; k <= (int)ones.size(); ++k) {
                assert(bv->select1(k) == (0 <= k && k < (int)ones.size() ? ones[k] : -1));
            }
            for (int k = -1; k <= (int)zeros.size(); ++k) {
                assert(bv->select0(k) == (0 <= k && k < (int)zeros.size() ? zeros[k] : -1));
            }
        }
    }
}

}  // namespace

int main() {
    // 64 bit 境界と select の標本間隔 256 をまたぐ長さを多く含める
    run(1, 70, 2000);
    run(2, 600, 1000);
    run(3, 5000, 300);
    cout << "BitVector random test: OK" << endl;
    return 0;
}
