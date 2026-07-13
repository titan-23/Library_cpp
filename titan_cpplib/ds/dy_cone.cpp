#pragma once

#include <vector>
#include <tuple>
#include <random>
#include <cassert>
#include <climits>
#include <numeric>
#include <algorithm>
#include <unordered_map>
using namespace std;

// DyCone
namespace titan23 {

// オフライン動的連結性 (削除時刻を重みとする最大全域森)
// 頂点重み T は可換群を要求する (+=, -=, ゼロ値でのデフォルト構築)
//
// 使い方
//   1. add_edge / delete_edge / add_point / next_query を時系列順に呼ぶ
//   2. run(out) を呼ぶと k 番目の next_query の時点で out(k) が呼ばれる
//   3. same / size / sum / group_count は run のコールバック内または run 後に呼ぶ
//
// 契約
//   - delete_edge の対象は現存する辺 (assert)
//   - 多重辺は可 (削除は後入れ先出しで対応づける)
//   - 自己ループは追加・削除とも無視される
//   - run は 1 回だけ呼べる (assert)
//
// 計算量は 1 操作あたり償却・期待 O(log n)
// 解析は review/dycone_complexity.md 設計は review/dycone_redesign.md
template<typename T>
class DyCone {
  private:
    static constexpr int ADD = 0, DEL = 1, POINT = 2, QUERY = 3;
    static constexpr int NEVER = -INT_MAX; // 削除されない辺の重み

    int n;
    int group_count_;
    int phase; // 0: 構築 / 1: run 中 / 2: run 後
    vector<int> P, W, rd, sz;
    vector<T> val; // ポインタ木での部分木重み和
    vector<tuple<int, int, int>> Q; // (種別, u, v) POINT の v は vals の添字
    vector<T> vals;
    vector<int> S; // 更新 i が ADD のときその辺の重み DEL のとき削除重み
    unordered_map<long long, vector<int>> mp; // 現存辺 -> 追加時刻のスタック
    vector<int> stk;

    // u から重み w 以下の辺を登った先を返す 道中の単調性違反を短絡する
    int find(int u, int w = 0) {
        while (W[u] <= w) {
            while (W[P[u]] <= W[u]) {
                val[P[u]] -= val[u];
                sz[P[u]] -= sz[u];
                P[u] = P[P[u]];
            }
            u = P[u];
        }
        return u;
    }

    // u の根経路の集約を上から順に開く
    void disconnect(int u) {
        stk.clear();
        while (P[u] != u) {
            stk.push_back(u);
            u = P[u];
        }
        for (int i = (int)stk.size()-1; i >= 0; --i) {
            int v = stk[i];
            val[P[v]] -= val[v];
            sz[P[v]] -= sz[v];
        }
    }

    // u から重み w 以下の辺を登りつつ集約を閉じる
    int connect(int u, int w = 0) {
        while (W[u] <= w) {
            val[P[u]] += val[u];
            sz[P[u]] += sz[u];
            u = P[u];
        }
        return u;
    }

    // u-v パス上の最大重み辺を担うノードを返す 非連結なら -1
    int max_edge(int u, int v) {
        if (find(u) != find(v)) return -1;
        while (true) {
            if (W[u] > W[v]) swap(u, v);
            if (P[u] == v) break;
            u = P[u];
        }
        return u;
    }

    void sub_add(int u, int v, int w) {
        disconnect(u);
        disconnect(v);
        assert(find(u) != find(v)); // 呼び出し元が非連結を保証する
        group_count_--;
        while (u != v) {
            u = connect(u, w);
            v = connect(v, w);
            if (rd[u] < rd[v]) swap(u, v);
            int np = P[v];
            int nw = W[v];
            P[v] = u;
            W[v] = w;
            u = np;
            w = nw;
        }
        connect(u);
    }

    void sub_del(int u, int v, int w) {
        while (P[u] != u) {
            if (W[u] == w) {
                int x = u;
                while (P[x] != x) {
                    x = P[x];
                    val[x] -= val[u];
                    sz[x] -= sz[u];
                }
                P[u] = u;
                W[u] = 1;
                if (find(u) != find(v)) group_count_++;
                return;
            }
            while (W[P[u]] <= W[u]) {
                val[P[u]] -= val[u];
                sz[P[u]] -= sz[u];
                P[u] = P[P[u]];
            }
            u = P[u];
        }
    }

    void inner_add_edge(int u, int v, int w) {
        int p = max_edge(u, v);
        if (p == -1) {
            sub_add(u, v, w);
        } else if (W[p] > w) {
            sub_del(p, P[p], W[p]);
            sub_add(u, v, w);
        }
    }

    void inner_delete_edge(int u, int v, int w) {
        sub_del(u, v, w);
        sub_del(v, u, w);
    }

    // 根までの経路上に x を加算する 道中の単調性違反を短絡する
    void inner_add_point(int u, T x) {
        while (true) {
            if (P[u] == u) {
                val[u] += x;
                break;
            }
            while (W[P[u]] <= W[u]) {
                val[P[u]] -= val[u];
                sz[P[u]] -= sz[u];
                P[u] = P[P[u]];
            }
            val[u] += x;
            u = P[u];
        }
    }

  public:
    explicit DyCone(int n, unsigned int seed = 1321312)
            : n(n), group_count_(n), phase(0),
              P(n), W(n, 1), rd(n), sz(n, 1), val(n) {
        iota(P.begin(), P.end(), 0);
        iota(rd.begin(), rd.end(), 0);
        shuffle(rd.begin(), rd.end(), mt19937(seed));
    }

    explicit DyCone(const vector<T> &init, unsigned int seed = 1321312)
            : DyCone((int)init.size(), seed) {
        val = init;
    }

    void reserve(int cap) {
        Q.reserve(cap);
        S.reserve(cap);
        mp.reserve(cap);
    }

    // ---- 構築フェーズ: 時系列順に呼ぶ ----

    void add_edge(int u, int v) {
        assert(phase == 0);
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return;
        if (u > v) swap(u, v);
        mp[(long long)u * n + v].push_back((int)Q.size());
        Q.emplace_back(ADD, u, v);
        S.push_back(NEVER);
    }

    void delete_edge(int u, int v) {
        assert(phase == 0);
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return;
        if (u > v) swap(u, v);
        auto it = mp.find((long long)u * n + v);
        assert(it != mp.end() && !it->second.empty()); // 現存する辺のみ削除できる
        int t = (int)Q.size();
        S[it->second.back()] = -t;
        it->second.pop_back();
        Q.emplace_back(DEL, u, v);
        S.push_back(-t);
    }

    void add_point(int u, T x) {
        assert(phase == 0);
        assert(0 <= u && u < n);
        Q.emplace_back(POINT, u, (int)vals.size());
        vals.push_back(x);
        S.push_back(NEVER);
    }

    void next_query() {
        assert(phase == 0);
        Q.emplace_back(QUERY, 0, 0);
        S.push_back(NEVER);
    }

    // ---- 実行: 1 回限り ----

    template<typename F> // void out(int k)
    void run(F &&out) {
        assert(phase == 0);
        phase = 1;
        int k = 0;
        int m = (int)Q.size();
        for (int i = 0; i < m; ++i) {
            const auto &[type, u, v] = Q[i];
            switch (type) {
                case ADD: inner_add_edge(u, v, S[i]); break;
                case DEL: inner_delete_edge(u, v, S[i]); break;
                case POINT: inner_add_point(u, vals[v]); break;
                case QUERY: out(k++); break;
                default: assert(false);
            }
        }
        phase = 2;
    }

    // ---- 参照: run のコールバック内または run 後に呼ぶ ----

    bool same(int u, int v) {
        assert(phase >= 1);
        assert(0 <= u && u < n && 0 <= v && v < n);
        return find(u) == find(v);
    }

    int size(int u) {
        assert(phase >= 1);
        assert(0 <= u && u < n);
        return sz[find(u)];
    }

    T sum(int u) {
        assert(phase >= 1);
        assert(0 <= u && u < n);
        return val[find(u)];
    }

    int group_count() const {
        assert(phase >= 1);
        return group_count_;
    }
};
} // namespace titan23
