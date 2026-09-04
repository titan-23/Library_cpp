/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/centroid_decomposition.cpp
#pragma once

#include <vector>
using namespace std;

namespace titan23 {

class CentroidDecomposition {
private:
    int C;
    vector<vector<int>> G, T;
    vector<int> sub;
    vector<bool> banned, solve_banned;

    int dfs(int v, int p) {
        sub[v] = 1;
        for (int x : G[v]) {
            if (x == p || banned[x]) continue;
            sub[v] += dfs(x, v);
        }
        return sub[v];
    }

    int find_centroid(int v, int p, int mid) const {
        for (int x : G[v]) {
            if (x == p || banned[x]) continue;
            if (sub[x] > mid) return find_centroid(x, v, mid);
        }
        return v;
    }

    int build(int v) {
        int total = dfs(v, -1);
        int c = find_centroid(v, -1, total/2);
        banned[c] = true;
        for (int x : G[c]) if (!banned[x]) {
            int w = build(x);
            T[c].push_back(w);
            T[w].push_back(c);
        }
        return c;
    }

    template <typename F>
    void inner_solve(int v, int p, F &&func) {
        func(v, solve_banned);
        solve_banned[v] = true;
        for (int x : T[v]) if (x != p) {
            inner_solve(x, v, func);
        }
    }

public:
    CentroidDecomposition() : C(-1) {}
    CentroidDecomposition(const vector<vector<int>> &G) : G(G) {
        int n = G.size();
        if (n == 0) return;
        T.resize(n);
        sub.resize(n);
        banned.resize(n, false);
        solve_banned.resize(n);
        C = build(0);
    }

    pair<int, vector<vector<int>>> get_result() {
        return {C, T};
    }

    // vをまたぐパスについて考える vのみ / vを端点 / vを端点としない などの場合分け
    // パスの重複に注意 / 元の木と重心分解の木の混同に注意
    // bannedを見てはいけない
    // cd.solve([&] (int v, const vector<bool>& banned) {});

    // solve(func) は重心分解木 T を根 C から葉に向かって辿る
    // 各頂点 v で func(v, banned) を呼ぶ
    // 第2引数の banned は v より上位で既に中心として使われた頂点を true にした配列
    // v の処理後に v 自身も true になってから、T の子へ降りる
    // つまり func が呼ばれた時点で元の木 G 上で v から辿れる頂点のうち
    // banned が false な範囲が
    // 「v を中心として今回処理すべき部分木」にあたる
    // func 内で部分木を探索するときは元の木 G の隣接リストを辿る
    // banned が true な頂点には踏み込まない
    // v をまたぐパスを数えるときは v の子部分木ごとに独立に集計する
    // それぞれを集計してから組み合わせる
    // 同じ子部分木に属する2頂点間の対は v をまたがない
    // まとめて集計すると重複や誤カウントになる
    // cd.solve([&] (int v, const vector<bool>& banned) { ... });
    template <typename F>
    void solve(F &&func) {
        if (G.empty()) return;
        fill(solve_banned.begin(), solve_banned.end(), false);
        inner_solve(C, -1, func);
    }
};
} // namespace titan23
