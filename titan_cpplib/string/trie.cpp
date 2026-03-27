#include <iostream>
#include <vector>
#include <string>
#include <array>
using namespace std;

namespace titan23 {

template <char B='a', int ALPHABET_SIZE=26>
class Trie {
private:
    struct Node {
        array<int, ALPHABET_SIZE> child;
        int count = 0;
        int stop_count = 0;

        Node() {
            child.fill(-1);
        }
    };

    vector<Node> a;
    int root;

    int new_node() {
        a.push_back(Node());
        return a.size() - 1;
    }

public:
    Trie() {
        root = new_node();
    }

    /// @brief ルートノードのIDを取得する
    int get_root() const {
        return root;
    }

    /// @brief 指定したノードと文字インデックスから子ノードのIDを取得する
    int get_child(int v, int idx) const {
        return a[v].child[idx];
    }

    /// @brief ノードのcountを取得する
    int get_count(int v) const {
        return a[v].count;
    }

    /// @brief ノードのstop_countを取得する
    int get_stop_count(int v) const {
        return a[v].stop_count;
    }

    /// @brief sを追加する
    void add(const string &s) {
        int v = root;
        for (char c : s) {
            a[v].count++;
            int idx = c - B;
            if (a[v].child[idx] == -1) {
                a[v].child[idx] = new_node();
            }
            v = a[v].child[idx];
        }
        a[v].stop_count++;
    }

    /// @brief prefの接頭辞の中で最長の文字列の長さを返す
    int s_prefix(const string &pref) const {
        int v = root;
        for (int i = 0; i < pref.size(); ++i) {
            int idx = pref[i] - B;
            if (a[v].child[idx] == -1) {
                return i;
            }
            v = a[v].child[idx];
        }
        return pref.size();
    }

    /// @brief sがあれば一つ削除する / O(|s|)
    void erase(const string &s) {
        if (!contains(s)) return;
        int v = root;
        for (char c : s) {
            a[v].count--;
            int idx = c - B;
            v = a[v].child[idx];
        }
        a[v].stop_count--;
        v = root;
        for (char c : s) {
            int idx = c - B;
            int next_idx = a[v].child[idx];
            if (a[next_idx].count + a[next_idx].stop_count == 0) {
                a[v].child[idx] = -1;
                break;
            }
            v = next_idx;
        }
    }

    /// @brief prefを接頭辞に持つすべての文字列を削除し、削除した文字列の個数を返す / O(|pref|)
    int erase_prefix(const string &pref) {
        int v = root;
        for (char c : pref) {
            int idx = c - B;
            if (a[v].child[idx] == -1) return 0;
            v = a[v].child[idx];
        }
        int remove_cnt = a[v].count + a[v].stop_count;
        v = root;
        if (a[v].count - remove_cnt == 0) {
            a.clear();
            root = new_node();
            return remove_cnt;
        }

        for (char c : pref) {
            a[v].count -= remove_cnt;
            int idx = c - B;
            int next_idx = a[v].child[idx];
            if ((a[next_idx].count + a[next_idx].stop_count) - remove_cnt == 0) {
                a[v].child[idx] = -1;
                return remove_cnt;
            }
            v = next_idx;
        }
        return remove_cnt;
    }

    /// @brief prefを接頭辞に持つ文字列があるかどうか / O(|pref|)
    bool contains_prefix(const string &pref) const {
        int v = root;
        for (char c : pref) {
            int idx = c - B;
            if (a[v].child[idx] == -1) return false;
            v = a[v].child[idx];
        }
        return true;
    }

    /// @brief sの接頭辞になり得る文字列が存在しているか / O(|s|)
    bool contains_prefix_inv(const string &s) const {
        int v = root;
        for (char c : s) {
            int idx = c - B;
            if (a[v].child[idx] == -1) return false;
            if (a[v].stop_count > 0) return true;
            v = a[v].child[idx];
            if (a[v].stop_count > 0) return true;
        }
        return false;
    }

    /// @brief sを接頭辞に持つ文字列がいくつあるか
    /// @return a[i]:= s[:i]スタートの文字列がいくつあるか
    vector<int> count_prefix(const string &s) const {
        int v = root;
        int n = s.size();
        vector<int> ans(n + 1, 0);
        for (int i = 0; i < n; ++i) {
            ans[i] = a[v].count + a[v].stop_count;
            int idx = s[i] - B;
            if (a[v].child[idx] == -1) return ans;
            v = a[v].child[idx];
        }
        ans[n] = a[v].count + a[v].stop_count;
        return ans;
    }

    /// @brief sがいくつ含まれているか
    int count(const string &s) const {
        int v = root;
        for (char c : s) {
            int idx = c - B;
            if (a[v].child[idx] == -1) return 0;
            v = a[v].child[idx];
        }
        return a[v].stop_count;
    }

    vector<string> tovector(bool unique=false) const {
        vector<string> res;
        string s;

        auto dfs = [&] (auto &&dfs, int v) -> void {
            if (v == -1) return;
            if (a[v].stop_count > 0) {
                if (unique) {
                    res.push_back(s);
                } else {
                    for (int i = 0; i < a[v].stop_count; ++i) {
                        res.push_back(s);
                    }
                }
            }
            for (int i = 0; i < ALPHABET_SIZE; ++i) {
                if (a[v].child[i] != -1) {
                    s.push_back(static_cast<char>(B + i));
                    dfs(dfs, a[v].child[i]);
                    s.pop_back();
                }
            }
        };

        dfs(dfs, root);
        return res;
    }

    int size() const {
        return a[root].count;
    }

    bool contains(const string &s) const {
        return count(s) > 0;
    }

    void print() const {
        auto dfs = [&] (auto &&dfs, int v, const string &indent) -> void {
            int child_count = 0;
            for (int i = 0; i < ALPHABET_SIZE; ++i) {
                if (a[v].child[i] != -1) child_count++;
            }
            if (child_count == 0) return;

            int done = 0;
            for (int i = 0; i < ALPHABET_SIZE; ++i) {
                if (a[v].child[i] != -1) {
                    done++;
                    char c = static_cast<char>(B + i);
                    string c_str(1, c);
                    int child_idx = a[v].child[i];

                    if (a[child_idx].stop_count > 0) {
                        c_str = "\033[32m" + c_str + "\033[m";
                    }

                    if (done < child_count) {
                        cout << indent << "├── " << c_str << "\n";
                        dfs(dfs, child_idx, indent + "|   ");
                    } else {
                        cout << indent << "└── " << c_str << "\n";
                        dfs(dfs, child_idx, indent + "    ");
                    }
                }
            }
        };

        cout << "root\n";
        dfs(dfs, root, "");
    }
};
} // namespace titan23
