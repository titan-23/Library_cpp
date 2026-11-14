/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/string/eer_tree.cpp
#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <cassert>
#include <array>
using namespace std;

namespace titan23 {

/// @brief 回文木 (eertree) / 空間O(n), 時間O(nlogσ)
/// 参考: https://math314.hateblo.jp/entry/2016/12/19/005919
class EerTree {
private:
    int last_idx, n;
    long long all_count;
    string s;
    vector<map<char, int>> child;
    // vector<array<int, 26>> child;
    vector<int> par, len, suffix_link, count, start, suffix_link_dep;
    vector<int> suff; // suff[i]:= s[:i+1]の最長回文接尾辞の頂点番号

    int new_node() {
        int idx = suffix_link.size();
        child.emplace_back();
        // static const array<int, 26> INIT_CHILD = [] {
        //     array<int, 26> a{};
        //     a.fill(-1);
        //     return a;
        // } ();
        // child.emplace_back(INIT_CHILD);
        suffix_link.emplace_back();
        len.emplace_back(0);
        par.emplace_back(0);
        count.emplace_back(0);
        start.emplace_back(0);
        suffix_link_dep.emplace_back(0);
        return idx;
    }

    int get_upper(int node, char c) const {
        while (node != 0) {
            if (n-len[node]-1 >= 0 && s[n-len[node]-1] == c) {
                break;
            }
            node = suffix_link[node];
        }
        return node;
    }

public:
    /// @brief 空文字列の回文木を作る / O(1)
    EerTree() : n(0), all_count(0) {
        int idx0 = new_node();
        int idx1 = new_node();
        len[idx0] = -1;
        len[idx1] = 0;
        suffix_link[idx0] = 0;
        suffix_link[idx1] = idx0;
        suffix_link_dep[idx0] = 0;
        suffix_link_dep[idx1] = 0;
        par[idx0] = 0; // 自身を指す
        par[idx1] = 0;
        last_idx = idx1;
    }

    /// @brief 長さ n の文字列の分だけ領域を確保する / O(n)
    void reserve(int n) {
        child.reserve(n+2);
        suffix_link.reserve(n+2);
        len.reserve(n+2);
        par.reserve(n+2);
        count.reserve(n+2);
        start.reserve(n+2);
        suffix_link_dep.reserve(n+2);
    }

    /// @brief 末尾に文字 c を追加する
    void add(char c) {
        s += c;
        int now = get_upper(last_idx, c);
        if (child[now].find(c) != child[now].end()) {
            last_idx = child[now][c];
        // if (child[now][c-'a'] != -1) {
        //     last_idx = child[now][c-'a'];
            suff.emplace_back(last_idx);
            n++;
            count[last_idx]++;
            all_count += suffix_link_dep[last_idx];
            return;
        }
        int idx = new_node();
        len[idx] = len[now]+2;
        child[now][c] = idx;
        // child[now][c-'a'] = idx;
        par[idx] = now;
        count[idx] = 1;
        start[idx] = n+1-len[idx];
        suff.emplace_back(idx);
        last_idx = idx;
        if (len[idx] == 1) {
            suffix_link[idx] = 1;
        } else {
            int k = get_upper(suffix_link[now], c);
            suffix_link[idx] = child[k][c];
            // suffix_link[idx] = child[k][c-'a'];
        }
        suffix_link_dep[idx] = suffix_link_dep[suffix_link[idx]]+1;
        all_count += suffix_link_dep[idx];
        n++;
    }

    /// @brief 頂点番号 idx の回文が s に最初に現れる区間 [l, r) を返す / O(1)
    pair<int, int> get_range_from_idx(int idx) {
        return {start[idx], start[idx]+len[idx]};
    }

    /// @brief s[i] で終わる最長の回文の頂点番号を返す / O(1)
    int get_suff(int i) const {
        return suff[i];
    }

    /// @brief s[i] で終わる最長の回文の長さを返す / O(1)
    int get_max_length_suffix(int i) {
        return len[suff[i]];
    }

    /// @brief s[i] で終わる空でない回文の個数を返す / O(1)
    int count_suffix_palindromes(int i) {
        return suffix_link_dep[suff[i]];
    }

    /// @brief s[i] で終わる空でない回文の頂点番号をすべて返す
    vector<int> enumerate_suffix(int i) {
        vector<int> res(count_suffix_palindromes(i));
        int k = 0;
        int idx = suff[i];
        while (idx > 1) {
            res[k] = idx; k++;
            idx = suffix_link[idx];
        }
        return res;
    }

    /// @brief 空でない回文の出現の総数を返す / O(1)
    long long count_all_palindromes() const {
        return all_count;
    }

    /// @brief 相異なる空でない回文の個数を返す / O(1)
    int count_unique_palindromes() const {
        return (int)child.size() - 2;
    }

    /// @brief 回文番号ごとの出現回数を返す / 回文番号は頂点番号から 2 を引いた値
    vector<int> get_freq() const {
        vector<int> cnt((int)count.size()-2, 0);
        for (int i = (int)child.size()-1; i >= 2; --i) {
            cnt[i-2] += count[i];
            if (suffix_link[i]-2 >= 0) cnt[suffix_link[i]-2] += cnt[i-2];
        }
        return cnt;
    }

    /// @brief 回文番号 idx の回文が s に最初に現れる区間 [l, r) を返す / O(1)
    pair<int, int> idx_to_range(int idx) const {
        idx += 2;
        return {start[idx], start[idx]+len[idx]};
    }

    /// @brief 回文番号 idx の回文 t を返す / O(|t|)
    string idx_to_string(int idx) const {
        idx += 2;
        return s.substr(start[idx], len[idx]);
    }

    /// @brief 木の構造と各回文の出現回数を出力する
    void print() const {
        vector<pair<int, int>> res((int)child.size()-2);

        auto dfs = [&] (auto &&dfs, int node, int par) -> void {
            if (node > 1) {
                res[node-2] = {par-1, suffix_link[node]-1};
            }
            for (auto [_, c] : child[node]) {
                dfs(dfs, c, node);
            // for (int i = 0; i < 26; ++i) if (child[node][i] != -1) {
            //     dfs(dfs, child[node][i], node);
            }
        };

        dfs(dfs, 0, 0);
        dfs(dfs, 1, 0);

        cout << res.size() << "\n";
        for (auto [p, s] : res) {
            cout << p << " " << s << "\n";
        }
        for (int i = 0; i < n; ++i) {
            cout << suff[i]-1 << " ";
        }
        cout << endl;

        cout << "---" << endl;
        auto freq = get_freq();
        for (int i = 0; i < count_unique_palindromes(); ++i) {
            cout << i << " : " << idx_to_string(i) << ", " << freq[i] << endl;
        }
    }
};
} // namespace titan23
