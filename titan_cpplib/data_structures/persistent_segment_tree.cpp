#include <vector>
#include <stack>
#include <memory>
using namespace std;

// PersistentSegmentTree
namespace titan23 {

    template <class T,
              T (*op)(T, T),
              T (*e)()>
    class PersistentSegmentTree {
      private:
        struct Node;

        using NodePtr = shared_ptr<Node>;
        // using NodePtr = Node*;

        NodePtr root;

        struct Node {
            T key, data;
            int size;
            NodePtr left, right;

            Node() : size(0), left(nullptr), right(nullptr) {}
            Node(T key) : key(key), data(key), size(1), left(nullptr), right(nullptr) {}

            NodePtr copy() {
                NodePtr node = make_shared<Node>(this->key);
                // NodePtr node = new Node(this->key);
                node->data = this->data;
                node->size = this->size;
                node->left = this->left;
                node->right = this->right;
                return node;
            }

            void update() {
                this->size = 1;
                this->data = this->key;
                if (this->left) {
                    this->size += this->left->size;
                    this->data = op(this->left->data, this->data);
                }
                if (this->right) {
                    this->size += this->right->size;
                    this->data = op(this->data, this->right->data);
                }
            }
        };

        void _build(const vector<T> &a) {
            auto build = [&] (auto &&build, int l, int r) -> NodePtr {
                int mid = (l + r) >> 1;
                NodePtr node = make_shared<Node>(a[mid]);
                // NodePtr node = new Node(a[mid]);
                if (l != mid) node->left = build(build, l, mid);
                if (mid+1 != r) node->right = build(build, mid+1, r);
                node->update();
                return node;
            };

            if (a.empty()) {
                this->root = nullptr;
                return;
            }
            this->root = build(build, 0, (int)a.size());
        }

        PersistentSegmentTree(NodePtr root) : root(root) {}

        PersistentSegmentTree<T, op, e> _new(NodePtr root) const {
            return PersistentSegmentTree<T, op, e>(root);;
        }

      public:
        PersistentSegmentTree() : root(nullptr) {}
        PersistentSegmentTree(const vector<T> a) {
            _build(a);
        }

        T prod(int l, int r) const {
            assert(0 <= l && l <= r && r <= len());

            auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> T {
                if (right <= l || r <= left) return e();
                if (l <= left && right < r) return node->data;
                int lsize = node->left ? node->left->size : 0;
                T res = e();
                if (node->left) {
                    res = dfs(dfs, node->left, left, left+lsize);
                }
                if (l <= left + lsize && left + lsize < r) {
                    res = op(res, node->key);
                }
                if (node->right) {
                    res = op(res, dfs(dfs, node->right, left+lsize+1, right));
                }
                return res;
            };

            return dfs(dfs, this->root, 0, len());
        }

        PersistentSegmentTree<T, op, e> set(int k, T v) const {
            assert(this->root);
            NodePtr node = this->root->copy();
            NodePtr nroot = node;
            NodePtr pnode = nullptr;
            int d = 0;
            stack<NodePtr> path;
            path.emplace(node);
            while (true) {
                int t = (node->left) ? node->left->size : 0;
                if (t == k) {
                    node = node->copy();
                    node->key = v;
                    path.emplace(node);
                    if (pnode) {
                        if (d) {
                            pnode->left = node;
                        } else {
                            pnode->right = node;
                        }
                    } else {
                        nroot = node;
                    }
                    while (!path.empty()) {
                        node = path.top();
                        path.pop();
                        node->update();
                    }
                    return _new(nroot);
                }

                pnode = node;
                if (t < k) {
                    k -= t + 1;
                    d = 0;
                    node = node->right->copy();
                    pnode->right = node;
                } else {
                    d = 1;
                    node = node->left->copy();
                    pnode->left = node;
                }
                path.emplace(node);
            }
        }

        T get(int k) const {
            assert(0 <= k && k < len());
            assert(this->root);
            NodePtr node = this->root;
            while (true) {
                int t = node->left ? node->left->size : 0;
                if (t == k) {
                    return node->key;
                }
                if (t < k) {
                    k -= t + 1;
                    node = node->right;
                } else {
                    node = node->left;
                }
            }
        }

        PersistentSegmentTree<T, op, e> copy() {
            return _new(this->root ? this->root->copy() : nullptr);
        }

        vector<T> tolist() const {
            vector<T> a;
            a.reserve(len());
            NodePtr node = root;
            stack<NodePtr> s;
            while (!s.empty() || node) {
                if (node) {
                    s.emplace(node);
                    node = node->left;
                } else {
                    node = s.top();
                    s.pop();
                    a.emplace_back(node->key);
                    node = node->right;
                }
            }
            return a;
        }

        int len() const {
            return this->root ? this->root->size : 0;
        }

        void print() const {
            vector<T> a = tolist();
            cout << "[";
            for (int i = 0; i < (int)a.size(); ++i) {
                cout << a[i];
                if (i != (int)a.size()-1) {
                    cout << ", ";
                }
            }
            cout << "]" << endl;
        }
    };
}  // namespace titan23
