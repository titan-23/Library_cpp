#include <cassert>
#include <memory>
#include <vector>
#include <stack>
using namespace std;

// StatePool
namespace titan23 {

  /**
   * @brief ノードプールクラス
   * @details intに注意
   */
  template<typename T>
  class StatePool {
   public:
    vector<T*> pool;
    stack<int> unused_indx;

   public:
    StatePool() {}
    StatePool(const unsigned int n) {
      init(n);
    }

    /**
     * @brief n要素確保する。
     */
    void init(const unsigned int n) {
      for (int i = 0; i < n; ++i) {
        T* state = new T;
        pool.emplace_back(state);
        unused_indx.emplace(i);
      }
    }

    /**
     * @brief id に対応する T のポインタを返す。
     */
    T* get(int id) const {
      assert(0 <= id && id < pool.size());
      return pool[id];
    }

    /**
     * @brief idに対応するTを仮想的に削除する。
     */
    void del(int id) {
      assert(0 <= id && id < pool.size());
      unused_indx.emplace(id);
    }

    /**
     * @brief T を仮想的に作成し、それに対応する id を返す。
     */
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

    /**
     * @brief id に対応するTをコピーし、コピー先のidを返す。
     * @details T のコピーメソッドを呼び出す。
     */
    int copy(const int id) {
      int new_id = gen();
      pool[id]->copy(pool[new_id]);
      return new_id;
    }
  };
}
