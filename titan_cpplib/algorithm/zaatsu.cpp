#include <vector>
using namespace std;

// Zaatsu
namespace titan23 {

    /**
     * @brief 座標圧縮管理クラス
     */
    template<typename T>
    class Zaatsu {
      private:
        vector<T> to_origin;
        int n;

      public:
        //! `used_items` からなる集合を管理するインスタンスを生成
        Zaatsu(vector<T> &used_items) : to_origin(used_items) {
            sort(to_origin.begin(), to_origin.end());
            to_origin.erase(unique(to_origin.begin(), to_origin.end()), to_origin.end());
            n = (int)to_origin.size();
        }

        //! 要素の種類数を返す(`max(to_zaatsu)`)
        int len() const {
            return n;
        }

        //! `x` を座標圧縮する
        int to_zaatsu(const T &x) const {
            return lower_bound(to_origin.begin(), to_origin.end(), x) - to_origin.begin();
        }

        //! 座標圧縮された `x` を戻す
        T to_origin(const int &x) const {
            assert(0 <= x && x < n);
            return to_origin[x];
        }
    };
}  // namespace titan23
