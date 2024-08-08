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
        stack<long long> unused_indx;

     public:
        StatePool() {}
        StatePool(const long long n) {
            init(n);
        }

        void clear() {
            while (!unused_indx.empty()) unused_indx.pop();
            for (long long i = (long long)pool.size()-1; i >= 0; --i) {
                unused_indx.emplace(i);
            }
        }

        // n要素確保する。
        void init(const long long n) {
            for (long long i = 0; i < n; ++i) {
                T* state = new T;
                pool.emplace_back(state);
                unused_indx.emplace(i);
            }
        }

        // id に対応する T のポインタを返す。
        T* get(long long id) const {
            assert(0 <= id && id < pool.size());
            return pool[id];
        }

        // idに対応するTを仮想的に削除する。
        void del(long long id) {
            assert(0 <= id && id < pool.size());
            unused_indx.emplace(id);
        }

        // T を作成し、それに対応する id を返す。
        long long gen() {
            long long state_id;
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

        // id に対応するTをコピーし、コピー先のidを返す。
        // T のコピーメソッドを呼び出す。
        long long copy(const long long id) {
            long long new_id = gen();
            pool[id]->copy(pool[new_id]);
            return new_id;
        }

        long long get_size() const {
            return pool.size();
        }
    };
}  // namespace titan23
