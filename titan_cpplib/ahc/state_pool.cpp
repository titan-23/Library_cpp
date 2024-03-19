#include <cassert>
#include <memory>
using namespace std;

// StatePool
namespace titan23 {

  /**
   * @brief ノードプールクラス
   * @fn init(const unsigned int n): n要素確保する。
   * @fn T* get(int i) const: iに対応するTのポインタを返す。
   * @fn void del(int state_id): state_idに対応するTを仮想的に削除する。
   * @fn int gen(): Tを仮想的に作成し、それに対応するidを返す。
   * @fn int copy(int i): iに対応するTをコピーし、コピー先のidを返す。
   */
  template<typename T>
  class StatePool {
  private:
    vector<T*> pool;
    stack<int> unused_indx;

  public:
    StatePool() {}
    StatePool(const unsigned int n) {
      init(n);
    }

    void init(const unsigned int n) {
      for (int i = 0; i < n; ++i) {
        T* state = new T;
        pool.emplace_back(state);
        unused_indx.emplace(i);
      }
    }

    T* get(int id) const {
      assert(0 <= id && id < pool.size());
      return pool[id];
    }
    
    void del(int id) {
      assert(0 <= id && id < pool.size());
      unused_indx.emplace(id);
    }

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

    int copy(int id) {
      int new_id = gen();
      pool[id]->copy(pool[new_id]);
      return new_id;
    }
  };
}
