#include<bits/stdc++.h>
using namespace std;

namespace titan23 {

template<typename T>
class MultisetSum {
  private:
    struct Node;
    using NodePtr = Node*;
    stack<NodePtr> unused_node;
    NodePtr root;

    struct Node {
        int size;
        NodePtr par, left, right;
        T key, sum;

        Node() : size(0), par(nullptr), left(nullptr), right(nullptr) {}

        Node(T key) : size(1), par(nullptr), left(nullptr), right(nullptr), key(key), sum(key) {}

        void init(T &key) {
            this->size = 1;
            this->par = nullptr;
            this->left = nullptr;
            this->right = nullptr;
            this->key = key;
            this->sum = key;
        }

        void update() {
            this->size = 1;
            this->sum = this->key;
            if (this->left) {
                this->size += this->left->size;
                this->sum += this->left->sum;
            }
            if (this->right) {
                this->size += this->right->size;
                this->sum += this->right->sum;
            }
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

    NodePtr find_splay(NodePtr node, const T &key) {
        NodePtr pnode = nullptr;
        while (node) {
            if (node->key == key) {
                node->splay();
                return node;
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
            return pnode;
        }
        return node;
    }

    NodePtr kth_splay(NodePtr node, int k) {
        while (true) {
            int t = node->left ? node->left->size : 0;
            if (t == k) {
                node->splay();
                return node;
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
            new_root->update();
        }
        if (new_root) new_root->par = nullptr;
        this->root = new_root;
    }

    MultisetSum(NodePtr root) : root(root) {}

    // leftのsize==k
    pair<NodePtr, NodePtr> split_node_kth(NodePtr node, int k) {
        if (node == nullptr || k <= 0) return make_pair(nullptr, node);
        if (k >= node->size) return make_pair(node, nullptr);
        node = this->kth_splay(node, k);
        NodePtr left_root = node->left;
        if (left_root) {
            left_root->par = nullptr;
            node->left = nullptr;
            node->update();
        }
        return make_pair(left_root, node);
    }

    NodePtr merge_node(NodePtr left, NodePtr right) {
        if (left == nullptr) return right;
        if (right == nullptr) return left;
        left = left->right_splay();
        left->right = right;
        right->par = left;
        left->update();
        return left;
    }
    MultisetSum<T> gen(NodePtr root_node) const {
        return MultisetSum<T>(root_node);
    }

  public:
    MultisetSum() : root(nullptr) {}

    pair<MultisetSum<T>, MultisetSum<T>> split(int k) {
        auto [left, right] = split_node_kth(this->root, k);
        return make_pair(gen(left), gen(right));
    }

    void merge(MultisetSum<T> &other) {
        this->root = merge_node(this->root, other->root);
    }

    void print_node(NodePtr node) {
        stack<NodePtr> st;
        vector<T> a;
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
        cout << "[";
        int n = a.size();
        for (int i = 0; i < n; ++i) {
            cout << a[i] << ", ";
        }
        cout << "]" << endl;
    }

    //! [l, r)の和
    T sum(int l, int r) {
        NodePtr a, b, c;
        tie(b, c) = split_node_kth(this->root, r);
        tie(a, b) = split_node_kth(b, l);
        T res = b ? b->sum : 0;
        a = merge_node(a, b);
        a = merge_node(a, c);
        this->root = a;
        return res;
    }

    bool discard(const T &key) {
        if (this->root == nullptr) return false;
        this->root = this->find_splay(this->root, key);
        if (this->root->key == key) {
            remove_root();
            return true;
        }
        return false;
    }

    void remove(const T &key) {
        assert(this->root != nullptr);
        this->root = this->find_splay(this->root, key);
        assert(this->root->key == key);
        remove_root();
    }

    T pop(int k) {
        assert(this->root != nullptr);
        this->root = this->kth_splay(this->root, k);
        T res = this->root->key;
        remove_root();
        return res;
    }

    void add(T key) {
        this->root = this->find_splay(this->root, key);
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

    T get(int k) {
        this->root = this->kth_splay(this->root, k);
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

    void test() const {
        auto dfs = [&] (auto dfs, NodePtr node, NodePtr pnode) {
            if (node == nullptr) return;
            assert(node->par == pnode);
            dfs(dfs, node->left, node);
            dfs(dfs, node->right, node);
        };
        dfs(dfs, this->root, nullptr);
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
}
