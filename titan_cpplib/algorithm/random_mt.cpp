#pragma once

#include <cassert>
#include <vector>
#include <unordered_set>
#include <random>
#include <algorithm>
#include <numeric>

using namespace std;

// RandomMT
namespace titan23 {

class RandomMT {
private:
    std::mt19937 _mt;

public:
    RandomMT() : _mt(std::random_device{}()) {}

    RandomMT(unsigned int seed) : _mt(seed) {}

    //! `[0.0, 1.0]` の乱数を返す(実数)
    double random() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(_mt);
    }

    //! `[0, end]` の乱数を返す
    int randint(const int end) {
        assert(0 <= end);
        std::uniform_int_distribution<int> dist(0, end);
        return dist(_mt);
    }

    //! `[begin, end]` の乱数を返す
    int randint(const int begin, const int end) {
        assert(begin <= end);
        std::uniform_int_distribution<int> dist(begin, end);
        return dist(_mt);
    }

    //! `[begin, end]` の乱数を返す
    ll randll(const ll begin, const ll end) {
        assert(begin <= end);
        std::uniform_int_distribution<ll> dist(begin, end);
        return dist(_mt);
    }

    //! `[0, end)` の乱数を返す
    int randrange(const int end) {
        assert(0 < end);
        std::uniform_int_distribution<int> dist(0, end - 1);
        return dist(_mt);
    }

    //! `[begin, end)` の乱数を返す
    int randrange(const int begin, const int end) {
        assert(begin < end);
        std::uniform_int_distribution<int> dist(begin, end - 1);
        return dist(_mt);
    }

    //! `[0, u64_MAX)` の乱数を返す
    unsigned long long rand_u64() {
        std::uniform_int_distribution<unsigned long long> dist;
        return dist(_mt);
    }

    //! `[0, end)` の異なる乱数を2つ返す
    pair<int, int> rand_pair(const int end) {
        assert(end >= 2);
        int u = randrange(end);
        int v = u + randrange(1, end);
        if (v >= end) v -= end;
        if (u > v) std::swap(u, v);
        return {u, v};
    }

    //! `[begin, end)` の異なる乱数を2つ返す
    pair<int, int> rand_pair(const int begin, const int end) {
        assert(end - begin >= 2);
        const int len = end - begin;
        int u = randrange(begin, end);
        int v = u + randrange(1, len);
        if (v >= end) v -= len;
        if (u > v) std::swap(u, v);
        return {u, v};
    }

    //! Note `a`は非const。aの先頭cnt要素をa全体からのランダムな要素で置き換え、その部分ベクターを返す
    vector<int> rand_vec(const int cnt, vector<int> &a) {
        int n = a.size();
        assert(cnt <= n);
        for (int i = 0; i < cnt; ++i) {
            int j = randrange(i, n);
            std::swap(a[i], a[j]);
        }
        return vector<int>(a.begin(), a.begin() + cnt);
    }

    //! `[begin, end)` の乱数を返す(実数)
    double randdouble(const double begin, const double end) {
        assert(begin < end);
        std::uniform_real_distribution<double> dist(begin, end);
        return dist(_mt);
    }

    //! `vector` をインプレースにシャッフルする / `O(n)`
    template <typename T>
    void shuffle(vector<T> &a) {
        std::shuffle(a.begin(), a.end(), _mt);
    }

    //! ベクターaから重複なしでk個の要素をランダムに選んで返す
    template <typename T>
    vector<T> choices(const vector<T> &a, const int k) {
        assert(a.size() >= k);
        vector<int> indices(a.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), _mt);
        vector<T> result(k);
        for (int i = 0; i < k; ++i) {
            result[i] = a[indices[i]];
        }
        return result;
    }

    //! ベクターからランダムに1要素を返す
    template <typename T>
    T choice(const vector<T> &a) {
        assert(!a.empty());
        return a[randrange(a.size())];
    }

    //! 重み付きでベクターからランダムに1要素を返す(重みは非正規化)
    template <typename T>
    T choice(const vector<T> &a, const vector<int> &w, bool normal) {
        assert(normal == false);
        assert(a.size() == w.size());

        double sum = std::accumulate(w.begin(), w.end(), 0.0);
        assert(sum > 0);

        vector<double> cw(w.size());
        cw[0] = static_cast<double>(w[0]) / sum;
        for (size_t i = 1; i < w.size(); ++i) {
            cw[i] = cw[i-1] + static_cast<double>(w[i]) / sum;
        }
        if (!cw.empty()) cw.back() = 1.0;

        return choice(a, cw);
    }

    //! 重み付きでベクターからランダムに1要素を返す(wは累積確率分布)
    template <typename T>
    T choice(const vector<T> &a, const vector<double> &w) {
        assert(!a.empty() && a.size() == w.size());
        double val = random();
        auto it = std::lower_bound(w.begin(), w.end(), val);
        size_t idx = std::distance(w.begin(), it);
        if (idx >= a.size()) idx = a.size() - 1;
        return a[idx];
    }
};

} // namespace titan23
