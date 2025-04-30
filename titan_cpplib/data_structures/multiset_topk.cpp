#include <vector>
#include <set>

using namespace std;

namespace titan23 {

template<class T, T(*op)(T, T), T(*e)(), T(*inv)(T)>
class MultisetTopK {
private:
    int K;
    multiset<T> small, large;
    T product;

    void rebuild() {
        while ((int)small.size() < K && !large.empty()) {
            auto it = large.begin();
            product = op(product, *it);
            small.insert(*it);
            large.erase(it);
        }
        while (!small.empty() && !large.empty()) {
            auto s = --small.end();
            auto l = large.begin();
            T vs = *s, vl = *l;
            if (vs <= vl) break;
            product = op(product, inv(vs));
            product = op(product, vl);
            small.erase(s); small.insert(vl);
            large.erase(l); large.insert(vs);
        }
    }

public:
    MultisetTopK() : K(-1), product(e()) {}
    MultisetTopK(int K) : K(K), product(e()) {}
    MultisetTopK(int K, vector<T> a) : K(K), large(a.begin(), a.end()), product(e()) {}

    void insert(T v) {
        large.insert(v);
    }

    void erase(T v) {
        if (small.find(v) != small.end()) {
            small.erase(small.find(v));
            product = op(product, inv(v));
        } else if (large.find(v) != large.end()) {
            large.erase(large.find(v));
        }
    }

    T sum() { 
        rebuild();
        return product;
    }

    int len() const { return small.size() + large.size(); }
};
} // namespace titan23
