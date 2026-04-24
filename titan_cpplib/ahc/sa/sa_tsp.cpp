// OMP_NUM_THREADS=8 time ./a.out < in/0000.txt > out.txt
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"
#include "titan_cpplib/ahc/kmeans.cpp"
using namespace std;
using ll = long long;
#define rep(i, n) for (int i = 0; i < (int)(n); ++i)

int N;
vector<pair<long double, long double>> YX;
vector<vector<ll>> DIST;
vector<vector<int>> nearest_neighbors;
int NEIBHORS;
int K;
vector<int> START_NODES;

// minimize SA
namespace sa {

thread_local titan23::Random sarnd;

void sa_init() {
    DIST.assign(N, vector<ll>(N, 0.0));
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            long double dx = YX[i].first - YX[j].first;
            long double dy = YX[i].second - YX[j].second;
            DIST[i][j] = (ll)(sqrtl(dx * dx + dy * dy) + 0.5);
        }
    }

    NEIBHORS = min(N-1, 5);
    nearest_neighbors.assign(N, vector<int>(NEIBHORS));
    for (int i = 0; i < N; ++i) {
        vector<pair<ll, int>> dist_idx;
        for (int j = 0; j < N; ++j) {
            if (i != j) dist_idx.push_back({DIST[i][j], j});
        }
        sort(dist_idx.begin(), dist_idx.end());
        for (int k = 0; k < NEIBHORS; ++k) {
            nearest_neighbors[i][k] = dist_idx[k].second;
        }
    }
}

// 座標の型
using ElmType = pair<long double, long double>;

// 2点間の距離（K-means用なので平方根を取らなくても順序は変わらないため、高速化のため2乗のまま）
long double kmeans_dist(const ElmType &a, const ElmType &b) {
    long double dx = a.first - b.first;
    long double dy = a.second - b.second;
    return dx * dx + dy * dy;
}

// 都市群の重心を計算
ElmType kmeans_mean(const vector<ElmType> &pts) {
    if (pts.empty()) return {0.0, 0.0};
    long double sx = 0, sy = 0;
    for (const auto &p : pts) {
        sx += p.first;
        sy += p.second;
    }
    return {sx / (long double)pts.size(), sy / (long double)pts.size()};
}

class State {
public:
    using ScoreType = ll;

    struct Param {
        double start_temp = 1e6;
        double end_temp = 1e0;
    };

    struct Changed {
        int TYPE_CNT = 6;
        int type;
        // 遷移の対象となるルートとインデックス
        int r1, r2;
        int u_idx, v_idx, w_idx;
        int L1, L2;

        // 各ルートの距離の変化分
        ll diff_d1, diff_d2;
        ScoreType diff;
        Changed() {}
    };

    struct Result {
        ScoreType score, true_score;
        vector<vector<int>> tours;
        Result() {}
        Result(ScoreType s, ScoreType ts, const vector<vector<int>> &t) : score(s), true_score(ts), tours(t) {}
        void print(ostream &os = cout) const {
            for (int k = 0; k < K; ++k) {
                for (int i = 0; i < tours[k].size(); ++i) {
                    os << tours[k][i] << (i + 1 == tours[k].size() ? "" : " ");
                }
                os << "\n";
            }
        }
    };

    Param param;
    Changed changed;

    bool is_valid;
    ScoreType score;
    vector<vector<int>> tours;
    vector<ScoreType> dists;
    vector<int> pos_route;
    vector<int> pos_idx;

    State() {}

    void init() {
        tours.assign(K, vector<int>());
        dists.assign(K, 0);
        pos_route.assign(N, -1);
        pos_idx.assign(N, -1);

        // 1. K-means にかける都市（デポ以外）の座標リストを作成
        vector<ElmType> city_coords;
        vector<int> city_ids;
        vector<bool> is_depot(N, false);
        for (int sn : START_NODES) {
            if (sn < N) is_depot[sn] = true;
        }

        for (int i = 0; i < N; ++i) {
            if (!is_depot[i]) {
                city_coords.push_back(YX[i]);
                city_ids.push_back(i);
            }
        }

        // 2. デポの座標を初期中心として設定
        vector<ElmType> init_centers;
        for (int sn : START_NODES) {
            init_centers.push_back(YX[sn]);
        }

        // 3. K-means 実行
        titan23::Kmeans<long double, ElmType, kmeans_dist, kmeans_mean> kmeans(K, 20);
        auto [labels, final_centers] = kmeans.fit(city_coords, init_centers);

        // 4. 各クラスタごとに都市を分類
        vector<vector<int>> clusters(K);
        for (int i = 0; i < (int)labels.size(); ++i) {
            clusters[labels[i]].push_back(city_ids[i]);
        }

        // 5. 各ルート内で貪欲法（Nearest Neighbor）を適用
        score = 0;
        for (int k = 0; k < K; ++k) {
            int start_node = START_NODES[k];
            tours[k].push_back(start_node);

            vector<int>& cluster_cities = clusters[k];
            vector<bool> visited(cluster_cities.size(), false);
            int current_node = start_node;

            // 未訪問の都市がなくなるまで最も近い都市を選択
            for (int i = 0; i < (int)cluster_cities.size(); ++i) {
                int next_city_idx = -1;
                ll min_d = LLONG_MAX;

                for (int j = 0; j < (int)cluster_cities.size(); ++j) {
                    if (visited[j]) continue;
                    ll d = DIST[current_node][cluster_cities[j]];
                    if (d < min_d) {
                        min_d = d;
                        next_city_idx = j;
                    }
                }

                if (next_city_idx != -1) {
                    visited[next_city_idx] = true;
                    current_node = cluster_cities[next_city_idx];
                    tours[k].push_back(current_node);
                }
            }

            // インデックスの更新とルート距離の計算
            ll d = 0;
            int sz = tours[k].size();
            for (int i = 0; i < sz; ++i) {
                int u = tours[k][i];
                int v = tours[k][(i + 1) % sz];
                pos_route[u] = k;
                pos_idx[u] = i;
                d += DIST[u][v];
            }
            dists[k] = d;
            score += d * d;
        }
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const {
        ll max_d = 0;
        rep(k, K) max_d = max(max_d, dists[k]);
        return max_d;
    }

    // thresholdを超えたら必ずreject(同じなら遷移する)
    // is_validをfalseにすると必ずrejectする
    // progress:焼きなまし進行度 0.0~1.0 まで
    void modify(const ScoreType threshold, const double progress) {
        if (N <= 3) {
            is_valid = false;
            return;
        }

        int prob_intra;
        if (K == 1) {
            prob_intra = 100;
        } else {
            double avg_nodes = (double)N / K;
            if (avg_nodes < 10.0) {
                prob_intra = 40;
            } else if (avg_nodes < 30.0) {
                prob_intra = 60;
            } else {
                prob_intra = 80;
            }
        }
        // modify関数内での確率判定
        int rand_val = sarnd.randrange(100);
        if (rand_val < prob_intra) {
            int intra_rand = sarnd.randrange(100);
            if (intra_rand < 50) changed.type = 0;       // Intra 2-opt
            else if (intra_rand < 67) changed.type = 1;  // Intra Or-opt
            else if (intra_rand < 92) changed.type = 2;  // Intra 反転Or-opt
            else changed.type = 3;                       // Intra Double Bridge
        } else {
            int inter_rand = sarnd.randrange(100);
            if (inter_rand < 50) changed.type = 4;       // Inter Block Shift
            else changed.type = 5;                       // Inter Block Swap
        }

        if (changed.type >= 0 && changed.type <= 3) {
            // ====== Intra 近傍 (既存ロジックのmTSP移植) ======
            int r1 = sarnd.randrange(K);
            auto& tr = tours[r1];
            int sz = tr.size();
            if (sz <= 3) { is_valid = false; return; }
            changed.r1 = r1;

            if (changed.type == 0) { // 2-opt
                int u_idx = sarnd.randrange(sz - 1) + 1; // 始点0を保護
                int u_prev = u_idx - 1;
                int cand = nearest_neighbors[tr[u_prev]][sarnd.randrange(NEIBHORS)];

                if (pos_route[cand] != r1) { is_valid = false; return; }
                int v_idx = pos_idx[cand];
                if (v_idx == 0) { is_valid = false; return; } // 始点0を保護

                if (u_idx == v_idx || v_idx == u_prev || v_idx == (u_idx - 2 + sz) % sz) {
                    is_valid = false; return;
                }
                int real_u = u_idx, real_v = v_idx;
                if (u_idx > v_idx) { real_u = v_idx + 1; real_v = u_idx - 1; }
                if (real_u <= 0) { is_valid = false; return; } // 始点0を保護

                changed.u_idx = real_u; changed.v_idx = real_v;
                int real_u_prev = real_u - 1, real_v_next = (real_v + 1) % sz;

                ll removed_dist = DIST[tr[real_u_prev]][tr[real_u]] + DIST[tr[real_v]][tr[real_v_next]];
                ll added_dist = DIST[tr[real_u_prev]][tr[real_v]] + DIST[tr[real_u]][tr[real_v_next]];

                changed.diff_d1 = added_dist - removed_dist;
                ll old_d = dists[r1];
                changed.diff = (old_d + changed.diff_d1) * (old_d + changed.diff_d1) - old_d * old_d;
                score += changed.diff;

            } else if (changed.type == 1 || changed.type == 2) { // Or-opt / 反転Or-opt
                int L = (changed.type == 1) ? (sarnd.randrange(3) + 1) : (sarnd.randrange(2) + 2);
                int u_idx = sarnd.randrange(sz - 1) + 1; // 始点0を保護
                if (u_idx + L > sz) { is_valid = false; return; } // 配列末尾跨ぎを禁止

                int target_node = (changed.type == 1) ? tr[u_idx] : tr[(u_idx + L - 1) % sz];
                int cand = nearest_neighbors[target_node][sarnd.randrange(NEIBHORS)];

                // ★candが別ルートにいる場合は棄却
                if (pos_route[cand] != r1) { is_valid = false; return; }
                int v_idx = pos_idx[cand];
                if (v_idx == 0 || (v_idx >= u_idx && v_idx < u_idx + L) || v_idx == u_idx - 1) {
                    is_valid = false; return;
                }

                changed.u_idx = u_idx; changed.v_idx = v_idx; changed.L1 = L;
                int u_prev = u_idx - 1, u_last = u_idx + L - 1;
                int u_next = (u_idx + L) % sz, v_next = (v_idx + 1) % sz;

                ll removed_dist = DIST[tr[u_prev]][tr[u_idx]] + DIST[tr[u_last]][tr[u_next]] + DIST[tr[v_idx]][tr[v_next]];
                ll added_dist;
                if (changed.type == 1) {
                    added_dist = DIST[tr[u_prev]][tr[u_next]] + DIST[tr[v_idx]][tr[u_idx]] + DIST[tr[u_last]][tr[v_next]];
                } else {
                    added_dist = DIST[tr[u_prev]][tr[u_next]] + DIST[tr[v_idx]][tr[u_last]] + DIST[tr[u_idx]][tr[v_next]];
                }

                changed.diff_d1 = added_dist - removed_dist;
                ll old_d = dists[r1];
                changed.diff = (old_d + changed.diff_d1) * (old_d + changed.diff_d1) - old_d * old_d;
                score += changed.diff;

            } else if (changed.type == 3) { // Double Bridge
                if (sz <= 4) { is_valid = false; return; }
                int p[4];
                p[0] = sarnd.randrange(sz - 1) + 1; // 始点0を保護
                p[1] = sarnd.randrange(sz - 1) + 1;
                p[2] = sarnd.randrange(sz - 1) + 1;
                p[3] = sarnd.randrange(sz - 1) + 1;
                sort(p, p + 4);
                if (p[0] == p[1] || p[1] == p[2] || p[2] == p[3]) { is_valid = false; return; }

                changed.u_idx = p[0]; changed.v_idx = p[1];
                changed.L1 = p[2]; changed.w_idx = p[3];
                int a1 = p[0], a2 = (p[0] + 1) % sz;
                int b1 = p[1], b2 = (p[1] + 1) % sz;
                int c1 = p[2], c2 = (p[2] + 1) % sz;
                int d1 = p[3], d2 = (p[3] + 1) % sz;

                ll removed_dist = DIST[tr[a1]][tr[a2]] + DIST[tr[b1]][tr[b2]]
                                + DIST[tr[c1]][tr[c2]] + DIST[tr[d1]][tr[d2]];
                ll added_dist   = DIST[tr[a1]][tr[c2]] + DIST[tr[d1]][tr[b2]]
                                + DIST[tr[c1]][tr[a2]] + DIST[tr[b1]][tr[d2]];

                changed.diff_d1 = added_dist - removed_dist;
                ll old_d = dists[r1];
                changed.diff = (old_d + changed.diff_d1) * (old_d + changed.diff_d1) - old_d * old_d;
                score += changed.diff;
            }

        } else if (changed.type == 4) {
            // ====== Inter 近傍: Block Shift ======
            int r1 = sarnd.randrange(K), r2 = sarnd.randrange(K);
            if (r1 == r2 || tours[r1].size() <= 1) { is_valid = false; return; }
            int sz1 = tours[r1].size(), sz2 = tours[r2].size();

            int L = sarnd.randrange(min(3, sz1 - 1)) + 1;
            int u_idx = sarnd.randrange(sz1 - L) + 1; // 始点0を保護
            int w_idx = sarnd.randrange(sz2);

            changed.r1 = r1; changed.r2 = r2;
            changed.u_idx = u_idx; changed.L1 = L; changed.w_idx = w_idx;

            int v_idx = u_idx + L - 1;
            int u_prev = u_idx - 1, v_next = (v_idx + 1) % sz1;
            int u_node = tours[r1][u_idx], v_node = tours[r1][v_idx];
            int w_node = tours[r2][w_idx], w_next_node = tours[r2][(w_idx + 1) % sz2];
            ll block_internal = 0;
            for (int i = u_idx; i < v_idx; ++i) {
                block_internal += DIST[tours[r1][i]][tours[r1][i + 1]];
            }
            ll removed1 = DIST[tours[r1][u_prev]][u_node] + DIST[v_node][tours[r1][v_next]];
            ll added1   = DIST[tours[r1][u_prev]][tours[r1][v_next]];
            changed.diff_d1 = added1 - removed1 - block_internal;

            ll removed2 = DIST[w_node][w_next_node];
            ll added2   = DIST[w_node][u_node] + DIST[v_node][w_next_node];
            changed.diff_d2 = added2 - removed2 + block_internal;

            ll old_d1 = dists[r1], old_d2 = dists[r2];
            changed.diff = ((old_d1 + changed.diff_d1) * (old_d1 + changed.diff_d1) - old_d1 * old_d1) +
                           ((old_d2 + changed.diff_d2) * (old_d2 + changed.diff_d2) - old_d2 * old_d2);
            score += changed.diff;

        } else if (changed.type == 5) {
            // ====== Inter 近傍: Block Swap ======
            int r1 = sarnd.randrange(K), r2 = sarnd.randrange(K);
            if (r1 == r2 || tours[r1].size() <= 1 || tours[r2].size() <= 1) { is_valid = false; return; }
            int sz1 = tours[r1].size(), sz2 = tours[r2].size();

            int L1 = sarnd.randrange(min(3, sz1 - 1)) + 1;
            int u1_idx = sarnd.randrange(sz1 - L1) + 1; // 始点0を保護

            int L2 = sarnd.randrange(min(3, sz2 - 1)) + 1;
            int u2_idx = sarnd.randrange(sz2 - L2) + 1; // 始点0を保護

            changed.r1 = r1; changed.r2 = r2;
            changed.u_idx = u1_idx; changed.L1 = L1;
            changed.w_idx = u2_idx; changed.L2 = L2;

            int v1_idx = u1_idx + L1 - 1, v2_idx = u2_idx + L2 - 1;
            int u1_prev = u1_idx - 1, v1_next = (v1_idx + 1) % sz1;
            int u2_prev = u2_idx - 1, v2_next = (v2_idx + 1) % sz2;

            int u1 = tours[r1][u1_idx], v1 = tours[r1][v1_idx];
            int u2 = tours[r2][u2_idx], v2 = tours[r2][v2_idx];
            ll block_internal1 = 0;
            for (int i = u1_idx; i < v1_idx; ++i) {
                block_internal1 += DIST[tours[r1][i]][tours[r1][i + 1]];
            }
            ll block_internal2 = 0;
            for (int i = u2_idx; i < v2_idx; ++i) {
                block_internal2 += DIST[tours[r2][i]][tours[r2][i + 1]];
            }
            ll removed1 = DIST[tours[r1][u1_prev]][u1] + DIST[v1][tours[r1][v1_next]];
            ll removed2 = DIST[tours[r2][u2_prev]][u2] + DIST[v2][tours[r2][v2_next]];

            ll added1 = DIST[tours[r1][u1_prev]][u2] + DIST[v2][tours[r1][v1_next]];
            ll added2 = DIST[tours[r2][u2_prev]][u1] + DIST[v1][tours[r2][v2_next]];

            changed.diff_d1 = added1 - removed1 + (block_internal2 - block_internal1);
            changed.diff_d2 = added2 - removed2 + (block_internal1 - block_internal2);

            ll old_d1 = dists[r1], old_d2 = dists[r2];
            changed.diff = ((old_d1 + changed.diff_d1) * (old_d1 + changed.diff_d1) - old_d1 * old_d1) +
                           ((old_d2 + changed.diff_d2) * (old_d2 + changed.diff_d2) - old_d2 * old_d2);
            score += changed.diff;
        }
    }

    void rollback() {
        if (!is_valid) return;
        score -= changed.diff;
    }

    void advance() {
        if (changed.type == 0) {
            int r = changed.r1;
            reverse(tours[r].begin() + changed.u_idx, tours[r].begin() + changed.v_idx + 1);
            for (int i = changed.u_idx; i <= changed.v_idx; ++i) pos_idx[tours[r][i]] = i;
            dists[r] += changed.diff_d1;
        } else if (changed.type == 1) {
            int r = changed.r1, u = changed.u_idx, v = changed.v_idx, L = changed.L1;
            if (u < v) {
                rotate(tours[r].begin() + u, tours[r].begin() + u + L, tours[r].begin() + v + 1);
                for (int i = u; i <= v; ++i) pos_idx[tours[r][i]] = i;
            } else {
                rotate(tours[r].begin() + v + 1, tours[r].begin() + u, tours[r].begin() + u + L);
                for (int i = v + 1; i < u + L; ++i) pos_idx[tours[r][i]] = i;
            }
            dists[r] += changed.diff_d1;
        } else if (changed.type == 2) {
            int r = changed.r1, u = changed.u_idx, v = changed.v_idx, L = changed.L1;
            reverse(tours[r].begin() + u, tours[r].begin() + u + L);
            if (u < v) {
                rotate(tours[r].begin() + u, tours[r].begin() + u + L, tours[r].begin() + v + 1);
                for (int i = u; i <= v; ++i) pos_idx[tours[r][i]] = i;
            } else {
                rotate(tours[r].begin() + v + 1, tours[r].begin() + u, tours[r].begin() + u + L);
                for (int i = v + 1; i < u + L; ++i) pos_idx[tours[r][i]] = i;
            }
            dists[r] += changed.diff_d1;
        } else if (changed.type == 3) {
            int r = changed.r1;
            int p1 = changed.u_idx, p2 = changed.v_idx, p3 = changed.L1, p4 = changed.w_idx;
            vector<int> new_tour;
            int sz = tours[r].size();
            new_tour.reserve(sz);
            for(int i = 0; i <= p1; ++i) new_tour.push_back(tours[r][i]);
            for(int i = p3 + 1; i <= p4; ++i) new_tour.push_back(tours[r][i]);
            for(int i = p2 + 1; i <= p3; ++i) new_tour.push_back(tours[r][i]);
            for(int i = p1 + 1; i <= p2; ++i) new_tour.push_back(tours[r][i]);
            for(int i = p4 + 1; i < sz; ++i) new_tour.push_back(tours[r][i]);

            tours[r] = std::move(new_tour);
            for(int i = 0; i < sz; ++i) pos_idx[tours[r][i]] = i;
            dists[r] += changed.diff_d1;
        } else if (changed.type == 4) {
            int r1 = changed.r1, r2 = changed.r2;
            int u = changed.u_idx, L = changed.L1, w = changed.w_idx;

            vector<int> block(tours[r1].begin() + u, tours[r1].begin() + u + L);
            tours[r1].erase(tours[r1].begin() + u, tours[r1].begin() + u + L);
            tours[r2].insert(tours[r2].begin() + w + 1, block.begin(), block.end());

            dists[r1] += changed.diff_d1;
            dists[r2] += changed.diff_d2;
            for(int i = u; i < tours[r1].size(); ++i) pos_idx[tours[r1][i]] = i;
            for(int i = w + 1; i < tours[r2].size(); ++i) {
                pos_route[tours[r2][i]] = r2;
                pos_idx[tours[r2][i]] = i;
            }
        } else if (changed.type == 5) {
            int r1 = changed.r1, r2 = changed.r2;
            int u1 = changed.u_idx, L1 = changed.L1, u2 = changed.w_idx, L2 = changed.L2;

            vector<int> b1(tours[r1].begin() + u1, tours[r1].begin() + u1 + L1);
            vector<int> b2(tours[r2].begin() + u2, tours[r2].begin() + u2 + L2);

            tours[r1].erase(tours[r1].begin() + u1, tours[r1].begin() + u1 + L1);
            tours[r1].insert(tours[r1].begin() + u1, b2.begin(), b2.end());

            tours[r2].erase(tours[r2].begin() + u2, tours[r2].begin() + u2 + L2);
            tours[r2].insert(tours[r2].begin() + u2, b1.begin(), b1.end());

            dists[r1] += changed.diff_d1;
            dists[r2] += changed.diff_d2;
            for(int i = u1; i < tours[r1].size(); ++i) { pos_route[tours[r1][i]] = r1; pos_idx[tours[r1][i]] = i; }
            for(int i = u2; i < tours[r2].size(); ++i) { pos_route[tours[r2][i]] = r2; pos_idx[tours[r2][i]] = i; }
        }
    }

    Result get_result() const {
        return Result(get_score(), get_true_score(), tours);
    }
};
}

void input() {
    string token;
    while (cin >> token) {
        if (token == "DIMENSION" || token == "DIMENSION:") {
            if (token == "DIMENSION") {
                string next_token;
                cin >> next_token;
                if (next_token == ":") {
                    cin >> N;
                } else {
                    N = stoi(next_token);
                }
            } else {
                cin >> N;
            }
        } else if (token == "NODE_COORD_SECTION") {
            break;
        }
    }

    YX.resize(N);
    for (int i = 0; i < N; ++i) {
        int id;
        double x, y;
        cin >> id >> x >> y;
        id--;
        YX[id] = {x, y};
    }

    K = 10;
    START_NODES.resize(K);
    rep(k, K) {
        START_NODES[k] = N+k;
    }
    YX.resize(N+K);
    rep(k, K) {
        YX[N+k] = YX[N/K*k];
    }
    N += K;
}

void solve() {
    sa::sa_init();
    sa::State::Result result = sa::sa_run<sa::State>(10000, true);
    // OMP_NUM_THREADS=32 time ./a.out < fnl4461.tsp > out.txt
    // sa::State::Result result = sa::replica_run<sa::State>(10000, 32, 100, true);
    result.print();
    cerr << "Score = " << result.score << endl;
    cerr << "Score = " << result.true_score << endl;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(3);
    cerr << fixed << setprecision(3);
    input();
    solve();
    return 0;
}

// 50778
