// OMP_NUM_THREADS=8 time ./a.out < in/0000.txt > out.txt
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"
using namespace std;
using ll = long long;


int N;
vector<pair<ll, ll>> YX;
vector<vector<ll>> DIST;
vector<vector<int>> nearest_neighbors;
int K_CAND;

// minimize SA
namespace sa {

thread_local titan23::Random sarnd;

void sa_init() {
    DIST.assign(N, vector<ll>(N, 0.0));
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            double dx = YX[i].first - YX[j].first;
            double dy = YX[i].second - YX[j].second;
            DIST[i][j] = (ll)(sqrt(dx * dx + dy * dy) + 0.5);
        }
    }

    K_CAND = min(N - 1, 10);
    nearest_neighbors.assign(N, vector<int>(K_CAND));
    for (int i = 0; i < N; ++i) {
        vector<pair<ll, int>> dist_idx;
        for (int j = 0; j < N; ++j) {
            if (i != j) dist_idx.push_back({DIST[i][j], j});
        }
        sort(dist_idx.begin(), dist_idx.end());
        for (int k = 0; k < K_CAND; ++k) {
            nearest_neighbors[i][k] = dist_idx[k].second;
        }
    }
}

class State {
public:
    using ScoreType = ll;

    struct Param {
        double start_temp = 1e2;
        double end_temp = 1e0;
    };

    struct Changed {
        int TYPE_CNT = 3;
        int type;
        int u, v;
        int length;
        ScoreType diff;
        Changed() {}
    };

    struct Result {
        ScoreType score, true_score;
        vector<int> tour;
        Result() {}
        Result(ScoreType s, ScoreType ts, const vector<int> &t) : score(s), true_score(ts), tour(t) {}
        void print(ostream &os = cout) const {
            for (int i = 0; i < N; ++i) {
                os << tour[i] << (i == N - 1 ? "" : " ");
            }
            os << "\n";
        }
    };

    Param param;
    Changed changed;

    bool is_valid;
    ScoreType score;
    vector<int> tour, pos;

    State() {}

    void init() {
        tour.resize(N);
        pos.resize(N);
        vector<bool> visited(N, false);
        int now = sarnd.randrange(N);
        tour[0] = now;
        visited[now] = true;
        score = 0;
        for (int i = 1; i < N; ++i) {
            int next_node = -1;
            ll min_dist = -1;
            for (int j = 0; j < N; ++j) {
                if (visited[j]) continue;
                if (next_node == -1 || DIST[now][j] < min_dist) {
                    next_node = j;
                    min_dist = DIST[now][j];
                }
            }
            tour[i] = next_node;
            visited[next_node] = true;
            score += min_dist;
            now = next_node;
        }
        score += DIST[tour[N - 1]][tour[0]];

        for (int i = 0; i < N; ++i) {
            pos[tour[i]] = i;
        }
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const {
        ScoreType ts = 0;
        for (int i = 0; i < N; ++i) {
            ts += DIST[tour[i]][tour[(i + 1) % N]];
        }
        return ts;
    }

    // thresholdを超えたら必ずreject(同じなら遷移する)
    // is_validをfalseにすると必ずrejectする
    // progress:焼きなまし進行度 0.0~1.0 まで
        void modify(const ScoreType threshold, const double progress) {
        if (N <= 3) {
            is_valid = false;
            return;
        }

        int rand_val = sarnd.randrange(10);
        if (rand_val < 5) changed.type = 0;
        else if (rand_val < 8) changed.type = 1;
        else changed.type = 2;

        if (changed.type == 0) {
            int u = sarnd.randrange(N);
            int u_prev = (u - 1 + N) % N;
            int cand = nearest_neighbors[tour[u_prev]][sarnd.randrange(K_CAND)];
            int v = pos[cand];

            if (u >= v || v - u == N - 1) {
                is_valid = false;
                return;
            }

            changed.u = u;
            changed.v = v;
            int v_next = (v + 1) % N;
            ScoreType removed_dist = DIST[tour[u_prev]][tour[u]] + DIST[tour[v]][tour[v_next]];
            ScoreType added_dist = DIST[tour[u_prev]][tour[v]] + DIST[tour[u]][tour[v_next]];
            changed.diff = added_dist - removed_dist;
            score += changed.diff;

        } else if (changed.type == 1) {
            int L = sarnd.randrange(3) + 1;
            int u = sarnd.randrange(N);
            int cand = nearest_neighbors[tour[u]][sarnd.randrange(K_CAND)];
            int v = pos[cand];

            if (u + L > N || (v >= u && v < u + L) || v == (u - 1 + N) % N) {
                is_valid = false;
                return;
            }

            changed.u = u;
            changed.v = v;
            changed.length = L;
            int u_prev = (u - 1 + N) % N;
            int u_last = (u + L - 1) % N;
            int u_next = (u + L) % N;
            int v_next = (v + 1) % N;
            ScoreType removed_dist = DIST[tour[u_prev]][tour[u]]
                                   + DIST[tour[u_last]][tour[u_next]]
                                   + DIST[tour[v]][tour[v_next]];
            ScoreType added_dist = DIST[tour[u_prev]][tour[u_next]]
                                 + DIST[tour[v]][tour[u]]
                                 + DIST[tour[u_last]][tour[v_next]];
            changed.diff = added_dist - removed_dist;
            score += changed.diff;

        } else {
            int L = sarnd.randrange(2) + 2;
            int u = sarnd.randrange(N);
            int u_last_node = tour[(u + L - 1) % N];
            int cand = nearest_neighbors[u_last_node][sarnd.randrange(K_CAND)];
            int v = pos[cand];

            if (u + L > N || (v >= u && v < u + L) || v == (u - 1 + N) % N) {
                is_valid = false;
                return;
            }

            changed.u = u;
            changed.v = v;
            changed.length = L;
            int u_prev = (u - 1 + N) % N;
            int u_last = (u + L - 1) % N;
            int u_next = (u + L) % N;
            int v_next = (v + 1) % N;
            ScoreType removed_dist = DIST[tour[u_prev]][tour[u]]
                                   + DIST[tour[u_last]][tour[u_next]]
                                   + DIST[tour[v]][tour[v_next]];
            ScoreType added_dist = DIST[tour[u_prev]][tour[u_next]]
                                 + DIST[tour[v]][tour[u_last]]
                                 + DIST[tour[u]][tour[v_next]];
            changed.diff = added_dist - removed_dist;
            score += changed.diff;
        }
    }

    // TODO
    void rollback() {
        if (!is_valid) return;
        score -= changed.diff;
    }

    void advance() {
        if (changed.type == 0) {
            reverse(tour.begin() + changed.u, tour.begin() + changed.v + 1);
        } else if (changed.type == 1) {
            int u = changed.u;
            int v = changed.v;
            int L = changed.length;
            if (u < v) {
                rotate(tour.begin() + u, tour.begin() + u + L, tour.begin() + v + 1);
            } else {
                rotate(tour.begin() + v + 1, tour.begin() + u, tour.begin() + u + L);
            }
        } else {
            int u = changed.u;
            int v = changed.v;
            int L = changed.length;
            reverse(tour.begin() + u, tour.begin() + u + L);
            if (u < v) {
                rotate(tour.begin() + u, tour.begin() + u + L, tour.begin() + v + 1);
            } else {
                rotate(tour.begin() + v + 1, tour.begin() + u, tour.begin() + u + L);
            }
        }

        for (int i = 0; i < N; ++i) {
            pos[tour[i]] = i;
        }
    }

    Result get_result() const {
        return Result(get_score(), get_true_score(), tour);
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
}

void solve() {
    sa::sa_init();
    sa::State::Result result = sa::sa_run<sa::State>(10000, true);
    result.print();
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
