#include <vector>
#include <cassert>
using namespace std;

// CumulativeSum2D
namespace titan23 {
    /**
     * @brief 2次元累積和ライブラリ
     *
     * @tparam T
     */
    template<typename T>
    class CumulativeSum2D {
      private:
        int h, w;
        vector<T> acc;

      public:
        CumulativeSum2D() {}
        CumulativeSum2D(int h, int w, vector<vector<T>> &a, T e) :
                h(h), w(w), acc((h+1)*(w+1), e) {
            for (int ij = 0; ij < h*w; ++ij) {
                int i = ij / w, j = ij % w;
                acc[(i+1)*(w+1)+j+1] = acc[i*(w+1)+j+1] + acc[(i+1)*(w+1)+j] - acc[i*(w+1)+j] + a[i][j];
            }
        }

        //! `[h1, h2) x [w1, w2)` の和を返す / `O(1)`
        T sum(const int h1, const int w1, const int h2, const int w2) const {
            assert(0 <= h1 && h1 <= h2 && h2 <= h);
            assert(0 <= w1 && w1 <= w2 && w2 <= w);
            return acc[h2*(w+1)+w2] - acc[h2*(w+1)+w1] - acc[h1*(w+1)+w2] + acc[h1*(w+1)+w1];
        }
    };
}  // namespace titan23
