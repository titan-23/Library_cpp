// #include "titan_cpplib/ahc/beam_search_with_tree.cpp"
#include <vector>
#include <algorithm>

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

using namespace std;

//! 木上のビームサーチライブラリ
namespace beam_search_with_tree {

using ScoreType = long long;
using HashType = unsigned long long;

struct Action {
    Action() {}

    friend ostream& operator<<(ostream& os, const Action &action) {
        return os;
    }
};

class State {
  private:
    titan23::Random srand;
    ScoreType score;
    HashType hash;

  public:
    // TODO Stateを初期化する
    void init() {
        this->score = 0;
    }

    // TODO
    //! `action` をしたときの評価値とハッシュ値を返す
    //! ロールバックに必要な情報はすべてactionにメモしておく
    pair<ScoreType, HashType> try_op(Action &action) const {
    }

    // TODO
    //! `action` をする
    void apply_op(const Action &action) {
    }

    // TODO
    //! `action` を戻す
    void rollback(const Action &action) {
    }

    // TODO
    //! 現状態から遷移可能な `Action` の `vector` を返す
    vector<Action> get_actions() const {
    }

    ScoreType get_score() {
        return this->score;
    }

    void print() const {
    }
};

struct BeamParam {
    int MAX_TURN;
    int BEAM_WIDTH;
};

using TreeNodeID = int;
using SubStateID = int;

//! try_opした結果をメモしておく構造体
struct SubState {
    TreeNodeID par;
    Action action;
    ScoreType score;

    SubState() : par(-1) {}
    SubState(TreeNodeID par, const Action &action, ScoreType score) : par(par), action(action), score(score) {}
};

//! ビームサーチの過程を表す木のノード
struct TreeNode {
    TreeNodeID par;
    Action pre_action;
    ScoreType score;
    vector<TreeNodeID> child;

    TreeNode() : par(-1) {}

    bool is_leaf() const {
        return child.empty();
    }
};

titan23::StatePool<TreeNode> treenode_pool;
titan23::StatePool<SubState> substate_pool;


class BeamSearchWithTree {
  private:
    ScoreType best_score;
    TreeNodeID best_id;
    titan23::HashSet seen;

    void get_next_beam_recursion(State* state, TreeNodeID node, vector<SubStateID> &next_beam, int depth, const int beam_width) {
        if (depth == 0) { // 葉
            vector<Action> actions = state->get_actions();
            for (Action &action : actions) {
                auto [score, hash] = state->try_op(action);
                if (seen.contains_insert(hash)) continue;
                SubStateID substate = substate_pool.gen();
                substate_pool.get(substate)->par = node;
                substate_pool.get(substate)->action = action;
                substate_pool.get(substate)->score = score;
                next_beam.emplace_back(substate);
            }
            return;
        }
        for (TreeNodeID nxt_node : treenode_pool.get(node)->child) {
            assert(treenode_pool.get(nxt_node)->is_valid);
        for (const TreeNodeID nxt_node : treenode_pool.get(node)->child) {
            state->apply_op(treenode_pool.get(nxt_node)->pre_action);
            get_next_beam_recursion(state, nxt_node, next_beam, depth-1, beam_width);
            state->rollback(treenode_pool.get(nxt_node)->pre_action);
        }
    }

    tuple<int, TreeNodeID, vector<SubStateID>> get_next_beam(State* state, TreeNodeID node, int turn, const int beam_width) {
        int cnt = 0;
        while (true) { // 一本道は行くだけ
            if (treenode_pool.get(node)->child.size() != 1) break;
            ++cnt;
            node = treenode_pool.get(node)->child[0];
            state->apply_op(treenode_pool.get(node)->pre_action);
        }
        vector<SubStateID> next_beam;
        seen.clear();
        get_next_beam_recursion(state, node, next_beam, turn-cnt, beam_width);
        return make_tuple(cnt, node, next_beam);
    }

    //! 親を返す / 無ければ自分を返す(!?)
    TreeNodeID get_par(SubStateID s_node, int cnt=1) {
        assert(0 <= s_node && s_node < substate_pool.get_size());
        TreeNodeID node = substate_pool.get(s_node)->par;
        assert(node != -1);
        for (int i = 0; i < cnt; ++i) {
            assert(node < treenode_pool.get_size());
            if (treenode_pool.get(node)->par == -1) {
                assert(node != -1);
                return node;
            }
            assert(node != -1);
            node = treenode_pool.get(node)->par;
        }
        assert(node != -1);
        assert(node < treenode_pool.get_size());
        return node;
    }

    //! 不要なNodeを削除し、木を更新する
    bool update_tree(const TreeNodeID node, const int depth) {
        if (treenode_pool.get(node)->is_leaf()) return depth == 0;
        int idx = 0;
        while (idx < treenode_pool.get(node)->child.size()) {
            TreeNodeID nxt_node = treenode_pool.get(node)->child[idx];
            if (!update_tree(nxt_node, depth-1)) {
                treenode_pool.del(nxt_node);
                treenode_pool.get(node)->child.erase(treenode_pool.get(node)->child.begin() + idx);
                continue;
            }
            ++idx;
        }
        return idx > 0;
    }

    //! node以上のパスを返す
    vector<Action> get_path(TreeNodeID node) {
        vector<Action> result;
        while (node != -1 && treenode_pool.get(node)->par != -1) {
            result.emplace_back(treenode_pool.get(node)->pre_action);
            node = treenode_pool.get(node)->par;
        }
        reverse(result.begin(), result.end());
        return result;
    }

    //! for debug
    void print_tree(State* state, const TreeNodeID node, int depth) {
    }

    //! node以下で、葉かつ最も評価値の良いノードを見るける / 葉はターン数からは判断していないので注意
    void get_best_node(TreeNodeID node) {
        if (treenode_pool.get(node)->is_leaf()) {
            if (best_id == -1 || treenode_pool.get(node)->score < best_score) {
                best_score = treenode_pool.get(node)->score;
                best_id = node;
            }
            return;
        }
        for (TreeNodeID nxt_node : treenode_pool.get(node)->child) {
            get_best_node(nxt_node);
        }
    }

    vector<Action> get_result(TreeNodeID root) {
        best_id = -1; // 更新
        get_best_node(root);
        TreeNodeID node = best_id;
        vector<Action> result = get_path(node);
        cerr << treenode_pool.get_size() << endl;
        return result;
    }

  public:
    /**
     * @brief ビームサーチをする
     *
     * @param param ターン数、ビーム幅を指定するパラメータ構造体
     * @param verbose 途中結果のスコアを標準エラー出力するかどうか
     * @return vector<Action>
     */
    vector<Action> search(const BeamParam &param, const bool verbose = false) {
        TreeNodeID root = treenode_pool.gen();
        treenode_pool.get(root)->child.clear();
        treenode_pool.get(root)->par = -1;

        State* state = new State;
        state->init();

        this->seen = titan23::HashSet(param.BEAM_WIDTH * 4); // TODO

        int now_turn = 0;

        for (int turn = 0; turn < param.MAX_TURN; ++turn) {
            if (verbose) cerr << "# turn : " << turn << endl;

            // 次のビーム候補を求める
            auto [apply_only_turn, next_root, next_beam] = get_next_beam(state, root, turn-now_turn, param.BEAM_WIDTH);
            root = next_root;
            now_turn += apply_only_turn;
            assert(!next_beam.empty());
            if (verbose) {
                cerr << "min_score=" << substate_pool.get((*min_element(next_beam.begin(), next_beam.end(), [] (const SubStateID &left, const SubStateID &right) {
                    return substate_pool.get(left)->score < substate_pool.get(right)->score;
                })))->score << endl;
            }

            // ビームを絞る // TODO 評価値が一致した場合、親の評価値も参考にするなど
            int beam_width = min(param.BEAM_WIDTH, (int)next_beam.size());
            nth_element(next_beam.begin(), next_beam.begin() + beam_width, next_beam.end(), [&] (const SubStateID &left, const SubStateID &right) {
                // if (substate_pool.get(left)->score != substate_pool.get(right)->score) {
                    return substate_pool.get(left)->score < substate_pool.get(right)->score;
                // }
                // return treenode_pool.get(get_par(left))->score < treenode_pool.get(get_par(right))->score;
            });

            // 探索木の更新
            for (int i = 0; i < beam_width; ++i) {
                SubStateID s = next_beam[i];
                TreeNodeID new_node = treenode_pool.gen();
                treenode_pool.get(substate_pool.get(s)->par)->child.emplace_back(new_node);
                treenode_pool.get(new_node)->par = substate_pool.get(s)->par;
                treenode_pool.get(new_node)->pre_action = substate_pool.get(s)->action;
                treenode_pool.get(new_node)->score = substate_pool.get(s)->score;
            }
            substate_pool.clear();
            update_tree(root, turn-now_turn+1);
        }

        // 答えを復元する
        vector<Action> result = get_result(root);
        return result;
    }
};
} // namespace beam_search

// int main() {
//     beam_search_with_tree::BeamParam param;
//     param.MAX_TURN = 2500;
//     param.BEAM_WIDTH = 1000;
//     beam_search_with_tree::BeamSearchWithTree bs;
//     vector<beam_search_with_tree::Action> ans = bs.search(param, true);
// }

