#include <vector>
#include <array>
#include <queue>
using namespace std;

/// @brief Aho-Corasick
namespace titan23 {
class AhoCorasick {
private:
    char B;
    struct Node {
        int cnt, fail;
        array<int, 26> child;
        Node() : cnt(0), fail(0) {
            child.fill(-1);
        }
    };

public:
    vector<Node> node;
    vector<vector<int>> trie, failtree;
    int root;

    AhoCorasick(char B) : B(B), node(1), root(0), trie(1), failtree(1) {
        node[0].fail = 0;
    }

    void reserve(int cap) {
        node.reserve(cap);
    }

    int add_string(const string &s) {
        int v = 0;
        for (char c : s) {
            c -= B;
            if (node[v].child[c] == -1) {
                node[v].child[c] = node.size();
                node.emplace_back();
                trie.emplace_back();
            }
            trie[v].push_back(node[v].child[c]);
            trie[node[v].child[c]].push_back(v);
            v = node[v].child[c];
        }
        node[v].cnt++;
        return v;
    }

    void build() {
        queue<int> todo;
        for (int i = 0; i < 26; ++i) {
            if (node[0].child[i] != -1) {
                node[node[0].child[i]].fail = 0;
                todo.push(node[0].child[i]);
            } else {
                node[0].child[i] = 0;
            }
        }
        while (!todo.empty()) {
            int v = todo.front(); todo.pop();
            node[v].cnt += node[node[v].fail].cnt;
            for (int i = 0; i < 26; ++i) {
                int x = node[v].child[i];
                if (x != -1) {
                    node[x].fail = node[node[v].fail].child[i];
                    todo.push(x);
                } else {
                    node[v].child[i] = node[node[v].fail].child[i];
                }
            }
        }

        trie.resize(node.size());
        for (vector<int> &t : trie) {
            sort(t.begin(), t.end());
            t.erase(unique(t.begin(), t.end()), t.end());
        }
        failtree.resize(node.size());
        for (int i = 1; i < (int)node.size(); ++i) {
            failtree[i].push_back(node[i].fail);
            failtree[node[i].fail].push_back(i);
        }
    }

    int next(int v, char c) const {
        return node[v].child[c-B];
    }

    Node get(int v) const {
        return node[v];
    }
};
} // namespace titan23
