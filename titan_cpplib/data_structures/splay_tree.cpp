#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

#define rep(i, n) for (int i = 0; i < (n); ++i)
#define ll long long

template<typename T>
class DynamicList {
  private:
    struct Node;
    using NodePtr = Node*;
    stack<NodePtr> unused_node;
    NodePtr root;

    struct Node {
        int size;
        NodePtr par, left, right;
        T key;

        Node() : size(0), par(nullptr), left(nullptr), right(nullptr) {}
        Node(T key) : key(key), size(1), par(nullptr), left(nullptr), right(nullptr) {}

        void init(T &key) {
            this->size = 1;
            this->par = nullptr;
            this->left = nullptr;
            this->right = nullptr;
            this->key = key;
        }

        void update() {
            this->size = 1;
            if (this->left) this->size += this->left->size;
            if (this->right) this->size += this->right->size;
        }

        void rotate_right() {
            NodePtr u = this->left;
            assert(u);
            this->left = u->right;
            u->right = this;
            if (this->par) {
                if (this->par->left == this) {
                    this->par->left = u;
                } else {
                    assert(this->par->right == this);
                    this->par->right = u;
                }
            }
            u->par = this->par;
            if (this->left) this->left->par = this;
            this->par = u;
            this->update();
            u->update();
        }

        void rotate_left() {
            NodePtr u = this->right;
            assert(u);
            this->right = u->left;
            u->left = this;
            if (this->par) {
                if (this->par->left == this) {
                    this->par->left = u;
                } else {
                    assert(this->par->right == this);
                    this->par->right = u;
                }
            }
            u->par = this->par;
            if (this->right) this->right->par = this;
            this->par = u;
            this->update();
            u->update();
        }

        void splay() {
            while (this->par && this->par->par) {
                if (this->par->left == this) {
                    if (this->par->par->left == this->par) {
                        this->par->par->rotate_right();
                        this->par->rotate_right();
                    } else {
                        this->par->rotate_right();
                        this->par->rotate_left();
                    }
                } else {
                    if (this->par->par->right == this->par) {
                        this->par->par->rotate_left();
                        this->par->rotate_left();
                    } else {
                        this->par->rotate_left();
                        this->par->rotate_right();
                    }
                }
            }

            if (this->par) {
                if (this->par->left == this) {
                    this->par->rotate_right();
                } else {
                    this->par->rotate_left();
                }
            }

            assert(this->par == nullptr);
        }

        NodePtr left_splay() {
            NodePtr node = this;
            while (node->left) node = node->left;
            node->splay();
            assert(node->left == nullptr);
            return node;
        }

        NodePtr right_splay() {
            NodePtr node = this;
            while (node->right) node = node->right;
            node->splay();
            assert(node->right == nullptr);
            return node;
        }
    };

    void find_splay(const T &key) {
        NodePtr node = root;
        stack<NodePtr> st;
        while (!st.empty() || node) {
            if (node) {
                if (node->key == key) break;
                st.emplace(node);
                node = node->left;
            } else {
                node = st.top();
                st.pop();
                node = node->right;
            }
        }
        node->splay();
        this->root = node;
    }

    void find_sorted_splay(const T &key) {
        NodePtr node = this->root, pnode = nullptr;
        while (node) {
            if (node->key == key) {
                node->splay();
                this->root = node;
                return;
            }
            pnode = node;
            if (key < node->key) {
                node = node->left;
            } else {
                node = node->right;
            }
        }
        if (pnode) {
            pnode->splay();
            this->root = pnode;
        }
    }

    void kth_splay(int k) {
        NodePtr node = this->root;
        while (true) {
            int t = node->left ? node->left->size : 0;
            if (t == k) {
                node->splay();
                this->root = node;
                return;
            }
            if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    void remove_root() {
        assert(this->root && this->root->par == nullptr);
        unused_node.emplace(this->root);
        NodePtr new_root;
        if (!this->root->left) {
            new_root = this->root->right;
        } else if (!this->root->right) {
            new_root = this->root->left;
        } else {
            new_root = this->root->left;
            new_root->par = nullptr;
            new_root = new_root->right_splay();
            new_root->right = this->root->right;
            new_root->right->par = new_root;
        }
        if (new_root) new_root->par = nullptr;
        this->root = new_root;
    }

    DynamicList(NodePtr root) : root(root) {}

    DynamicList<T> gen(NodePtr root_node) const {
        return DynamicList<T>(root_node);
    }

  public:
    DynamicList() : root(nullptr) {}

    pair<DynamicList<T>, DynamicList<T>> split(int k) {
        this->kth_splay(k);
        NodePtr right_root = this->root->right;
        right_root->par = nullptr;
        this->root->right = nullptr;
        this->root->update();
        return make_pair(gen(this->root), gen(right_root));
    }

    void merge(DynamicList<T> &other) {
        if (this->root == nullptr) {
            this->root = other->root;
        } else if (other->root == nullptr) {
        } else {
            this->root->right_splay();
            this->root->right = other->root;
            other->root->par = this->root;
            this->root->update();
        }
    }

    bool discard_if_sorted(const T &key) {
        assert(this->root != nullptr);
        this->find_sorted_splay(key);
        if (this->root->key == key) {
            remove_root();
            return true;
        }
        return false;
    }

    bool discard(const T &key) {
        assert(this->root != nullptr);
        this->find_splay(key);
        if (this->root->key == key) {
            remove_root();
            return true;
        }
        return false;
    }

    void remove_sorted(const T &key) {
        assert(this->root != nullptr);
        this->find_sorted_splay(key);
        assert(this->root->key == key);
        remove_root();
    }

    void remove(const T &key) {
        assert(this->root != nullptr);
        this->find_splay(key);
        assert(this->root->key == key);
        remove_root();
    }

    T pop(int k) {
        assert(this->root != nullptr);
        this->kth_splay(k);
        T res = this->root->key;
        remove_root();
        return res;
    }

    void insert_sorted(T key) {
        this->find_sorted_splay(key);
        NodePtr node;
        if (unused_node.empty()) {
            node = new Node(key);
        } else {
            node = unused_node.top();
            unused_node.pop();
            node->init(key);
        }
        if (this->root) {
            if (this->root->key >= key) {
                node->left = this->root->left;
                if (node->left) node->left->par = node;
                this->root->left = nullptr;
                node->right = this->root;
                node->right->par = node;
            } else {
                node->right = this->root->right;
                if (node->right) node->right->par = node;
                this->root->right = nullptr;
                node->left = this->root;
                node->left->par = node;
            }
            this->root->update();
            node->update();
        }
        assert(node->par == nullptr);
        this->root = node;
    }

    void insert(int k, T key) {
        this->kth_splay(k);
        NodePtr node;
        if (unused_node.empty()) {
            node = new Node(key);
        } else {
            node = unused_node.top();
            unused_node.pop();
            node->init(key);
        }
        if (this->root) {
            node->left = this->root->left;
            if (node->left) node->left->par = node;
            this->root->left = nullptr;
            node->right = this->root;
            node->right->par = node;
            this->root->update();
            node->update();
        }
        this->root = node;
    }

    void emplace_back(T key) {
        NodePtr node;
        if (unused_node.empty()) {
            node = new Node(key);
        } else {
            node = unused_node.top();
            unused_node.pop();
            node->init(key);
        }
        if (this->root == nullptr) {
            this->root = node;
            return;
        }
        node->left = this->root;
        this->root->par = node;
        node->update();
        this->root = node;
    }

    T pop_back() {
        NodePtr node = this->root->right_splay();
        T res = node->key;
        remove_root();
        return res;
    }

    T get(int k) {
        this->kth_splay(k);
        return this->root->key;
    }

    int len() const {
        return this->root ? this->root->size : 0;
    }

    int get_height() const {
        auto rec = [&] (auto &&rec, NodePtr node) -> int {
            if (node == nullptr) return 0;
            int h = 0;
            if (node->left) h = max(h, rec(rec, node->left));
            if (node->right) h = max(h, rec(rec, node->right));
            return h + 1;
        };
        return rec(rec, this->root);
    }

    vector<T> tovector() const {
        NodePtr node = this->root;
        stack<NodePtr> st;
        vector<T> a;
        a.reserve(len());
        while ((!st.empty()) || node) {
            if (node) {
                st.emplace(node);
                node = node->left;
            } else {
                node = st.top();
                st.pop();
                a.emplace_back(node->key);
                node = node->right;
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

    void print() const {
        vector<T> a = tovector();
        int n = a.size();
        cout << "[";
        for (int i = 0; i < n-1; ++i) {
            cout << a[i] << ", ";
        }
        if (n-1 >= 0) {
            cout << a[n-1];
        }
        cout << "]" << endl;
    }
};

#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"

void solve() {
    int q;
    cin >> q;
    DynamicList<int> s;
    rep(_, q) {
        int t, x;
        cin >> t >> x;
        if (t == 1) {
            s.insert_sorted(x);
        } else {
            cout << s.pop(x-1) << '\n';
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(15);
    solve();
    return 0;
}
