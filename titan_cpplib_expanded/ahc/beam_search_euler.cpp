// #include "titan_cpplib/ahc/beam_search_euler.cpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

// #include "titan_cpplib/algorithm/random.cpp"
#include <cassert>
#include <unordered_set>
#include <vector>
using namespace std;

// Random
namespace titan23 {

    /**
     * @brief (疑似)乱数生成クラス(XOR shift)
     */
    class Random {
      private:
        unsigned int _x, _y, _z, _w;

        unsigned int _xor128() {
            const unsigned int t = _x ^ (_x << 11);
            _x = _y;
            _y = _z;
            _z = _w;
            _w = (_w ^ (_w >> 19)) ^ (t ^ (t >> 8));
            return _w;
        }

      public:
        Random() : _x(123456789),
                   _y(362436069),
                   _z(521288629),
                   _w(88675123) {}

        //! `[0, 1]` の乱数を返す(実数)
        double random() { return (double)(_xor128()) / 0xFFFFFFFF; }

        //! `[0, end]` の乱数を返す(整数)
        int randint(const int end) {
            assert(0 <= end);
            return (((unsigned long long)_xor128() * (end+1)) >> 32);
        }

        //! `[begin, end]` の乱数を返す(整数)
        int randint(const int begin, const int end) {
            assert(begin <= end);
            return begin + (((unsigned long long)_xor128() * (end-begin+1)) >> 32);
        }

        //! `[0, end)` の乱数を返す(整数)
        int randrange(const int end) {
            assert(0 < end);
            return (((unsigned long long)_xor128() * end) >> 32);
        }

        //! `[begin, end)` の乱数を返す(整数)
        int randrange(const int begin, const int end) {
            assert(begin < end);
            return begin + (((unsigned long long)_xor128() * (end-begin)) >> 32);
        }

        //! `[0, u64_MAX)` の乱数を返す / zobrist hash等の使用を想定
        unsigned long long rand_u64() {
            return ((unsigned long long)_xor128() << 32) | _xor128();
        }

        //! `[0, end)` の異なる乱数を2つ返す
        pair<int, int> rand_pair(const int end) {
            assert(end >= 2);
            int u = randrange(end);
            int v = u + randrange(1, end);
            if (v >= end) v -= end;
            if (u > v) swap(u, v);
            return {u, v};
        }

        //! `[begin, end)` の異なる乱数を2つ返す
        pair<int, int> rand_pair(const int begin, const int end) {
            assert(end - begin >= 2);
            int u = randrange(begin, end);
            int v = (u + randrange(1, end-begin));
            if (v >= end) v -= (end-begin);
            if (u > v) swap(u, v);
            return {u, v};
        }

        //! Note `a`は非const
        vector<int> rand_vec(const int cnt, vector<int> &a) {
            int n = a.size();
            for (int i = 0; i < cnt; ++i) {
                int j = randrange(i, n);
                swap(a[i], a[j]);
            }
            vector<int> res(a.begin(), a.begin()+cnt);
            return res;
        }

        //! `[begin, end)` の乱数を返す(実数)
        double randdouble(const double begin, const double end) {
            assert(begin < end);
            return begin + random() * (end-begin);
        }

        //! `vector` をインプレースにシャッフルする / `O(n)`
        template <typename T>
        void shuffle(vector<T> &a) {
            int n = a.size();
            for (int i = 0; i < n-1; ++i) {
                int j = randrange(i, n);
                swap(a[i], a[j]);
            }
        }

        template <typename T>
        vector<T> choices(const vector<T> &a, const int k) {
            assert(a.size() >= k);
            vector<T> r(k);
            unordered_set<int> seen;
            for (int i = 0; i < k; ++i) {
                int x = randrange(a.size());
                while (seen.find(x) != seen.end()) x = randrange(a.size());
                seen.insert(x);
                r[i] = a[x];
            }
            return r;
        }

        template <typename T>
        T choice(const vector<T> &a) {
            return a[randrange(a.size())];
        }

        template <typename T>
        T choice(const vector<T> &a, const vector<int> &w, bool normal) {
            assert(normal == false);
            assert(a.size() == w.size());
            double sum = 0.0;
            for (const int &x: w) sum += x;
            assert(sum > 0);
            vector<double> x(w.size());
            for (int i = 0; i < x.size(); ++i) {
                x[i] = (double)w[i] / sum;
                if (i-1 >= 0) x[i] += x[i-1];
            }
            return choice(a, x);
        }

        template <typename T>
        T choice(const vector<T> &a, const vector<double> &w) {
            double i = random();
            int l = -1, r = a.size()-1;
            while (r - l > 1) {
                int mid = (l + r) / 2;
                if (w[mid] <= i) l = mid;
                else r = mid;
            }
            return a[r];
        }
    };
} // namespace titan23

// #include "titan_cpplib/ahc/state_pool.cpp"
#include <cassert>
#include <memory>
#include <vector>
#include <stack>
using namespace std;

// StatePool
namespace titan23 {

    // ノードプールクラス
    template<typename T>
    class StatePool {
      private:
        vector<T*> pool;
        stack<int> unused_indx;

      public:
        StatePool() {}
        StatePool(const int n) { init(n); }

        //! clear
        void clear() {
            while (!unused_indx.empty()) unused_indx.pop();
            for (int i = (int)pool.size()-1; i >= 0; --i) {
                unused_indx.emplace(i);
            }
        }

        //! n要素確保する。
        void init(const int n) {
            for (int i = 0; i < n; ++i) {
                T* state = new T;
                pool.emplace_back(state);
                unused_indx.emplace(i);
            }
        }

        //! id に対応する T のポインタを返す。
        T* get(int id) const {
            assert(0 <= id && id < pool.size());
            return pool[id];
        }

        //! idに対応するTを仮想的に削除する。
        void del(int id) {
            assert(0 <= id && id < pool.size());
            unused_indx.emplace(id);
        }

        //! T を作成し、それに対応する id を返す。
        int gen() {
            int state_id;
            if (unused_indx.empty()) {
                T* state = new T;
                state_id = pool.size();
                pool.emplace_back(state);
            } else {
                state_id = unused_indx.top();
                unused_indx.pop();
            }
            return state_id;
        }

        //! id に対応するTをコピーし、コピー先のidを返す。
        //! T のコピーメソッドを呼び出す。
        int copy(const int id) {
            int new_id = gen();
            pool[id]->copy(pool[new_id]);
            return new_id;
        }

        //! 内部サイズを呼び出す
        int get_size() const {
            return pool.size();
        }
    };
}  // namespace titan23

// #include "titan_cpplib/ahc/timer.cpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

// Timer
namespace titan23 {

    /**
     * @brief 時間計測クラス
     */
    class Timer {
      private:
        chrono::time_point<chrono::high_resolution_clock> start_timepoint;

      public:
        Timer() : start_timepoint(chrono::high_resolution_clock::now()) {}

        //! リセットする
        void reset() {
            start_timepoint = chrono::high_resolution_clock::now();
        }

        //! 経過時間[ms](double)を返す
        double elapsed() const {
            auto end_timepoint = chrono::high_resolution_clock::now();
            auto start = chrono::time_point_cast<chrono::microseconds>(start_timepoint).time_since_epoch().count();
            auto end = chrono::time_point_cast<chrono::microseconds>(end_timepoint).time_since_epoch().count();
            return (end - start) * 0.001;
        }
    };
}  // namespace titan23

// #include "titan_cpplib/data_structures/hash_set.cpp"
#include <vector>
#include <random>
#include <iostream>
#include <cassert>

using namespace std;

// HashSet
namespace titan23 {

    class HashSet {
      private:
        using u64 = unsigned long long;
        static constexpr const u64 K = 0x517cc1b727220a95;
        static constexpr const int M = 2;
        vector<u64> exist;
        vector<u64> keys;
        int msk, xor_;
        int size;

        int hash(const u64 &key) const {
            return (((((key>>32)&msk) ^ (key&msk) ^ xor_)) * (HashSet::K & msk)) & msk;
        }

        pair<int, bool> get_pos(const u64 &key) const {
            int h = hash(key);
            while (true) {
                if (!(exist[h>>6]>>(h&63)&1)) return {h, false};
                if (keys[h] == key) return {h, true};
                h = (h + 1) & msk;
            }
        }

        int bit_length(const int x) const {
            if (x == 0) return 0;
            return 32 - __builtin_clz(x);
        }

        void rebuild() {
            vector<u64> old_exist = exist;
            vector<u64> old_keys = keys;
            exist.resize(HashSet::M*old_exist.size()+1);
            fill(exist.begin(), exist.end(), 0);
            keys.resize(HashSet::M*old_keys.size());
            size = 0;
            msk = (1<<bit_length(keys.size()-1))-1;
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<int> dis(0, msk);
            xor_ = dis(gen);
            for (int i = 0; i < (int)old_keys.size(); ++i) {
                if (old_exist[i>>6]>>(i&63)&1) {
                    insert(old_keys[i]);
                }
            }
        }

      public:
        HashSet() : exist(1, 0), keys(1), msk(0), xor_(0), size(0) {}

        HashSet(const int n) {
            int s = 1<<bit_length(n);
            s *= HashSet::M;
            assert(s > 0);
            exist.resize((s>>6)+1, 0);
            keys.resize(s);
            msk = (1<<bit_length(keys.size()-1))-1;
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<int> dis(0, msk);
            xor_ = dis(gen);
            size = 0;
        }

        bool contains(const u64 key) const {
            return get_pos(key).second;
        }

        void insert(const u64 key) {
            const auto [pos, is_exist] = get_pos(key);
            if (!is_exist) {
                exist[pos>>6] |= 1ull<<(pos&63);
                keys[pos] = key;
                ++size;
                if (HashSet::M*size > keys.size()) {
                    rebuild();
                }
            }
        }

        //! keyがすでにあればtrue, なければ挿入してfalse / `O(1)`
        bool contains_insert(const u64 key) {
            const auto [pos, is_exist] = get_pos(key);
            if (!is_exist) {
                exist[pos>>6] |= 1ull<<(pos&63);
                keys[pos] = key;
                ++size;
                if (HashSet::M*size > keys.size()) {
                    rebuild();
                }
                return false;
            }
            return true;
        }

        //! 全ての要素を削除する / `O(n/w)`
        void clear() {
            this->size = 0;
            fill(exist.begin(), exist.end(), 0);
        }

        int len() const {
            return size;
        }
    };
} // namespaced titan23

// #include "titan_cpplib/others/print.cpp"
#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
using namespace std;

// print

// color
static const string PRINT_RED = "\033[31m";
static const string PRINT_GREEN = "\033[32m";
static const string PRINT_NONE = "\033[m";

// pair<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t) {
    os << "( " << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << " )";
    return os;
}

// vector<T>
template <typename T>
ostream& operator<<(ostream& os, const vector<T>& a) {
    int n = (int)a.size();
    os << "[";
    for (int i = 0; i < n-1; ++i) {
        os << a[i] << ", ";
    }
    if (n > 0) {
        os << a.back();
    }
    os << "]";
    return os;
}

// vector<vector<T>>
template <typename T>
ostream& operator<<(ostream& os, const vector<vector<T>>& a) {
    os << "[\n";
    int h = (int)a.size();
    for (int i = 0; i < h; ++i) {
        os << "  " << a[i] << '\n';
    }
    os << "]";
    return os;
}

// set<T>
template <typename T>
ostream& operator<<(ostream& os, const set<T>& a) {
    os << "{";
    for (const T &x: a) {
        os << x;
        if (x != *(--a.end())) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_set<T>
template <typename T>
ostream& operator<<(ostream& os, const unordered_set<T>& a) {
    set<T> s;
    for (const T &x : a) {
        s.insert(x);
    }
    os << s;
    return os;
}

// map<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& mp) {
    os << "{";
    auto it = mp.begin();
    while (it != mp.end()) {
        os << it->first << ": " << it->second;
        ++it;
        if (it != mp.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}


using namespace std;

#define rep(i, n) for (int i = 0; i < (n); ++i)

//! 木上のビームサーチライブラリ
namespace beam_search_with_tree {

using ScoreType = long long;
using HashType = unsigned long long;
const ScoreType INF = 1e9;

// Action
struct Action {
    char d;
    ScoreType pre_score, nxt_score;
    HashType pre_hash, nxt_hash;

    Action() {}
    Action(const char d) : d(d), pre_score(INF), nxt_score(INF), pre_hash(0), nxt_hash(0) {}
};
ostream& operator<<(ostream& os, const Action &action) {
    os << action.d;
    return os;
}

class State {
  private:

  public:
    ScoreType score;
    HashType hash;

    State() {}

    // TODO Stateを初期化する
    void init() {}

    // TODO
    //! `action` をしたときの評価値とハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    pair<ScoreType, HashType> try_op(Action &action) const {}

    bool is_done() const {}

    // TODO
    //! `action` をする
    void apply_op(const Action &action) {}

    // TODO
    //! `action` を戻す
    void rollback(const Action &action) {}

    // TODO
    //! 現状態から遷移可能な `Action` の `vector` を返す
    vector<Action> get_actions() const {}

    void print() const {}
};

struct BeamParam {
    int MAX_TURN;
    int BEAM_WIDTH;
};

class BeamSearchWithTree {
    // ref: https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639

  private:
    static constexpr const int PRE_ORDER = -1;
    static constexpr const int POST_ORDER = -2;

    titan23::HashSet seen;
    using ActionIDType = int;
    ActionIDType ActionID;
    vector<Action> result;

    // ビームサーチの過程を表す木
    // <dir or id, action, action_id>
    // dir or id := 葉のとき、leaf_id
    //              そうでないとき、行きがけなら-1、帰りがけなら-2
    vector<tuple<int, Action, ActionIDType>> tree;

    // 次のビーム候補を保持する配列
    vector<tuple<int, ScoreType, Action, ActionIDType>> next_beam; // <par, score, action, action_id>

    vector<vector<int>> next_beam_data;

    void get_next_beam(State* state, const int turn) {
        next_beam.clear();
        next_beam.reserve(tree.size() * 4); // TODO
        seen.clear();

        if (turn == 0) {
            vector<Action> actions = state->get_actions();
            for (Action &action : actions) {
                auto [score, hash] = state->try_op(action);
                if (seen.contains_insert(hash)) continue;
                next_beam.emplace_back(PRE_ORDER, score, action, ActionID);
                ActionID++;
            }
            return;
        }

        int leaf_id = 0;
        for (int i = 0; i < tree.size(); ++i) {
            auto [dir_or_leaf_id, action, _] = tree[i];
            if (dir_or_leaf_id >= 0) {
                state->apply_op(action);
                vector<Action> actions = state->get_actions();
                std::get<0>(tree[i]) = leaf_id;
                for (Action &action : actions) {
                    auto [score, hash] = state->try_op(action);
                    if (seen.contains_insert(hash)) continue;
                    next_beam.emplace_back(leaf_id, score, action, ActionID);
                    ActionID++;
                }
                ++leaf_id;
                state->rollback(action);
            } else if (dir_or_leaf_id == PRE_ORDER) {
                state->apply_op(action);
            } else {
                state->rollback(action);
            }
        }
    }

    //! 不要なNodeを削除し、木を更新する
    int update_tree(State* state, const int turn) {
        vector<tuple<int, Action, ActionIDType>> new_tree;
        new_tree.reserve(tree.size());
        if (turn == 0) {
            for (auto &[par, _, new_action, action_id] : next_beam) {
                assert(par == -1);
                new_tree.emplace_back(0, new_action, action_id);
            }
            swap(tree, new_tree);
            return 0;
        }

        int i = 0;
        int apply_only_turn = 0;
        while (true) {
            const auto &[dir_or_leaf_id, action, action_id] = tree[i];
            // 行きがけかつ帰りがけのaction_idが一致しているなら、一本道なので行くだけ
            if (dir_or_leaf_id == PRE_ORDER && action_id == std::get<2>(tree.back())) {
                ++i;
                result.emplace_back(action);
                state->apply_op(action);
                tree.pop_back();
                apply_only_turn++;
            } else {
                break;
            }
        }

        for (; i < tree.size(); ++i) {
            const auto &[dir_or_leaf_id, action, action_id] = tree[i];
            if (dir_or_leaf_id >= 0) {
                if (next_beam_data[dir_or_leaf_id].empty()) continue;
                new_tree.emplace_back(PRE_ORDER, action, action_id);
                for (const int beam_idx : next_beam_data[dir_or_leaf_id]) {
                    auto &[_, __, new_action, new_action_id] = next_beam[beam_idx];
                    new_tree.emplace_back(dir_or_leaf_id, new_action, new_action_id);
                }
                new_tree.emplace_back(POST_ORDER, action, action_id);
                next_beam_data[dir_or_leaf_id].clear();
            } else if (dir_or_leaf_id == PRE_ORDER) {
                new_tree.emplace_back(PRE_ORDER, action, action_id);
            } else {
                int pre_dir = std::get<0>(new_tree.back());
                if (pre_dir == PRE_ORDER) {
                    new_tree.pop_back(); // 一つ前が行きがけなら、削除して追加しない
                } else {
                    new_tree.emplace_back(POST_ORDER, action, action_id);
                }
            }
        }
        swap(tree, new_tree);
        return apply_only_turn;
    }

    void get_result() {
        int best_id = -1;
        ScoreType best_score = 0;
        for (auto [par, score, _, __] : next_beam) {
            if (best_id == -1 || score < best_score) {
                best_score = score;
                best_id = par;
            }
        }
        assert(best_id != -1);
        for (const auto &[dir_or_leaf_id, action, _] : tree) {
            if (dir_or_leaf_id >= 0) {
                if (best_id == dir_or_leaf_id) {
                    result.emplace_back(action);
                    return;
                }
            } else if (dir_or_leaf_id == PRE_ORDER) {
                result.emplace_back(action);
            } else {
                result.pop_back();
            }
        }
        cerr << PRINT_RED << "Error: 解が見つかりませんでした" << PRINT_NONE << endl;
        assert(false);
    }

  public:

    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose ログ出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(const BeamParam &param, const bool verbose = false) {
        if (verbose) cerr << PRINT_GREEN << "Info: start search()" << PRINT_NONE << endl;

        ActionID = 0;
        State* state = new State;
        state->init();

        this->seen = titan23::HashSet(param.BEAM_WIDTH * 4); // TODO

        int now_turn = 0;
        for (int turn = 0; turn < param.MAX_TURN; ++turn) {
            if (verbose) cerr << "Info: # turn : " << turn+1 << endl;

            // 次のビーム候補を求める
            get_next_beam(state, turn-now_turn);

            if (next_beam.empty()) {
                cerr << PRINT_RED << "Error: \t次の候補が見つかりませんでした" << PRINT_NONE << endl;
                assert(!next_beam.empty());
            }

            // ビームを絞る
            int beam_width = min(param.BEAM_WIDTH, (int)next_beam.size());
            assert(beam_width <= param.BEAM_WIDTH);

            nth_element(next_beam.begin(), next_beam.begin() + beam_width, next_beam.end(), [&] (const tuple<int, ScoreType, Action, ActionIDType> &left, const tuple<int, ScoreType, Action, ActionIDType> &right) {
                return std::get<1>(left) < std::get<1>(right);
            });

            tuple<int, ScoreType, Action, ActionIDType> bests = *min_element(next_beam.begin(), next_beam.begin() + beam_width, [&] (const tuple<int, ScoreType, Action, ActionIDType> &left, const tuple<int, ScoreType, Action, ActionIDType> &right) {
                return std::get<1>(left) < std::get<1>(right);
            });
            if (verbose) cerr << "Info: \tbest_score = " << std::get<1>(bests) << endl;
            if (std::get<1>(bests) == 0) { // TODO 終了条件
                cerr << PRINT_GREEN << "Info: find valid solution." << PRINT_NONE << endl;
                get_result();
                result.emplace_back(std::get<2>(bests));
                return result;
            }

            // 探索木の更新
            if (turn != 0) {
                if (next_beam_data.size() < next_beam.size()) {
                    next_beam_data.resize(next_beam.size());
                }
                for (int i = 0; i < beam_width; ++i) {
                    auto &[par, _, new_action, new_action_id] = next_beam[i];
                    next_beam_data[par].emplace_back(i);
                }
            }
            int apply_only_turn = update_tree(state, turn);
            now_turn += apply_only_turn;
        }

        // 答えを復元する
        if (verbose) cerr << PRINT_GREEN << "Info: MAX_TURN finished." << PRINT_NONE << endl;
        get_result();
        return result;
    }
};
} // namespace beam_search

// int main() {
//     beam_search_with_tree::BeamParam param;
//     param.MAX_TURN = 1000;
//     param.BEAM_WIDTH = 100;
//     beam_search_with_tree::init_zhs();
//     beam_search_with_tree::BeamSearchWithTree bs;
//     vector<beam_search_with_tree::Action> ans = bs.search(param, true);
//     for (const beam_search_with_tree::Action &action : ans) {
//         cout << action;
//     }
//     cout << endl;
//     cerr << "Score = " << ans.size() << endl;
//     return 0;
// }

