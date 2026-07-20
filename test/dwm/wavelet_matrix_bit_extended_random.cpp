#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/wavelet_matrix_bit.cpp"

using namespace std;

namespace {

using Key = long long;
using Matrix = titan23::WaveletMatrix<Key, 7>;

void run(const uint64_t seed) {
    mt19937_64 rng(seed);
    const int n = rng() % 200;
    vector<Key> keys(n);
    for (Key &key : keys) key = rng() % 127;
    const Matrix matrix(keys);

    static_assert(is_same_v<decltype(matrix.topk(0, 0, 0)), vector<pair<Key, int>>>);
    assert(matrix.tovector() == keys);
    assert(matrix.topk(0, n, 0).empty());

    for (int query = 0; query < 10000; ++query) {
        int l = rng() % (n + 1);
        int r = rng() % (n + 1);
        if (l > r) swap(l, r);
        Key lower = rng() % 127;
        Key upper = rng() % 127;
        if (lower > upper) swap(lower, upper);
        vector<int> positions;
        for (int i = l; i < r; ++i) {
            if (lower <= keys[i] && keys[i] < upper) positions.emplace_back(i);
        }
        assert(matrix.next_index_in_value_range(l, r, lower, upper) == (positions.empty() ? -1 : positions.front()));
        assert(matrix.prev_index_in_value_range(l, r, lower, upper) == (positions.empty() ? -1 : positions.back()));
        for (int k = 0; k < static_cast<int>(positions.size()); ++k) {
            assert(matrix.kth_index_in_value_range(l, r, lower, upper, k) == positions[k]);
        }

        if (l == r) continue;
        vector<int> count(127);
        for (int i = l; i < r; ++i) ++count[keys[i]];
        Key majority = -1;
        for (int key = 0; key < 127; ++key) {
            if (count[key] * 2 > r - l) majority = key;
        }
        const auto [found, value] = matrix.has_majority(l, r);
        assert(found == (majority != -1));
        if (found) assert(value == majority);

        const int position = l + rng() % (r - l);
        const Key key = keys[position];
        int occurrence = 0;
        for (int i = l; i < position; ++i) occurrence += keys[i] == key;
        assert(matrix.range_select(l, r, occurrence, key) == position);
    }
}

} // namespace

int main() {
    for (uint64_t seed = 0; seed < 12; ++seed) run(seed * 1000003 + 149);
    return 0;
}
