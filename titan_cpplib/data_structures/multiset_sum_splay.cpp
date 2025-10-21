#include<bits/stdc++.h>
using namespace std;

namespace titan23 {

template<typename T>
class MultisetSumSplay {
private:
    int root;

    struct MemoryAllocator {

        struct Node {
            int par, left, right;
            T key, sum;
            int size;
            Node() : par(0), left(0), right(0), key(T{}), sum(key), size(0) {}
            Node(int par, int left, int right, T &key, T &sum, int size) :
                    par(par), left(left), right(right), key(key), sum(sum), size(size) {}
        };

        vector<Node> d;
        int ptr;
        MemoryAllocator() : ptr(1) {
            d.resize(1);
            d[0].par = 0;
            d[0].left = 0;
            d[0].right = 0;
            d[0].key = T{};
            d[0].sum = T{};
            d[0].size = 0;
        }

        int new_node(T key) {
            if (d.size() > ptr) {
                d[ptr].par = 0;
                d[ptr].left = 0;
                d[ptr].right = 0;
                d[ptr].key = key;
                d[ptr].sum = key;
                d[ptr].size = 1;
            } else {
                d.emplace_back(0, 0, 0, key, key, 1);
            }
            ptr++;
            return ptr - 1;
        }

        void reserve(int cap) {
            d.reserve(cap);
        }

        void reset() {
            ptr = 1;
        }
    };
    static MemoryAllocator ma;

    void update(int node) {
        auto &nx = ma.d[node];
        nx.size = 1 + ma.d[nx.left].size + ma.d[nx.right].size;
        nx.sum = nx.key + ma.d[nx.left].sum + ma.d[nx.right].sum;
    }

    void rotate(int node) {
        auto &nx = ma.d[node];
        int pnode = nx.par;
        auto &np = ma.d[pnode];
        int gnode = np.par;
        if (gnode) {
            if (ma.d[gnode].left == pnode) {
                ma.d[gnode].left = node;
            } else {
                ma.d[gnode].right = node;
            }
        }
        nx.par = gnode;
        if (np.left == node) {
            np.left = nx.right;
            if (nx.right) ma.d[nx.right].par = pnode;
            nx.right = pnode;
        } else {
            np.right = nx.left;
            if (nx.left) ma.d[nx.left].par = pnode;
            nx.left = pnode;
        }
        np.par = node;

        nx.sum = np.sum;
        nx.size = np.size;
        update(pnode);
        // update(node);
    }

    int splay(int node) {
        while (ma.d[node].par && ma.d[ma.d[node].par].par) {
            int p = ma.d[node].par;
            bool a = ma.d[ma.d[p].par].left == p;
            bool b = ma.d[p].left == node;
            rotate(a == b ? p : node);
            rotate(node);
        }
        if (ma.d[node].par) rotate(node);
        return node;
    }

    int left_splay(int node) {
        while (ma.d[node].left) node = ma.d[node].left;
        return splay(node);
    }

    int right_splay(int node) {
        while (ma.d[node].right) node = ma.d[node].right;
        return splay(node);
    }

    int find_splay(int node, T key) {
        if (!node) return 0;
        int res = node, pnode = node;
        while (node) {
            pnode = node;
            if (ma.d[node].key < key || ma.d[node].key == key) {
                res = node;
                node = ma.d[node].right;
            } else {
                node = ma.d[node].left;
            }
        }
        splay(pnode);
        return splay(res);
    }

    int kth_splay(int node, int k) {
        while (1) {
            int t = ma.d[ma.d[node].left].size;
            if (t == k) break;
            if (t > k) {
                node = ma.d[node].left;
            } else {
                node = ma.d[node].right;
                k -= t + 1;
            }
        }
        return splay(node);
    }

    void remove_root() {
        assert(root && ma.d[root].par == 0);
        int new_root = 0;
        if (!ma.d[root].left) {
            new_root = ma.d[root].right;
        } else if (!ma.d[root].right) {
            new_root = ma.d[root].left;
        } else {
            new_root = ma.d[root].left;
            ma.d[new_root].par = 0;
            new_root = right_splay(new_root);
            ma.d[new_root].right = ma.d[root].right;
            ma.d[ma.d[new_root].right].par = new_root;
            update(new_root);
        }
        if (new_root) ma.d[new_root].par = 0;
        root = new_root;
    }

    MultisetSumSplay(int root) : root(root) {}

    // leftのsize==k
    pair<int, int> split_node_kth(int node, int k) {
        if (node == 0 || k <= 0) return make_pair(0, node);
        if (k >= ma.d[node].size) return make_pair(node, 0);
        node = kth_splay(node, k);
        int left_root = ma.d[node].left;
        if (left_root) {
            ma.d[left_root].par = 0;
            ma.d[node].left = 0;
            update(node);
        }
        return make_pair(left_root, node);
    }

    int merge_node(int left, int right) {
        if (left == 0) return right;
        if (right == 0) return left;
        left = right_splay(left);
        ma.d[left].right = right;
        ma.d[right].par = left;
        update(left);
        return left;
    }

public:
    MultisetSumSplay() : root(0) {}
    MultisetSumSplay(vector<T> a) {
        if (a.empty()) {
            root = nullptr;
            return;
        }
        auto build = [&] (auto &&build, int l, int r) -> int {
            int mid = (l + r) / 2;
            int node = ma.new_node(a[mid]);
            if (l != mid) {
                ma.d[node].left = build(build, l, mid);
                ma.d[node].left->par = node;
            }
            if (mid+1 != r) {
                ma.d[node].right = build(build, mid+1, r);
                ma.d[node].right->par = node;
            }
            update(ma.d[node]);
            return node;
        };
        root = build(build, 0, a.size());
    }

    pair<MultisetSumSplay<T>, MultisetSumSplay<T>> split(int k) {
        auto [left, right] = split_node_kth(root, k);
        return {MultisetSumSplay(left), MultisetSumSplay(right)};
    }

    void merge(MultisetSumSplay<T> &other) {
        root = merge_node(root, other.root);
        other.root = nullptr;
    }

    void print_node(int node) {
        stack<int> st;
        vector<T> a;
        while ((!st.empty()) || node) {
            if (node) {
                st.emplace(node);
                node = ma.d[node].left;
            } else {
                node = st.top();
                st.pop();
                a.emplace_back(ma.d[node].key);
                node = ma.d[node].right;
            }
        }
        cout << "[";
        int n = a.size();
        for (int i = 0; i < n; ++i) {
            cout << a[i] << ", ";
        }
        cout << "]" << endl;
    }

    //! vectorにしたときの区間[l, r)の和
    T range_sum_at(int l, int r) {
        assert(0 <= l && l <= r && r <= len());
        int a, b, c;
        tie(b, c) = split_node_kth(root, r);
        tie(a, b) = split_node_kth(b, l);
        T res = ma.d[b].sum;
        a = merge_node(a, b);
        a = merge_node(a, c);
        root = a;
        return res;
    }

    bool discard(const T key) {
        if (root == 0) return false;
        root = find_splay(root, key);
        if (ma.d[root].key == key) {
            remove_root();
            return true;
        }
        return false;
    }

    void remove(const T key) {
        root = find_splay(root, key);
        assert(ma.d[root].key == key);
        remove_root();
    }

    T pop(int k) {
        assert(0 <= k && k < len());
        root = kth_splay(root, k);
        T res = ma.d[root].key;
        remove_root();
        return res;
    }

    void add(T key) {
        int node = ma.new_node(key);
        if (!root) {
            root = node;
            return;
        }
        root = find_splay(root, key);
        if (ma.d[root].key <= ma.d[node].key) {
            ma.d[node].right = ma.d[root].right;
            if (ma.d[node].right) ma.d[ma.d[node].right].par = node;
            ma.d[root].right = 0;
            ma.d[node].left = root;
            ma.d[ma.d[node].left].par = node;
        } else {
            ma.d[node].right = root;
            if (ma.d[node].right) ma.d[ma.d[node].right].par = node;
        }
        update(root);
        update(node);
        root = node;
    }

    T get(int k) {
        assert(0 <= k && k < len());
        root = kth_splay(root, k);
        return ma.d[root].key;
    }

    int len() const {
        return ma.d[root].size;
    }

    int get_height() const {
        auto rec = [&] (auto &&rec, int node) -> int {
            if (node == 0) return 0;
            int h = 0;
            if (ma.d[node].left) h = max(h, rec(rec, ma.d[node].left));
            if (ma.d[node].right) h = max(h, rec(rec, ma.d[node].right));
            return h + 1;
        };
        return rec(rec, root);
    }

    // 総和がw未満となるように先頭からとるとき、いくつとれるか？
    int count_sumlim(T w) {
        int ans = 0;
        T now = 0;
        int node = root, pnode = 0;
        while (node) {
            pnode = node;
            if (now + ma.d[ma.d[node].left].sum+ma.d[node].key < w) {
                now += ma.d[ma.d[node].left].sum+ma.d[node].key;
                ans += ma.d[ma.d[node].left].size + 1;
                node = ma.d[node].right;
            } else {
                node = ma.d[node].left;
            }
        }
        if (pnode) root = splay(pnode);
        return ans;
    }

    // key未満の要素数を返す
    int index(T key) {
        if (!root) return 0;
        int ans = 0;
        int node = root, pnode = 0;
        while (node) {
            pnode = node;
            if (ma.d[node].key < key) {
                ans += ma.d[ma.d[node].left].size + 1;
                node = ma.d[node].right;
            } else {
                node = ma.d[node].left;
            }
        }
        if (pnode) root = splay(pnode);
        return ans;
    }

    // key以下の要素数を返す
    int index_right(T key) {
        if (!root) return 0;
        int ans = 0;
        int node = root, pnode = 0;
        while (node) {
            pnode = node;
            if (ma.d[node].key <= key) {
                ans += ma.d[ma.d[node].left].size + 1;
                node = ma.d[node].right;
            } else {
                node = ma.d[node].left;
            }
        }
        if (pnode) root = splay(pnode);
        return ans;
    }

    // low以上high未満の要素数を返す
    int count_range(T low, T high) {
        assert(low <= high);
        return index(high) - index(low);
    }

    void reserve(int cap) {
        ma.reserve(cap);
    }

    void clear() {
        ma.reset();
        root = 0;
    }

    vector<T> tovector() const {
        int node = root;
        stack<int> st;
        vector<T> a;
        a.reserve(len());
        while ((!st.empty()) || node) {
            if (node) {
                st.emplace(node);
                node = ma.d[node].left;
            } else {
                node = st.top();
                st.pop();
                a.emplace_back(ma.d[node].key);
                node = ma.d[node].right;
            }
        }
        return a;
    }

    void test_sorted() const {
        vector<T> a = tovector();
        int n = a.size();
        for (int i = 0; i < n-1; ++i) {
            assert(a[i] <= a[i+1]);
        }
    }

    void test() const {
        auto dfs = [&] (auto &&dfs, int node, int pnode) {
            if (node == 0) return;
            assert(ma.d[node].par == pnode);
            dfs(dfs, ma.d[node].left, node);
            dfs(dfs, ma.d[node].right, node);
        };
        dfs(dfs, root, 0);
    }

    friend ostream& operator<<(ostream& os, const MultisetSumSplay<T> &S) {
        vector<T> a = S.tovector();
        int n = a.size();
        os << "{";
        for (int i = 0; i < n-1; ++i) {
            os << a[i] << ", ";
        }
        if (n-1 >= 0) {
            os << a[n-1];
        }
        os << "}";
        return os;
    }
};

template<typename T> typename MultisetSumSplay<T>::MemoryAllocator MultisetSumSplay<T>::ma;
}
