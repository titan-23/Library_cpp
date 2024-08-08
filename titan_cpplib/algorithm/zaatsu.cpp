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
        vector<T> _to_origin;
        int _n;

      public:
        //! `used_items` からなる集合を管理するインスタンスを生成
        Zaatsu(vector<T> &used_items) : _to_origin(used_items) {
            sort(_to_origin.begin(), _to_origin.end());
            _to_origin.erase(unique(_to_origin.begin(), _to_origin.end()), _to_origin.end());
            _n = (int)_to_origin.size();
        }

        //! 要素の種類数を返す(`max(to_zaatsu)`)
        int len() const {
            return _n;
        }

        //! `x` を座標圧縮する
        int to_zaatsu(const T &x) const {
            return lower_bound(_to_origin.begin(), _to_origin.end(), x) - _to_origin.begin();
        }

        //! 座標圧縮された `x` を戻す
        T to_origin(const int &x) const {
            return _to_origin[x];
        }
    };
}  // namespace titan23
