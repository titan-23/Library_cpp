#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
using namespace std;

// print

// color
static const string PRINT_RED = "\033[91m"; // 赤字
static const string PRINT_GREEN = "\033[92m"; // 緑字
static const string PRINT_BLUE = "\033[94m";  // 青字
static const string PRINT_NONE = "\033[m"; // 色を元に戻す

string to_red(const string s) {
    return PRINT_RED + s + PRINT_NONE;
}

string to_green(const string s) {
    return PRINT_GREEN + s + PRINT_NONE;
}

string to_blue(const string s) {
    return PRINT_BLUE + s + PRINT_NONE;
}

// pair<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t) {
    os << "( " << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << " )";
    return os;
}

// tuple<T1, T2, T3, T4>
template<typename T1, typename T2, typename T3, typename T4>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3, T4> &t) {
    os << "( " << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << ", " << get<3>(t) << " )";
    return os;
}

// vector<T>
template <typename T>
ostream& operator<<(ostream& os, const vector<T>& a) {
    int n = (int)a.size();
    os << "[";
    for (int i = 0; i < n-1; ++i) {
        os << a[i] << ", ";
    }
    if (n > 0) {
        os << a.back();
    }
    os << "]";
    return os;
}

// vector<vector<T>>
template <typename T>
ostream& operator<<(ostream& os, const vector<vector<T>>& a) {
    os << "[\n";
    int h = (int)a.size();
    for (int i = 0; i < h; ++i) {
        os << "  " << a[i] << '\n';
    }
    os << "]";
    return os;
}

// set<T>
template <typename T>
ostream& operator<<(ostream& os, const set<T>& s) {
    os << "{";
    auto it = s.begin();
    while (it != s.end()) {
        os << *it;
        ++it;
        if (it != s.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_set<T>
template <typename T>
ostream& operator<<(ostream& os, const unordered_set<T>& a) {
    set<T> s;
    for (const T &x : a) {
        s.insert(x);
    }
    os << s;
    return os;
}

// map<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& mp) {
    os << "{";
    auto it = mp.begin();
    while (it != mp.end()) {
        os << it->first << ": " << it->second;
        ++it;
        if (it != mp.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_map<K, V>
template <typename K, typename V>
ostream& operator<<(ostream& os, const unordered_map<K, V>& mp) {
    map<K, V> m;
    for (const auto &[k, v] : mp) {
        m[k] = v;
    }
    os << m;
    return os;
}
