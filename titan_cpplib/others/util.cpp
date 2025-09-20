#include <vector>
#include <algorithm>
using namespace std;

template <typename T>
bool discard_vec(vector<T> &a, T x) {
    auto it = find(a.begin(), a.end(), x);
    if (it != a.end()) {
        a.erase(it);
        return true;
    } else {
        return false;
    }
}

template <typename T>
void remove_vec(vector<T> &a, T x) {
    auto it = find(a.begin(), a.end(), x);
    assert(it != a.end());
    a.erase(it);
}

template <typename T>
bool contains_vec(vector<T> &a, T x) {
    return find(a.begin(), a.end(), x) != a.end();
}
