#pragma once

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
        stack<int> unused_idx;

      public:
        StatePool() {}
        StatePool(const int n) { init(n); }

        //! clear
        void clear() {
            while (!unused_idx.empty()) unused_idx.pop();
            for (int i = (int)pool.size()-1; i >= 0; --i) {
                unused_idx.emplace(i);
            }
        }

        //! n要素確保する。
        void init(const int n) {
            for (int i = 0; i < n; ++i) {
                T* state = new T;
                pool.emplace_back(state);
                unused_idx.emplace(i);
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
            unused_idx.emplace(id);
        }

        //! T を作成し、それに対応する id を返す。
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

        //! id に対応するTをコピーし、コピー先のidを返す。
        //! T のコピーメソッドを呼び出す。
        int copy(const int id) {
            int new_id = gen();
            pool[id]->copy(pool[new_id]);
            return new_id;
        }

        int used_size() const {
            return (int)pool.size() - (int)unused_idx.size();
        }

        //! 内部サイズを呼び出す
        int get_size() const {
            return pool.size();
        }
    };
}  // namespace titan23
