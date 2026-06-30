#include <vector>
#include <algorithm>
using namespace std;

// Zaatsu
namespace titan23 {

/**
 * @brief 座標圧縮管理クラス
 */
template<typename T>
class Zaatsu {
private:
    vector<T> a;
    int n;

public:
    Zaatsu() : n(0) {}

    /// @brief `used_items` からなる集合を管理するインスタンスを生成
    Zaatsu(const vector<T> &used_items) : a(used_items) {
        sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        n = (int)a.size();
    }

    /// @brief `v` を追加する
    void add(const T &v) {
        a.push_back(v);
    }

    /// @brief 構築する
    void build() {
        sort(a.begin(), a.end());
        a.erase(unique(a.begin(), a.end()), a.end());
        n = (int)a.size();
    }

    /// @brief 要素の種類数を返す
    int len() const { return n; }

    /// @brief 要素の種類数を返す
    int size() const { return n; }

    /// @brief `v` を座標圧縮する
    int to_zaatsu(const T &v) const {
        return lower_bound(a.begin(), a.end(), v) - a.begin();
    }

    /// @brief 座標圧縮された要素 `x` を戻す
    T to_origin(const int &x) const {
        assert(0 <= x && x < n);
        return a[x];
    }
};
}  // namespace titan23
