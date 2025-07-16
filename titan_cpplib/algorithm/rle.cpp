#include <vector>
using namespace std;

// rle
namespace titan23 {

template<typename T>
vector<pair<T, int>> rle(const vector<T> &a) {
    int n = (int)a.size();
    vector<pair<T, int>> res;
    T now = a[0];
    res.emplace_back(now, 1);
    for (int i = 1; i < n; ++i) {
        if (a[i] == now) {
            res.back().second++;
        } else {
            res.emplace_back(a[i], 1);
            now = a[i];
        }
    }
    return res;
}

}  // namespace titan23
