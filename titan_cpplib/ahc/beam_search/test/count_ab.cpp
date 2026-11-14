/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/test/count_ab.cpp
#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

// a.cpp をもとに、走査の回数を数える計測用プログラム
// エンジンをマクロで切り替え、State 側のカウンタで
// apply/rollback の呼び出し回数と 1 手ごとの swap の回数を数える
//   -DENGINE_PLAIN   : tour エンジン + compose 無効 (普通の差分更新型と同じ)
//   -DENGINE_COMPOSE : tour エンジン + compose (beam_search_compose.cpp)
//   -DENGINE_RADIX   : 圧縮辺木 (beam_search_radix.cpp)

#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/hash_policy.hpp>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/state_pool.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/alg/random.cpp"
#if defined(ENGINE_RADIX)
#include "titan_cpplib/ahc/beam_search/beam_search_radix.cpp"
#else
#include "titan_cpplib/ahc/beam_search/beam_search_compose.cpp"
#endif
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

// State 側の計測カウンタ。エンジンによらない実際の仕事量を表す
//   g_tryop          : try_op の呼び出し回数
//   g_apply_calls    : apply_op の呼び出し回数。合成 action は 1 回と数える
//   g_rollback_calls : rollback の呼び出し回数
//   g_swap_fwd/back  : 1 手ごとの swap の回数。合成 action は構成する手の数だけ数える
long long g_tryop = 0;
long long g_apply_calls = 0, g_rollback_calls = 0;
long long g_swap_fwd = 0, g_swap_back = 0;

/// 木上のビームサーチライブラリ
namespace beam_search {

using ScoreType = int;
using HashType = unsigned long long;
const ScoreType INF = 1e9;
titan23::Random brnd;
HashType zhs[10][10][101]; // zhs[i][j][k]:=(i,j)にkがあるときのハッシュ
int revB[101];
HashType GOAL_HASH;

/// @brief ハッシュ用の乱数表と、各値の目標位置を列優先で数えた表 revB を作る
void beam_init() {
    rep(i, N) rep(j, N) rep(k, N*N+1) {
        zhs[i][j][k] = brnd.rand_u64();
    }
    rep(v, N*N) {
        revB[v] = ((v-1)%N)*N + ((v-1)/N);
    }
    revB[0] = 0;
}

/// @brief 合成した Action の 2 手目以降の 1 手 / 途中の状態に戻すための pre_* を持つ
struct ChainStep {
    char d;
    ScoreType pre_score;
    HashType pre_hash;
    int pre_inv_ud;
    int pre_inv_lr;
};

/// @brief 空きマスを 1 方向に動かす操作 / 合成すると 2 手目以降を chain に持つ
/// nxt_* は合成した手をすべて適用した後の状態を表す
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

    /// @brief nxt をこの操作の後ろに合成する / ENGINE_PLAIN では合成せず false を返す
    bool compose(Action &nxt) {
#ifdef ENGINE_PLAIN
        (void)nxt;
        return false;  // 合成しない。普通の差分更新型と同じ
#else
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
#endif
    }

    string to_string() const {
        string r(1, d);
        for (auto &s : chain) r.push_back(s.d);
        return r;
    }
};

/// @brief N×N のスライドパズルの盤面を持つ状態
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
    /// @brief 入力の盤面 A で初期化する
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

    /// @brief action を適用した後の評価値、ハッシュ値、完成したかを返す
    /// 戻すための情報はすべて action に記録する。threshold 以上と分かった時点で INF を返す
    tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const {
        ++g_tryop;
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

    /// @brief 空きマスを d の向きに 1 マス動かす
    void swap_one_forward(char d) {
        ++g_swap_fwd;
        int py = y, px = x;
        if (d == 'D') ++y;
        if (d == 'U') --y;
        if (d == 'R') ++x;
        if (d == 'L') --x;
        swap(F[y][x], F[py][px]);
    }

    /// @brief 空きマスを d と逆の向きに 1 マス動かす
    void swap_one_back(char d) {
        ++g_swap_back;
        int py = y, px = x;
        if (d == 'D') --y;
        if (d == 'U') ++y;
        if (d == 'R') --x;
        if (d == 'L') ++x;
        swap(F[y][x], F[py][px]);
    }

    /// @brief action を適用する
    void apply_op(const Action &action) {
        ++g_apply_calls;
        swap_one_forward(action.d);
        for (auto &cs : action.chain) swap_one_forward(cs.d);
        inv_lr = action.nxt_inv_lr;
        inv_ud = action.nxt_inv_ud;
        score = action.nxt_score;
        hash = action.nxt_hash;
    }

    /// @brief action を戻す。合成された手は逆順に戻す
    void rollback(const Action &action) {
        ++g_rollback_calls;
        for (auto it = action.chain.rbegin(); it != action.chain.rend(); ++it) {
            swap_one_back(it->d);
        }
        swap_one_back(action.d);
        inv_lr = action.pre_inv_lr;
        inv_ud = action.pre_inv_ud;
        score = action.pre_score;
        hash = action.pre_hash;
    }

    /// @brief 遷移可能な action を submit に渡す
    template<class Submit>
    void enumerate_actions(const int turn, const Action &last_action, Submit &&submit) const {
        // 合成された last_action では、最後の 1 手の方向が chain.back().d にある
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

    void print() const {
    }

    string get_state_info() const {
        return "{}";
    }
};

/// @brief 最大ターン数とビーム幅から BeamParam を返す
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

/// @brief 最大ターン数、ビーム幅、制限時間などから BeamParam を返す
flying_squirrel::BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting=false, bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}

using Result = flying_squirrel::BeamResult<ScoreType, Action, State>;

/// @brief param で探索して結果を返す
template<bool materialize_final_state=true>
Result search(flying_squirrel::BeamParam &param, const bool verbose=false) {
#if defined(ENGINE_RADIX)
    flying_squirrel::BeamSearchRadix<ScoreType, HashType, Action, State, INF> bs;
    return bs.search<materialize_final_state>(param, verbose);
#else
    flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF, false> bs;
    return bs.search<materialize_final_state>(param, verbose, "");
#endif
}
} // namespace beam_search


/// @brief マクロで選んだエンジンの名前を返す
const char* engine_name() {
#if defined(ENGINE_RADIX)
    return "radix";
#elif defined(ENGINE_PLAIN)
    return "plain(tour, no-compose)";
#else
    return "compose(tour)";
#endif
}

void solve() {
    beam_search::beam_init();
    auto param = flying_squirrel::BeamParam(1200, 1e4, 1900, false, false);
    auto result = beam_search::search<false>(param, true);
    auto &ans = result.actions;
    cerr << ans.size() << endl;
    long long primitive = 0;
    for (auto &action : ans) primitive += 1 + (long long)action.chain.size();
    for (auto action : ans) {
        cout << action;
    }
    cout << "\n";

    cerr << "[state counters] engine=" << engine_name() << endl;
    cerr << "  try_op         = " << g_tryop << endl;
    cerr << "  apply_calls    = " << g_apply_calls
         << "  rollback_calls = " << g_rollback_calls << endl;
    cerr << "  swap_fwd       = " << g_swap_fwd
         << "  swap_back      = " << g_swap_back << endl;
    cerr << "  answer actions = " << ans.size()
         << "  primitive moves = " << primitive << endl;
}

int main() {
    input();
    solve();
    return 0;
}
