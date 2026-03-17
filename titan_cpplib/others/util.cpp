#include <vector>
#include <algorithm>
using namespace std;

template <typename T>
bool discard_vec(vector<T> &a, const T &x) {
    auto it = find(a.begin(), a.end(), x);
    if (it == a.end()) return false;
    a.erase(it);
    return true;
}

template <typename T>
void remove_vec(vector<T> &a, const T &x) {
    auto it = find(a.begin(), a.end(), x);
    assert(it != a.end());
    a.erase(it);
}

template <typename T>
bool contains_vec(const vector<T> &a, const T &x) {
    return find(a.begin(), a.end(), x) != a.end();
}

template <typename T>
int index(const vector<T> &a, const T &x) {
    auto it = find(a.begin(), a.end(), x);
    assert(it != a.end());
    return it - a.begin();
}
