#include <vector>
using namespace std;

namespace titan23 {

template<typename T>
class FastStack {
private:
    vector<T> s;
    size_t idx;

public:
    FastStack() : idx(0) {}
    FastStack(size_t cap) : s(cap), idx(0) {}
    bool empty() const { return idx == 0; }
    void emplace(T v) {
        if (idx < s.size()) {
            s[idx] = v;
        } else {
            s.emplace_back(v);
        }
        idx++;
    }
    void clear() { idx = 0; }
    void pop() { --idx; }
    T top() const { return s[idx-1]; }
    size_t size() const { return idx; }
};
} // namespace titan23
