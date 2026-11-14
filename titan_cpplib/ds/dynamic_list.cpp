/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_list.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
using namespace std;

namespace titan23 {

/// @brief 任意位置に挿入と削除ができる列 / 長さ BUCKET_MAX 以下のバケットに分けて持つ
template<typename T>
class DynamicList {
    private:
    static const int BUCKET_MAX = 500;
    vector<vector<T>> data;
    int n;

    void build(const vector<T> &a) {
        long long s = len();
        int bucketn = max((int)(s+BUCKET_MAX-1)/BUCKET_MAX, (int)ceil(sqrt(s)));
        data.resize(bucketn);
        for (int i = 0; i < bucketn; ++i) {
            int start = s*i/bucketn;
            int stop = min((int)len(), (int)(s*(i+1)/bucketn));
            data[i] = vector<T>(a.begin()+start, a.begin()+stop);
        }
    }

    pair<int, int> get_bucket(int k) const {
        if (k == len()) return {-1, -1};
        if (k < len()/2) {
            for (int i = 0; i < data.size(); ++i) {
                if (k < data[i].size()) return {i, k};
                k -= data[i].size();
            }
        } else {
            int tot = len();
            for (int i = data.size()-1; i >= 0; --i) {
                if (tot-data[i].size() <= k) {
                    return {i, k-(tot-data[i].size())};
                }
                tot -= data[i].size();
            }
        }
        assert(false);
    }

public:
    DynamicList() : n(0) {}

    /// @brief 列 a から構築する
    DynamicList(const vector<T> &a) : n(a.size()) {
        build(a);
    }

    /// @brief 末尾に key を追加する
    void emplace_back(T key) {
        insert(len(), key);
    }

    /// @brief k 番目に key を挿入する
    void insert(int k, T key) {
        assert(0 <= k && k <= len());
        if (data.empty()) {
            ++n;
            data.push_back({key});
            return;
        }
        auto [bucket_pos, bit_pos] = get_bucket(k);
        if (bucket_pos == -1) {
            bucket_pos = data.size()-1;
            data.back().emplace_back(key);
        } else {
            data[bucket_pos].insert(data[bucket_pos].begin() + bit_pos, key);
        }

        if (data[bucket_pos].size() > BUCKET_MAX) {
            vector<T> right(data[bucket_pos].begin() + BUCKET_MAX/2, data[bucket_pos].end());
            data[bucket_pos].erase(data[bucket_pos].begin() + BUCKET_MAX/2, data[bucket_pos].end());
            data.emplace(data.begin() + bucket_pos+1, right);
        }
        ++n;
    }

    /// @brief k 番目の値を返す
    bool access(int k) const {
        assert(0 <= k && k < len());
        auto [bucket_pos, bit_pos] = get_bucket(k);
        return data[bucket_pos][bit_pos];
    }

    /// @brief k 番目を削除して返す
    T pop(int k) {
        assert(0 <= k && k < len());
        auto [bucket_pos, bit_pos] = get_bucket(k);
        bool res = data[bucket_pos][bit_pos];
        data[bucket_pos].erase(data[bucket_pos].begin() + bit_pos);
        --n;
        if (data[bucket_pos].empty()) {
            data.erase(data.begin() + bucket_pos);
        }
        return res;
    }

    /// @brief k 番目を v に変更する
    void set(int k, bool v) {
        assert(0 <= k && k < len());
        auto [bucket_pos, bit_pos] = get_bucket(k);
        data[bucket_pos][bit_pos] = v;
    }

    /// @brief 全要素を列で返す
    vector<T> tovector() const {
        vector<T> a(len());
        int ptr = 0;
        for (const vector<T> &d: data) for (const T &x: d) {
            a[ptr++] = x;
        }
        return a;
    }

    /// @brief 列を出力する
    void print() const {
        vector<T> a = tovector();
        int n = (int)a.size();
        assert(n == len());
        cout << "[";
        for (int i = 0; i < n-1; ++i) {
            cout << a[i] << ", ";
        }
        if (n > 0) {
            cout << a.back();
        }
        cout << "]";
        cout << endl;
    }

    bool empty() const { return n == 0; }
    int len() const { return n; }
};
} // namespace titan23
