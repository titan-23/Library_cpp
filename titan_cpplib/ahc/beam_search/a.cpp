#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/hash_policy.hpp>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"
using namespace std;

#define rep(i, n) for (int i = 0; i < (n); ++i)
template<typename T> T abs(const T a, const T b) { return a > b ? a-b : b-a; }

int N;
vector<vector<int>> A;

void input() {
    cin >> N;
    A.resize(N, vector<int>(N));
    rep(i, N) rep(j, N) {
        cin >> A[i][j];
    }
}

//! 木上のビームサーチライブラリ
namespace beam_search {

using ScoreType = int;
using HashType = unsigned long long;
const ScoreType INF = 1e9;
titan23::Random brnd;
HashType zhs[10][10][101]; // zhs[i][j][k]:=(i,j)にkがあるときのハッシュ
int revB[101];
HashType GOAL_HASH;

void beam_init() {
    rep(i, N) rep(j, N) rep(k, N*N+1) {
        zhs[i][j][k] = brnd.rand_u64();
    }
    rep(v, N*N) {
        revB[v] = ((v-1)%N)*N + ((v-1)/N);
    }
    revB[0] = 0;
}

// メモリ量は少ない方がよく、score,hash のメモは無くしたい
// compose 対応: chain に「2 ステップ目以降」を格納する。各 step は rollback の
// 中間状態復元用に pre_* をキャッシュ済み。primary (d, pre_*, nxt_*) は
// (chain 空のとき)1 ステップ、(chain あり)先頭ステップを表す。nxt_* は
// 常に "composed 全体を適用したあとの状態" を表す。
struct ChainStep {
    char d;
    ScoreType pre_score;
    HashType pre_hash;
    int pre_inv_ud;
    int pre_inv_lr;
};

struct Action {
    char d;
    ScoreType pre_score, nxt_score;
    HashType pre_hash, nxt_hash;
    int pre_inv_ud, nxt_inv_ud;
    int pre_inv_lr, nxt_inv_lr;
    vector<ChainStep> chain;

    Action() {}
    Action(char d) : d(d), pre_score(INF), nxt_score(INF), pre_hash(0), nxt_hash(0) {}
    friend ostream& operator<<(ostream& os, const Action &action) {
        os << action.d;
        for (auto &s : action.chain) os << s.d;
        return os;
    }

    bool compose(Action &nxt) {
        chain.reserve(chain.size() + 1 + nxt.chain.size());
        ChainStep s;
        s.d = nxt.d;
        s.pre_score = nxt.pre_score;
        s.pre_hash = nxt.pre_hash;
        s.pre_inv_ud = nxt.pre_inv_ud;
        s.pre_inv_lr = nxt.pre_inv_lr;
        chain.push_back(s);
        for (auto &cs : nxt.chain) chain.push_back(cs);
        nxt_score = nxt.nxt_score;
        nxt_hash = nxt.nxt_hash;
        nxt_inv_ud = nxt.nxt_inv_ud;
        nxt_inv_lr = nxt.nxt_inv_lr;
        return true;
    }

    string to_string() const {
        string r(1, d);
        for (auto &s : chain) r.push_back(s.d);
        return r;
    }
};

class State {
private:
    ScoreType score;
    HashType hash;

    vector<vector<int>> F;
    int y, x;
    int inv_ud, inv_lr;

    // (i, j) に v があるときのスコア
    int calc_pos(const int i, const int j, const int v) const {
        if (v == 0) return 0;
        int s = abs(i-((v-1)/N)) + abs(j-((v-1)%N));
        return s;
    }

    int get_inversion_ud() const {
        int cnt = 0;
        rep(i, N * N) {
            int y1 = i / N;
            int x1 = i % N;
            if (F[y1][x1] == 0) continue;
            for (int j = i + 1; j < N * N; ++j) {
                int y2 = j / N;
                int x2 = j % N;
                if (F[y2][x2] == 0) continue;
                if (F[y1][x1] > F[y2][x2]) {
                    ++cnt;
                }
            }
        }
        return cnt;
    }

    int get_inversion_lr() const {
        int cnt = 0;
        rep(i, N * N) {
            int y1 = i % N;
            int x1 = i / N;
            if (F[y1][x1] == 0) continue;
            for (int j = i + 1; j < N * N; ++j) {
                int y2 = j % N;
                int x2 = j / N;
                if (F[y2][x2] == 0) continue;
                if (revB[F[y1][x1]] > revB[F[y2][x2]]) {
                    ++cnt;
                }
            }
        }
        return cnt;
    }

public:
    void init() {
        this->score = 0;
        this->hash = 0;

        F = A;
        rep(i, N) rep(j, N) {
            if (F[i][j] == 0) {
                y = i; x = j;
            }
            score += calc_pos(i, j, F[i][j]);
            hash ^= zhs[i][j][F[i][j]];
        }

        GOAL_HASH = 0;
        rep(i, N) rep(j, N) {
            if (i == N-1 && j == N-1) {
                GOAL_HASH ^= zhs[i][j][0];
            } else {
                GOAL_HASH ^= zhs[i][j][i*N+j+1];
            }
        }

        inv_ud = get_inversion_ud();
        inv_lr = get_inversion_lr();
        int inv_dist = inv_ud/(N-1)+inv_ud%(N-1) + inv_lr/(N-1)+inv_lr%(N-1);
        score += inv_dist;
    }

    //! ロールバックに必要な情報はすべてactionにメモしておく
    //! threshold以上であれば計算しなくてよい
    //! INFを返すと無条件で採用しない
    tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const {
        action.pre_score = score;
        action.pre_hash = hash;
        action.pre_inv_ud = inv_ud;
        action.pre_inv_lr = inv_lr;

        int ny = y, nx = x;
        if (action.d == 'D') ++ny;
        if (action.d == 'U') --ny;
        if (action.d == 'R') ++nx;
        if (action.d == 'L') --nx;

        ScoreType nxt_score = score;
        HashType nxt_hash = hash;
        int nxt_inv_ud = inv_ud;
        int nxt_inv_lr = inv_lr;

        int pre_inv_dist = inv_ud/(N-1)+inv_ud%(N-1)+inv_lr/(N-1)+inv_lr%(N-1);
        nxt_score -= pre_inv_dist;
        nxt_score -= calc_pos(y, x, F[y][x]);
        nxt_score -= calc_pos(ny, nx, F[ny][nx]);

        nxt_score += calc_pos(y, x, F[ny][nx]);
        nxt_score += calc_pos(ny, nx, F[y][x]);

        if (nxt_score >= threshold) {
            return {INF, 0, 0};
        }

        nxt_hash ^= zhs[y][x][F[y][x]];
        nxt_hash ^= zhs[y][x][F[ny][nx]];
        nxt_hash ^= zhs[ny][nx][F[ny][nx]];
        nxt_hash ^= zhs[ny][nx][F[y][x]];

        if (action.d == 'D') {
            int fn = F[ny][nx];
            for (int ind = y * N + x + 1; ind < ny * N + nx; ++ind) {
                if (F[ind / N][ind % N] > fn) --nxt_inv_ud;
                else ++nxt_inv_ud;
            }
        } else if (action.d == 'R') {
            int fn = revB[F[ny][nx]];
            for (int ind = x * N + y + 1; ind < nx * N + ny; ++ind) {
                if (revB[F[ind % N][ind / N]] > fn) --nxt_inv_lr;
                else ++nxt_inv_lr;
            }
        } else if (action.d == 'U') {
            int fn = F[ny][nx];
            for (int ind = ny * N + nx + 1; ind < y * N + x; ++ind) {
                if (F[ind / N][ind % N] < fn) --nxt_inv_ud;
                else ++nxt_inv_ud;
            }
        } else if (action.d == 'L') {
            int fn = revB[F[ny][nx]];
            for (int ind = nx * N + ny + 1; ind < x * N + y; ++ind) {
                if (revB[F[ind % N][ind / N]] < fn) --nxt_inv_lr;
                else ++nxt_inv_lr;
            }
        }
        int nxt_inv_dist = nxt_inv_ud/(N-1) + nxt_inv_ud%(N-1) + nxt_inv_lr/(N-1) + nxt_inv_lr%(N-1);
        nxt_score += nxt_inv_dist;

        action.nxt_score = nxt_score;
        action.nxt_hash = nxt_hash;
        action.nxt_inv_ud = nxt_inv_ud;
        action.nxt_inv_lr = nxt_inv_lr;

        return {nxt_score, nxt_hash, nxt_hash == GOAL_HASH};
    }

    void swap_one_forward(char d) {
        int py = y, px = x;
        if (d == 'D') ++y;
        if (d == 'U') --y;
        if (d == 'R') ++x;
        if (d == 'L') --x;
        swap(F[y][x], F[py][px]);
    }

    void swap_one_back(char d) {
        int py = y, px = x;
        if (d == 'D') --y;
        if (d == 'U') ++y;
        if (d == 'R') --x;
        if (d == 'L') ++x;
        swap(F[y][x], F[py][px]);
    }

    void apply_op(const Action &action) {
        swap_one_forward(action.d);
        for (auto &cs : action.chain) swap_one_forward(cs.d);
        inv_lr = action.nxt_inv_lr;
        inv_ud = action.nxt_inv_ud;
        score = action.nxt_score;
        hash = action.nxt_hash;
    }

    void rollback(const Action &action) {
        for (auto it = action.chain.rbegin(); it != action.chain.rend(); ++it) {
            swap_one_back(it->d);
        }
        swap_one_back(action.d);
        inv_lr = action.pre_inv_lr;
        inv_ud = action.pre_inv_ud;
        score = action.pre_score;
        hash = action.pre_hash;
    }

    //! 新方式: enumerate_actions / try_op 一体化 sink。
    //! 生成した action を submit に渡すと try_op + 判定 + push まで行う。中間 vector を作らない。
    //! submit.threshold() でライブ worst を取得でき早期枝刈りに使えるが、ここでは try_op 側に任せる。
    template<class Submit>
    void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const {
        // composed last_action の場合、最後に踏んだ primitive 方向は chain.back().d
        char last_d = last_action.chain.empty() ? last_action.d : last_action.chain.back().d;
        auto rev = [&] () -> char {
            if (turn == 0) return 'Z';
            if (last_d == 'U') return 'D';
            if (last_d == 'D') return 'U';
            if (last_d == 'L') return 'R';
            if (last_d == 'R') return 'L';
            assert(false);
        };
        Action a;
        const string s = "UDLR";
        for (char c : s) {
            if (c == rev()) continue;
            int ny = y, nx = x;
            if (c == 'D') ++ny;
            if (c == 'U') --ny;
            if (c == 'R') ++nx;
            if (c == 'L') --nx;
            if (0 <= ny && ny < N && 0 <= nx && nx < N) {
                a.d = c;
                submit(a);
            }
        }
    }

#if 0
    //! 旧方式: 遷移可能な Action の vector を actions に入れる
    void enumerate_actions(vector<Action> &actions, const int turn, const Action &last_action, const ScoreType threshold) const {
        auto rev = [&] () -> char {
            if (turn == 0) return 'Z';
            if (last_action.d == 'U') return 'D';
            if (last_action.d == 'D') return 'U';
            if (last_action.d == 'L') return 'R';
            if (last_action.d == 'R') return 'L';
            assert(false);
        };
        const string s = "UDLR";
        for (char c : s) {
            if (c == rev()) continue;
            int ny = y, nx = x;
            if (c == 'D') ++ny;
            if (c == 'U') --ny;
            if (c == 'R') ++nx;
            if (c == 'L') --nx;
            if (0 <= ny && ny < N && 0 <= nx && nx < N) {
                actions.push_back({c});
            }
        }
    }
#endif

    void print() const {
    }

    string get_state_info() const {
        return "{}";
    }
};

/// @brief BeamParamを返す
/// @param max_turn 最大探索ターン
/// @param beam_width ビーム幅
/// @return BeamParam
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

/// @brief BeamParamを返す
/// @param max_turn 最大探索ターン
/// @param beam_width ビーム幅
/// @param time_limit 制限時間
/// @param is_adjusting 制限時間に合わせるかどうか
/// @param clear_hash_every_turn ハッシュ辞書を毎ターンclearするかどうか
/// @return
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting=false, bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}

vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF, false> bs;
    return bs.search(param, verbose, history_file);
}
} // namespace beam_search


void solve() {
    beam_search::beam_init();
    auto param = flying_squirrel::BeamParam(1200, 1e4, 1900, false, false);
    auto ans = beam_search::search(param, true, "");
    cerr << ans.size() << endl;
    for (auto action : ans) {
        cout << action;
    }
    cout << "\n";
}

int main() {
    input();
    solve();
    return 0;
}
