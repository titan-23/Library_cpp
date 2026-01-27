#include <vector>
using namespace std;

namespace titan23 {
struct SuffixAutomaton {
    struct Node {
        int len, link, to[26];
        Node() : len(0), link(-1) { fill(to, to+26, -1); }
    };

    vector<Node> a;
    int last;

    SuffixAutomaton() : last(0) { a.emplace_back(); }

    void add(char c) {
        int now = a.size();
        a.emplace_back();
        a[now].len = a[last].len + 1;
        int p = last, idx = c - 'a';
        while (p != -1 && a[p].to[idx] == -1) {
            a[p].to[idx] = now;
            p = a[p].link;
        }
        if (p == -1) {
            a[now].link = 0;
        } else {
            int q = a[p].to[idx];
            if (a[p].len + 1 == a[q].len) {
                a[now].link = q;
            } else {
                int clone = a.size();
                a.push_back(a[q]);
                a[clone].len = a[p].len + 1;
                while (p != -1 && a[p].to[idx] == q) {
                    a[p].to[idx] = clone;
                    p = a[p].link;
                }
                a[q].link = a[now].link = clone;
            }
        }
        last = now;
    }
};
} // namespace titan23
