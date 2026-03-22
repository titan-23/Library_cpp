#include <bits/stdc++.h>
#include "titan_cpplib/algorithm/random_mt.cpp"
using namespace std;

enum class TreeType {
    // 基本的にはこれらのタイプ
    Random,     // 一様ランダム
    Line,       // 一直線の木
    AlmostLine, // 一直線の木+α
    Uni,        // ウニグラフ(中心から √N 本のパスグラフが生えている)
    Star,       // スターグラフ(中心から長さ 1 のパスグラフが生えている)
    Balanced,   // 完全二分木

    Centipede,  // ムカデグラフ(一直線の木の各頂点に一つずつ頂点を接続する) centipedeはムカデの意、ググると画像が出るので注意 `centipede graph`ならよさそう
    MultiUni,   // √N個のウニグラフをランダムにつなぐ
    MultiStar,  // √N 個のスターグラフをランダムにつなぐ
    LongPathDecompositionKiller, // 長さ 1, 3, 5, 7, ... のパスをくっつけていくグラフ
    Lobster,    // ロブスター木(？)
};

/**
 * @brief 木のジェネレータ
 */
class TreeGenerator {
  private:
    titan23::RandomMT TreeRnd;

    class UnionFind {
    private:
        int n, group_numbers;
        vector<int> par;

    public:
        UnionFind() : n(0), group_numbers(0), par(0) {}
        UnionFind(int n) : n(n), group_numbers(n), par(n, -1) {}

        int root(int x) {
            int a = x, y;
            while (par[a] >= 0) a = par[a];
            while (par[x] >= 0) {
                y = x;
                x = par[x];
                par[y] = a;
            }
            return a;
        }

        bool unite(int x, int y) {
            x = root(x);
            y = root(y);
            if (x == y) return false;
            --group_numbers;
            if (par[x] >= par[y]) swap(x, y);
            par[x] += par[y];
            par[y] = x;
            return true;
        }

        int size(const int x) { return -par[root(x)]; }

        bool same(const int x, const int y) { return root(x) == root(y); }

        int group_count() const { return group_numbers; }
    };

    int _idx(const vector<int> &a, int x) {
        for (int i = 0; i < (int)a.size(); ++i) {
            if (a[i] == x) return i;
        }
        return -1;
    }

    void tree_shuffle(vector<pair<int, int>> &edges, const bool fixed_point=false) {
        int n = edges.size() + 1;
        vector<int> p(n);
        for (int i = 0; i < n; ++i) {
            p[i] = i;
        }
        if (p.size() > 1) {
            if (fixed_point) {
                TreeRnd.shuffle(p.begin()+1, p.end());
            } else {
                TreeRnd.shuffle(p.begin(), p.end());
            }
        }
        for (int i = 0; i < n-1; ++i) {
            edges[i].first = p[edges[i].first];
            edges[i].second = p[edges[i].second];
            if (TreeRnd.randint(0, 1) == 0) {
                swap(edges[i].first, edges[i].second);
            }
        }
        TreeRnd.shuffle(edges.begin(), edges.end());
    }

    string _encode_tree(int u, int p, const vector<vector<int>>& G) const {
        vector<string> ch;
        for (int v : G[u]) {
            if (v != p) ch.push_back(_encode_tree(v, u, G));
        }
        sort(ch.begin(), ch.end());
        string res = "(";
        for (const string &s : ch) res += s;
        res += ")";
        return res;
    }

    string _get_tree_hash(int n, const vector<pair<int, int>>& edges) const {
        if (n <= 1) return "()";
        vector<vector<int>> G(n);
        vector<int> deg(n, 0);
        for (auto &[u, v] : edges) {
            G[u].push_back(v);
            G[v].push_back(u);
            deg[u]++;
            deg[v]++;
        }
        queue<int> q;
        for (int i = 0; i < n; ++i) {
            if (deg[i] <= 1) q.push(i);
        }
        int rem = n;
        while (rem > 2) {
            int sz = q.size();
            rem -= sz;
            for (int i = 0; i < sz; ++i) {
                int u = q.front(); q.pop();
                for (int v : G[u]) {
                    if (--deg[v] == 1) q.push(v);
                }
            }
        }
        vector<int> centers;
        while (!q.empty()) {
            centers.push_back(q.front());
            q.pop();
        }
        if (centers.size() == 1) {
            return _encode_tree(centers[0], -1, G);
        } else {
            string h1 = _encode_tree(centers[0], centers[1], G);
            string h2 = _encode_tree(centers[1], centers[0], G);
            if (h1 > h2) swap(h1, h2);
            return h1 + h2;
        }
    }

    /**
     * @brief 一様ランダム
     */
    void gen_random(const int N, vector<pair<int, int>> &edges) {
        // https://speakerdeck.com/tsutaj/beerbash-lt-230711?slide=27
        if (N <= 1) return;
        if (N == 2) {
            if (TreeRnd.randint(0, 1) == 0) {
                edges.emplace_back(0, 1);
            } else {
                edges.emplace_back(1, 0);
            }
            return;
        }
        assert(N >= 2);
        edges.reserve(N-1);
        vector<int> D(N, 1);
        vector<int> A(N-2, 0);
        for (int i = 0; i < N-2; ++i) {
            int v = TreeRnd.randint(0, N-1);
            D[v]++;
            A[i] = v;
        }
        set<pair<int, int>> st;
        for (int i = 0; i < N; ++i) {
            st.insert({D[i], i});
        }
        for (const int &a : A) {
            auto it = st.begin();
            int d = it->first, v = it->second;
            st.erase(it);
            assert(d == 1);
            edges.emplace_back(v, a);
            D[v]--;
            st.erase({D[a], a});
            D[a]--;
            if (D[a] >= 1) {
                st.insert({D[a], a});
            }
        }
        int u = _idx(D, 1);
        D[u]--;
        int v = _idx(D, 1);
        edges.emplace_back(u, v);
        assert(edges.size() == N-1);
    }

    /**
     * @brief ウニグラフ
     * 中心から √N 本のパスグラフが生えている
     */
    void gen_uni(const int N, vector<pair<int, int>> &edges, int d=-1) {
        if (N <= 1) return;
        if (d == -1) {
            d = sqrt(N);
        }
        assert(1 <= d && d <= N-1);
        int center = 0;
        vector<int> last(d, center);
        for (int i = 1; i <= d; ++i) {
            int idx = i-1;
            edges.emplace_back(i, last[idx]);
            last[idx] = i;
        }
        for (int i = d+1; i < N; ++i) {
            int idx = TreeRnd.randint(0, d-1);
            edges.emplace_back(i, last[idx]);
            last[idx] = i;
        }
    }

    /**
     * @brief スターグラフ
     * 中心から長さ 1 のパスが生えている
     */
    void gen_star(const int N, vector<pair<int, int>> &edges) {
        gen_uni(N, edges, N-1);
    }

    /**
     * @brief パスグラフ
     */
    void gen_line(const int N, vector<pair<int, int>> &edges) {
        for (int i = 0; i < N-1; ++i) {
            edges.emplace_back(i, i+1);
        }
    }

    /**
     * @brief パスグラフ+α
     * 長さ N*9/10 のパスを作り、残りを適当にくっつける
     */
    void gen_almost_line(const int N, vector<pair<int, int>> &edges) {
        int length = max(1, N*9/10);
        for (int i = 0; i < length; ++i) {
            edges.emplace_back(i, i+1);
        }
        for (int i = length+1; i < N; ++i) {
            // int v = TreeRnd.randint(0, i);
            int v = TreeRnd.randint(0, i-1);
            edges.emplace_back(i, v);
        }
    }

    /**
     * @brief ウニつなぎ
     * √N 個のウニを作り、ランダムにつなぐ
     */
    void gen_multi_uni(const int N, vector<pair<int, int>> &edges, bool is_star=false) {
        const int uni_cnt = max(1, (int)sqrt(N));
        vector<vector<int>> each_uni(uni_cnt);
        for (int i = 0; i < uni_cnt; ++i) {
            each_uni[i%uni_cnt].emplace_back(i);
        }
        for (int i = uni_cnt; i < N; ++i) {
            int idx = TreeRnd.randint(0, uni_cnt-1);
            each_uni[idx].emplace_back(i);
        }

        vector<vector<pair<int, int>>> all_edges;
        for (const vector<int> &a : each_uni) {
            assert(!a.empty());
            vector<pair<int, int>> es;
            if (a.size() > 1) {
                int d = is_star ? (int)a.size()-1 : TreeRnd.randint(1, (int)a.size()-1);
                gen_uni(a.size(), es, d);
            }
            tree_shuffle(es);
            for (int i = 0; i < es.size(); ++i) {
                es[i].first = a[es[i].first];
                es[i].second = a[es[i].second];
            }
            all_edges.emplace_back(es);
        }

        vector<pair<int, int>> tree_edges;
        gen_random(all_edges.size(), tree_edges);
        tree_shuffle(tree_edges);
        for (auto [u, v] : tree_edges) {
            const vector<int> &es_u = each_uni[u];
            const vector<int> &es_v = each_uni[v];
            int u2 = es_u[TreeRnd.randint(0, es_u.size()-1)];
            int v2 = es_v[TreeRnd.randint(0, es_v.size()-1)];
            edges.emplace_back(u2, v2);
        }
        for (const vector<pair<int, int>> &es : all_edges) {
            for (auto [u, v] : es) {
                edges.emplace_back(u, v);
            }
        }
    }

    /**
     * @brief スターつなぎ
     * √N 個のスターを作り、ランダムにつなぐ
     */
    void gen_multi_star(const int N, vector<pair<int, int>> &edges) {
        return gen_multi_uni(N, edges, true);
    }

    /**
     * @brief ムカデグラフ
     * (一直線の木の各頂点に一つずつ頂点を接続する)
     */
    void gen_centipede(const int N, vector<pair<int, int>> &edges) {
        for (int i = 0; i < N-1; i += 2) {
            edges.emplace_back(i, i+1);
        }
        for (int i = 0; i < N; i += 2) {
            if (i+2 < N) {
                edges.emplace_back(i, i+2);
            }
        }
    }

    /**
     * @brief 完全二分木
     */
    void gen_balanced(const int N, vector<pair<int, int>> &edges) {
        for (int i = 1; i <= N; ++i) {
            if (i*2+0 <= N) edges.emplace_back(i-1, i*2+0-1);
            if (i*2+1 <= N) edges.emplace_back(i-1, i*2+1-1);
        }
    }

    /**
     * @brief 長さ 1, 3, 5, 7, ... のパスをくっつけていく
     * @ref https://github.com/yosupo06/library-checker-problems/blob/master/tree/vertex_set_path_composite/gen/long-path-decomposition_killer.cpp
     */
    void gen_long_path_decomposition_killer(const int N, vector<pair<int, int>> &edges) {
        vector<int> roots;
        int root = 0;
        for (int len = 1; ; len += 2) {
            if (root + len > N) break;
            roots.emplace_back(root);
            for (int i = 0; i < len - 1; ++i) {
                edges.emplace_back(root + i, root + i + 1);
            }
            root += len;
        }
        for (int i = 0; i + 1 < (int)roots.size(); ++i) {
            edges.emplace_back(roots[i], roots[i + 1]);
        }
        for (int i = root; i < N; ++i) {
            edges.emplace_back(0, i);
        }
    }

    void gen_lobster(const int N, vector<pair<int, int>> &edges) {
        const int path_len = max(1, (int)sqrt(sqrt(N))); // 謎
        const int D = max(1, (int)sqrt(sqrt(sqrt(N-path_len)))); // 謎
        vector<pair<int, int>> dia = {{0, 0}};
        for (int i = 0; i < path_len-1; ++i) {
            edges.emplace_back(i, i+1);
            dia.emplace_back(i+1, 0);
        }
        for (int i = path_len; i < N; ++i) {
            assert(!dia.empty());
            int idx = TreeRnd.randint(0, (int)dia.size()-1);
            auto [j, d] = dia[idx];
            edges.emplace_back(i, j);
            if (d < D) {
                dia.emplace_back(i, d+1);
            }
        }
    }

    bool check(const vector<pair<int, int>> &edges) const {
        int n = edges.size() + 1;
        UnionFind uf(n);
        for (auto [u, v] : edges) {
            if (uf.same(u, v)) return false;
            uf.unite(u, v);
        }
        return uf.group_count() == 1;
    }

  public:
    TreeGenerator() {}
    TreeGenerator(int seed) {
        TreeRnd = titan23::RandomMT(seed);
    }

    /**
     * @brief 所望の木を生成し、辺集合を返す
     *
     * @param tree_type 木のタイプ
     * @param N 頂点数
     * @param fixed_point 根や中心を固定するかどうか．する場合，頂点`0`がそれになる．
     * @param seed `-1`のとき、seedは設定しない。それ以外のとき、seed値を設定する。
     * @return vector<pair<int, int>> 辺集合。0-indexed.
     */
    vector<pair<int, int>> generate(
        const TreeType tree_type,
        const int N,
        const bool fixed_point=false,
        const int seed=-1
    ) {
        if (seed != -1) {
            TreeRnd = titan23::RandomMT(seed);
        }
        if (N <= 1) {
            return {};
        }

        vector<pair<int, int>> edges;
        switch (tree_type) {
            case TreeType::Random:
                gen_random(N, edges); break;
            case TreeType::Line:
                gen_line(N, edges); break;
            case TreeType::AlmostLine:
                gen_almost_line(N, edges); break;
            case TreeType::Uni:
                gen_uni(N, edges); break;
            case TreeType::MultiUni:
                gen_multi_uni(N, edges); break;
            case TreeType::Star:
                gen_star(N, edges); break;
            case TreeType::MultiStar:
                gen_multi_star(N, edges); break;
            case TreeType::Centipede:
                gen_centipede(N, edges); break;
            case TreeType::Balanced:
                gen_balanced(N, edges); break;
            case TreeType::LongPathDecompositionKiller:
                gen_long_path_decomposition_killer(N, edges); break;
            case TreeType::Lobster:
                gen_lobster(N, edges); break;
            default: assert(false);
        }

        tree_shuffle(edges, fixed_point);

        if (!check(edges)) {
            cerr << "this is not a tree." << endl;
            exit(1);
        }

        return edges;
    }

    /**
     * @brief 所望の木を生成し、辺集合を返す
     *
     * @param tree_type 木のタイプ
     * @param N 頂点数
     * @param fixed_point 根や中心を固定するかどうか．する場合，頂点`1`がそれになる．
     * @param seed `-1`のとき、seedは設定しない。それ以外のとき、seed値を設定する。
     * @return vector<pair<int, int>> 辺集合。**1-indexed**.
     */
    vector<pair<int, int>> generate_1indexed(
        const TreeType tree_type,
        const int N,
        const bool fixed_point=false,
        const int seed=-1
    ) {
        vector<pair<int, int>> edges = generate(
            tree_type,
            N,
            fixed_point,
            seed
        );
        for (int i = 0; i < N-1; ++i) {
            edges[i].first++;
            edges[i].second++;
        }
        return edges;
    }

    /**
     * @brief N頂点のラベル付き木をすべて生成する(N^(N-2)通り)
     * N <= 8 程度
     * @param N 頂点数
     * @return vector<vector<pair<int, int>>> 0-indexed の辺集合のリスト
     */
    vector<vector<pair<int, int>>> generate_all_labeled(int N) const {
        if (N <= 0) return {};
        if (N == 1) return {{}};
        if (N == 2) return {{{0, 1}}};

        vector<vector<pair<int, int>>> res;
        long long num_trees = 1;
        for (int i = 0; i < N - 2; ++i) num_trees *= N;

        for (long long i = 0; i < num_trees; ++i) {
            vector<int> prufer(N - 2);
            long long temp = i;
            for (int j = 0; j < N - 2; ++j) {
                prufer[j] = temp % N;
                temp /= N;
            }

            vector<int> deg(N, 1);
            for (int v : prufer) deg[v]++;

            vector<pair<int, int>> edges;
            edges.reserve(N - 1);
            for (int v : prufer) {
                for (int j = 0; j < N; ++j) {
                    if (deg[j] == 1) {
                        edges.emplace_back(v, j);
                        deg[v]--;
                        deg[j]--;
                        break;
                    }
                }
            }
            int u = -1, v = -1;
            for (int j = 0; j < N; ++j) {
                if (deg[j] == 1) {
                    if (u == -1) u = j;
                    else v = j;
                }
            }
            edges.emplace_back(u, v);
            res.push_back(edges);
        }
        return res;
    }

    /**
     * @brief N頂点のラベルなし木を生成する
     * N <= 14 程度
     * @param N 頂点数
     * @return vector<vector<pair<int, int>>> 0-indexed の辺集合のリスト
     */
    vector<vector<pair<int, int>>> generate_all_unlabeled(int N) const {
        if (N <= 0) return {};
        if (N == 1) return {{}};
        set<string> seen;
        vector<vector<pair<int, int>>> now_tree = {{}};
        for (int i = 2; i <= N; ++i) {
            vector<vector<pair<int, int>>> next_trees;
            seen.clear();
            for (const auto &edges : now_tree) {
                for (int v = 0; v < i - 1; ++v) {
                    vector<pair<int, int>> new_edges = edges;
                    new_edges.emplace_back(v, i - 1);
                    string h = _get_tree_hash(i, new_edges);
                    if (seen.find(h) == seen.end()) {
                        seen.insert(h);
                        next_trees.push_back(new_edges);
                    }
                }
            }
            now_tree = move(next_trees);
        }
        return now_tree;
    }

    vector<vector<pair<int, int>>> generate_all_labeled_1indexed(int N) const {
        auto res = generate_all_labeled(N);
        for (auto &edges : res) {
            for (auto &[u, v] : edges) {
                u++; v++;
            }
        }
        return res;
    }

    vector<vector<pair<int, int>>> generate_all_unlabeled_1indexed(int N) const {
        auto res = generate_all_unlabeled(N);
        for (auto &edges : res) {
            for (auto &[u, v] : edges) {
                u++; v++;
            }
        }
        return res;
    }
};

// // sample
// int main() {
//     TreeGenerator tree;
//     int case_num = 1;
//     for (int i = 0; i < case_num; ++i) {
//         int N = 3;
//         vector<pair<int, int>> edges = tree.generate_1indexed(TreeType::MultiUni, N);
//         cout << N << " " << N-1 << endl;
//         for (auto [u, v] : edges) {
//             cout << u << " " << v << endl;
//         }
//     }

//     for (int i = 0; i < 100; ++i) {
//         vector<pair<int, int>> edges = tree.generate_1indexed(TreeType::MultiUni, i);
//     }
//     return 0;
// }
