#include "titan_cpplib/data_structures/merge_sort_tree.cpp"
using namespace std;

namespace titan23 {

template<typename T>
class RangeKthSmallestPointUpdate {
private:
    using F = int;
    F func(const vector<int> &a) { return 0; }
    titan23::MergeSortTree<T, F, func> mst;
    T lower, upper;

    int get_cnt(T m) {
        int cnt = 0;
        mst.prod(l, r, [&] (vector<T> &data, F &func_data) {
            cnt += upper_bound(data.begin(), data.end(), m) - data.begin();
        });
        return cnt;
    }

public:
    RangeKthSmallestPointUpdate() {}

    // upper: 上限
    // lower: 下限-1
    RangeKthSmallestPointUpdate(vector<T> A, T lower, T upper) : lower(lower), upper(upper) {
        mst = titan23::MergeSortTree<T, F, func>(A);
    }

    T kth_smallest(int l, int r) const {
        T ok = upper, ng = lower;
        while (abs(ok - ng) > 1) {
            T mid = ng + (ok - ng)/2;
            if (get_cnt(mid) >= k+1) {
                ok = mid;
            } else {
                ng = mid;
            }
        }
        return ok;
    }
};

} // namespace titan23
