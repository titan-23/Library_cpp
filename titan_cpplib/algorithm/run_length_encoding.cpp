#include <vector>
#include <string>

namespace titan23 {

template<typename T>
vector<pair<T, int>> rle(const vector<T> &A) {
    vector<pair<T, int>> ret;
    if (A.empty()) return ret;
    T now = A[0];
    int cnt = 1;
    for (int i = 1; i < (int)A.size(); ++i) {
        if (A[i] == now) {
            ++cnt;
        } else {
            ret.emplace_back(now, cnt);
            now = A[i];
            cnt = 1;
        }
    }
    ret.emplace_back(now, cnt);
    return ret;
}

vector<pair<char,int>> rle(const string &S) {
    vector<pair<char,int>> ret;
    if (S.empty()) return ret;
    char now = S[0];
    int cnt = 1;
    for (int i = 1; i < (int)S.size(); ++i) {
        if (S[i] == now) ++cnt;
        else {
            ret.emplace_back(now, cnt);
            now = S[i];
            cnt = 1;
        }
    }
    ret.emplace_back(now, cnt);
    return ret;
}
} // namespace titan23
