#include <vector>
#include <array>
#include <algorithm>
using namespace std;

namespace titan23 {

class BloomFilter {
private:
    using u64 = unsigned long long;
    int size;
    std::vector<u64> bits;
    int msk;
    static constexpr int K = 3; // ハッシュ関数の数

    // 最小の2のべき乗に切り上げる関数
    int next_pow2(int n) const {
        if (n == 0) return 1;
        int p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    std::array<u64, K> get_hashes(u64 key) const {
        auto splitmix64 = [&] () -> u64 {
            key += 0x9e3779b97f4a7c15;
            u64 z = key;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
            z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
            return z ^ (z >> 31);
        };
        array<u64, K> res;
        for (int i = 0; i < K; ++i) {
            res[i] = splitmix64();
        }
        return res;
    }

public:
    BloomFilter(int init_size) : size(0) {
        int nbits = next_pow2((init_size + 63) / 64);
        bits.resize(nbits, 0);
        msk = nbits - 1;
    }

    void insert(const u64 key) {
        for (const u64 &h : get_hashes(key)) {
            bits[(h >> 6) & msk] |= (1ull << (h & 63));
        }
        ++size;
    }

    bool contains(const u64 key) const {
        for (const u64 &h : get_hashes(key)) {
            if (!((bits[(h >> 6) & msk] >> (h & 63)) & 1)) {
                return false;
            }
        }
        return true; // たぶん存在する
    }

    bool contains_insert(const u64 key) {
        array<u64, K> hashes = get_hashes(key);
        bool contains = true;
        for (const u64 &h : hashes) {
            if (!((bits[(h >> 6) & msk] >> (h & 63)) & 1)) {
                contains = false;
                break;
            }
        }
        if (contains) return true;
        for (const u64 &h : hashes) {
            bits[(h >> 6) & msk] |= (1ull << (h & 63));
        }
        return false;
    }

    int inner_len() const { return (int)bits.size(); }

    int len() const { return size; }

    bool empty() const { return size == 0; }

    void clear() {
        if (empty()) return;
        size = 0;
        std::fill(bits.begin(), bits.end(), 0);
    }
};

} // namespace titan23
