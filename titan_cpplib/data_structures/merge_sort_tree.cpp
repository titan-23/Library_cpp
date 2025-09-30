#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

template<class T, class F, F func(const vector<T>&)>
class MergeSortTree {
private:
    int n, size, log;

    struct Data {
        vector<T> data;
        F func_data;
    };

    vector<Data> data;

    int bit_length(const int x) const {
        return x > 0 ? 32 - __builtin_clz(x) : 0;
    }

public:
    MergeSortTree() {}
    MergeSortTree(vector<T> a) {
        n = a.size();
        log = bit_length(n);
        size = 1 << log;
        data.resize(size<<1);
        for (int i = size; i < size+n; ++i) {
            data[i].data.emplace_back(a[i-size]);
        }
        for (int i = size-1; i > 0; --i) {
            const auto &l = data[i<<1].data;
            const auto &r = data[i<<1|1].data;
            auto &now = data[i].data;
            now.resize(l.size() + r.size());
            std::merge(l.begin(), l.end(), r.begin(), r.end(), now.begin());
        }
        for (int i = 1; i < data.size(); ++i) {
            data[i].func_data = func(data[i].data);
        }
    }

    // &つけるのを忘れずに！
    // auto g = [&] (vector<T> &data, F &func_data) -> void;
    template <class G>
    void prod(int l, int r, G g) {
        l += size; r += size;
        while (l < r) {
            if (l & 1) {
                g(data[l].data, data[l].func_data);
                l++;
            }
            if (r & 1) {
                --r;
                g(data[r].data, data[r].func_data);
            }
            l >>= 1; r >>= 1;
        }
    }
};
} // namespace titan23
