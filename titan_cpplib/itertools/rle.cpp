#include <vector>
#include <string>
using namespace std;

namespace titan23 {

template<typename T>
vector<pair<T, int>> RLE(const vector<T> &a) {
    vector<pair<T, int>> res;
    if (a.empty()) return res;
    res.emplace_back(a[0], 1);
    for (int i = 1; i < (int)a.size(); ++i) {
        if (res.back().first == a[i]) {
            res.back().second++;
        } else {
            res.emplace_back(a[i], 1);
        }
    }
    return res;
}

vector<pair<char, int>> RLE(const string &s) {
    vector<pair<char, int>> res;
    if (s.empty()) return res;
    res.emplace_back(s[0], 1);
    for (int i = 1; i < (int)s.size(); ++i) {
        if (res.back().first == s[i]) {
            res.back().second++;
        } else {
            res.emplace_back(s[i], 1);
        }
    }
    return res;
}
} // namespace titan23
