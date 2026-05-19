#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")
#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/hash_policy.hpp>

#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"
using namespace std;

#define rep(i, n) for (int i = 0; i < (n); ++i)
const int dy[4] = {-1, 0, 0, 1};
const int dx[4] = {0, -1, 1, 0};

const int N = 100;
const int K = 8;
// const int K = 1;
const int H = 50;
const int W = 50;
const int T = 2500;
string R[N][H];

int ID[K];

void input() {
    int _;
    cin >> _ >> _ >> _ >> _ >> _;
    rep(i, N) rep(j, H) cin >> R[i][j];
}


//! 木上のビームサーチライブラリ
namespace beam_search { // flying squirrel over trees

using ScoreType = int;
using HashType = unsigned long long;
const ScoreType INF = 1e9;

// TODO Action
// メモリ量は少ない方がよく、score,hash のメモは無くしたい
struct Action {
    char dir = 'z';
    short is_moved = 0;
    Action() {}
    Action(char dir) : dir(dir) {}
    Action(char dir, short is_moved) : dir(dir), is_moved(is_moved) {}

    friend ostream& operator<<(ostream& os, const Action &action) {
        os << action.dir;
        return os;
    }

    string to_string() const { return ""; }
};

class State {
private:
    titan23::Random srand;
    ScoreType score;
    HashType hash;

    int gindx(int k, int y, int x) const {
        return k*H*W + y*W + x;
    }

    pair<int, int> trans(const char dir, int y, int x) const {
        int ny = y, nx = x;
        switch (dir) {
            case ('U'): --ny; break;
            case ('D'): ++ny; break;
            case ('L'): --nx; break;
            case ('R'): ++nx; break;
            default:
                assert(false);
        }
        return {ny, nx};
    }

public:
    HashType zhs[K][H][W]; // マップkの(h,w)にいるときのハッシュ値
    pair<int, int> pos[K];
    int seen[K*H*W];
    vector<Action> actions_from_state[16];

    // TODO Stateを初期化する
    void init() {
        score = 0;
        hash = 0;

        rep(i, 16) {
            vector<Action> a;
            a.reserve(4);
            if (i >> 0 & 1) a.push_back(Action('U'));
            if (i >> 1 & 1) a.push_back(Action('D'));
            if (i >> 2 & 1) a.push_back(Action('L'));
            if (i >> 3 & 1) a.push_back(Action('R'));
            actions_from_state[i] = a;
        }

        rep(k, K) rep(i, H) rep(j, W) {
            zhs[k][i][j] = srand.rand_u64();
        }

        rep(i, K*H*W) seen[i] = 0;
        rep(k, K) {
            rep(i, H) rep(j, W) {
                if (R[ID[k]][i][j] == '@') {
                    seen[gindx(k, i, j)]++;
                    pos[k] = {i, j};
                    hash ^= zhs[k][i][j];
                    break;
                }
            }
        }
        score = 0;
    }

    // TODO
    //! `action` をしたときの評価値とハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    //! threshold以上であれば計算しなくてよい
    tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const {
        action.is_moved = 0;
        ScoreType s = 0;
        HashType hs = 0;
        rep(k, K) {
            auto [y, x] = pos[k];
            hs ^= zhs[k][y][x];
            if (R[ID[k]][y][x] == 'x') continue;
            auto [ny, nx] = trans(action.dir, y, x);
            switch (R[ID[k]][ny][nx]) {
                case ('#'):
                    ny = y;
                    nx = x;
                    break;
                case ('o'):
                    if (seen[gindx(k, ny, nx)] == 0) s -= 10;
                    action.is_moved |= 1 << k;
                    break;
                case ('x'):
                    s = 1e5;
                    action.is_moved |= 1 << k;
                    break;
                default:
                    action.is_moved |= 1 << k;
            }
            s += seen[gindx(k, ny, nx)] + 1;
            hs ^= zhs[k][ny][nx];
        }
        return {score + s, hash ^ hs, false};
    }

    // TODO
    //! `action` をする
    void apply_op(const Action &action) {
        // TODO
        rep(k, K) {
            auto [y, x] = pos[k];
            hash ^= zhs[k][y][x];
            auto [ny, nx] = trans(action.dir, y, x);
            switch (R[ID[k]][ny][nx]) {
                case ('#'):
                    ny = y; nx = x;
                    break;
                case ('x'):
                    assert(false);
                case ('o'):
                    if (seen[gindx(k, ny, nx)] == 0) {
                        score -= 10;
                    }
                break;
            }
            seen[gindx(k, ny, nx)]++;
            score += seen[gindx(k, ny, nx)];
            hash ^= zhs[k][ny][nx];
            pos[k] = {ny, nx};
        }
    }

    // TODO
    //! `action` を戻す
    void rollback(const Action &action) {
        // TODO
        rep(k, K) {
            auto [y, x] = pos[k];
            score -= seen[gindx(k, y, x)];
            hash ^= zhs[k][y][x];
            seen[gindx(k, y, x)]--;
            if (R[ID[k]][y][x] == 'o') {
                if (seen[gindx(k, y, x)] == 0) {
                    score += 10;
                }
            }
            if (action.is_moved >> k & 1) {
                int ny = y, nx = x;
                switch (action.dir) {
                    case ('U'): ++ny; break;
                    case ('D'): --ny; break;
                    case ('L'): ++nx; break;
                    case ('R'): --nx; break;
                    default: assert(false);
                }
                pos[k] = {ny, nx};
            }
            tie(y, x) = pos[k];
            hash ^= zhs[k][y][x];
        }
    }

    // TODO
    //! 現状態から遷移可能な `Action` の `vector` を返す
    template<class Emit>
    void get_actions(const int turn, const Action &last_action, Emit &&emit) const {
        int state = 0b1111;
        rep(k, K) {
            auto [y, x] = pos[k];
            state &= ((R[ID[k]][y-1][x] != 'x') << 0) | 0b1110;
            state &= ((R[ID[k]][y+1][x] != 'x') << 1) | 0b1101;
            state &= ((R[ID[k]][y][x-1] != 'x') << 2) | 0b1011;
            state &= ((R[ID[k]][y][x+1] != 'x') << 3) | 0b0111;
        }
        for (Action a : actions_from_state[state]) {
            emit(a);
        }
    }

    void print() const {}
    string get_state_info() const { return ""; }
};

flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF, false> bs;

flying_squirrel::BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting=false, bool a=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, a};
}

vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false) {
    return bs.search(param, verbose, "hist.json");
}
} // namespace beam_search


void print_ans(const vector<beam_search::Action> &ans) {
    for (int i = 0; i < K; ++i) {
        cout << ID[i] << ' ';
    }
    cout << endl;

    int score = 0;
    vector<pair<int, int>> pos(K);
    bool seen[K][H][W];
    rep(k, K) {
        rep(i, H) rep(j, W) {
            if (R[ID[k]][i][j] == '@') {
                pos[k] = {i, j};
            }
            seen[k][i][j] = false;
        }
    }
    auto nxt = [&] (beam_search::Action action, int k, int y, int x) -> pair<int, int> {
        int ny = y, nx = x;
        if (action.dir == 'U') ny--;
        if (action.dir == 'D') ny++;
        if (action.dir == 'L') nx--;
        if (action.dir == 'R') nx++;
        assert(R[ID[k]][ny][nx] != 'x');
        if (R[ID[k]][ny][nx] == '#') {
            ny = y; nx = x;
        }
        return {ny, nx};
    };

    if (ans.size() != 2500) {
        cerr << ans.size() << endl;
        assert(false);
    }
    for (int i = 0; i < ans.size(); ++i) {
        cout << ans[i];
        rep(k, K) {
            auto [ny, nx] = nxt(ans[i], k, pos[k].first, pos[k].second);
            if (R[ID[k]][ny][nx] == 'o' && seen[k][ny][nx] == false) {
                score += 1;
                seen[k][ny][nx] = true;
            }
            pos[k] = {ny, nx};
        }
    }
    cout << endl;

    cerr << "Score = " << score << endl;
}

void solve() {
    vector<pair<int, int>> search(N);
    rep(k, N) {
        queue<pair<int, int>> todo;
        vector<vector<int>> seen(H, vector<int>(W, false));
        int sy = -1, sx = -1;
        rep(i, H) rep(j, W) {
            if (R[k][i][j] == '@') {
                sy = i; sx = j;
            }
        }
        assert(sy != -1);
        todo.push({sy, sx});
        int s = 0;
        seen[sy][sx] = true;
        while (!todo.empty()) {
            s++;
            auto [y, x] = todo.front(); todo.pop();
            rep(d, 4) {
                int ny = y+dy[d];
                int nx = x+dx[d];
                if (0 <= ny && ny < H && 0 <= nx && nx < W) {
                    if (seen[ny][nx]) continue;
                    if (R[k][ny][nx] == 'x') {
                        s--;
                        continue;
                    }
                    if (R[k][ny][nx] == '#') continue;
                    seen[ny][nx] = true;
                    todo.push({ny, nx});
                }
            }
        }
        search[k] = {s, k};
    }
    sort(search.begin(), search.end());
    rep(k, K) {
        auto [_, id] = search[N-1-k];
        ID[k] = id;
    }

    auto param = beam_search::gen_param(2500, 1000, 1, false, true);
    vector<beam_search::Action> ans = beam_search::search(param, true);

    print_ans(ans);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(1);
    input();
    solve();
    return 0;
}
