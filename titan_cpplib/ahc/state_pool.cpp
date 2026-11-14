/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/state_pool.cpp
#pragma once

#include <cassert>
#include <memory>
#include <vector>
#include <stack>
using namespace std;

// StatePool
namespace titan23 {

/// @brief T を id で管理し、削除した id を使い回すプール
template<typename T>
class StatePool {
private:
    vector<T*> pool;
    stack<int> unused_idx;

public:
    StatePool() {}
    /// @brief T を n 個確保して構築する
    StatePool(const int n) { init(n); }

    /// @brief 確保済みのすべての id を未使用に戻す
    void clear() {
        while (!unused_idx.empty()) unused_idx.pop();
        for (int i = (int)pool.size()-1; i >= 0; --i) {
            unused_idx.emplace(i);
        }
    }

    /// @brief T を n 個確保する / 空のプールに対してだけ呼ぶ
    void init(const int n) {
        for (int i = 0; i < n; ++i) {
            T* state = new T;
            pool.emplace_back(state);
            unused_idx.emplace(i);
        }
    }

    /// @brief id に対応する T のポインタを返す / O(1)
    T* get(int id) const {
        assert(0 <= id && id < pool.size());
        return pool[id];
    }

    /// @brief id に対応する T を仮想的に削除する / T は解放せず、id を未使用に戻す
    void del(int id) {
        assert(0 <= id && id < pool.size());
        unused_idx.emplace(id);
    }

    /// @brief T を 1 個割り当て、その id を返す / 未使用の id があれば使い回す
    int gen() {
        int state_id;
        if (unused_idx.empty()) {
            T* state = new T;
            state_id = pool.size();
            pool.emplace_back(state);
        } else {
            state_id = unused_idx.top();
            unused_idx.pop();
        }
        return state_id;
    }

    /// @brief id に対応する T を新しい id へコピーし、コピー先の id を返す / T の copy メソッドを呼ぶ
    int copy(const int id) {
        int new_id = gen();
        pool[id]->copy(pool[new_id]);
        return new_id;
    }

    /// @brief 使用中の T の個数を返す
    int used_size() const {
        return (int)pool.size() - (int)unused_idx.size();
    }

    /// @brief 確保済みの T の個数を返す
    int get_size() const {
        return pool.size();
    }
};
}  // namespace titan23
