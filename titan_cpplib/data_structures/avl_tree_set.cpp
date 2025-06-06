#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include <stack>
#include "titan_cpplib/data_structures/bbst_node.cpp"
using namespace std;

// AVLTreeSet
namespace titan23 {

template<typename T>
class AVLTreeSet {
public:
    class AVLTreeSetNode {
    public:
        using AVLTreeSetNodePtr = AVLTreeSetNode*;
        T key;
        int size;
        char height;
        AVLTreeSetNodePtr par, left, right;

        AVLTreeSetNode() : size(0), height(0), par(nullptr), left(nullptr), right(nullptr) {}
        AVLTreeSetNode(const T &key) : key(key), size(1), height(1), par(nullptr), left(nullptr), right(nullptr) {}

        constexpr char balance() const {
            return (left ? left->height : 0) - (right ? right->height : 0);
        }

        constexpr void update() {
            size = 1 + (left ? left->size : 0) + (right ? right->size : 0);
            height = 1 + max((left ? left->height : 0), (right ? right->height : 0));
        }
    };

private:
    T missing;
    using AVLTreeSetNodePtr = AVLTreeSetNode*;
    AVLTreeSetNodePtr root;

    constexpr void _remove_balance(AVLTreeSetNodePtr node) {
        while (node) {
            AVLTreeSetNodePtr new_node = nullptr;
            node->update();
            if (node->balance() == 2) {
                new_node = node->left->balance() == -1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_LR(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_right(node);
            } else if (node->balance() == -2) {
                new_node = node->right->balance() == 1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_RL(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_left(node);
            } else if (node->balance() != 0) {
                node = node->par;
                break;
            }
            if (!new_node) {
                node = node->par;
                continue;
            }
            if (!new_node->par) {
                this->root = new_node;
                return;
            }
            node = new_node->par;
            if (new_node->key < node->key) {
                node->left = new_node;
            } else {
                node->right = new_node;
            }
            if (new_node->balance() != 0) break;
        }
        while (node) {
            node->update();
            node = node->par;
        }
    }

    constexpr void _add_balance(AVLTreeSetNodePtr node) {
        AVLTreeSetNodePtr new_node = nullptr;
        while (node) {
            node->update();
            if (node->balance() == 0) {
                node = node->par;
                break;
            }
            if (node->balance() == 2) {
                new_node = node->left->balance() == -1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_LR(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_right(node);
                break;
            } else if (node->balance() == -2) {
                new_node = node->right->balance() == 1 ? BBSTNode<AVLTreeSetNodePtr>::rotate_RL(node) : BBSTNode<AVLTreeSetNodePtr>::rotate_left(node);
                break;
            }
            node = node->par;
        }
        if (new_node) {
            node = new_node->par;
            if (node) {
                if (new_node->key < node->key) {
                    node->left = new_node;
                } else {
                    node->right = new_node;
                }
            } else {
                this->root = new_node;
            }
        }
        while (node) {
            node->update();
            node = node->par;
        }
    }

    public:
    AVLTreeSet() : missing(-1), root(nullptr) {}
    AVLTreeSet(T missing) : missing(-1), root(nullptr) {}
    AVLTreeSet(vector<T> &a, T missing) : missing(-1) {
        this->root = build(a);
    }

    AVLTreeSetNodePtr build(vector<T> a) {
        auto _build = [&] (auto &&_build, int l, int r) -> AVLTreeSetNodePtr {
            int mid = (l + r) / 2;
            AVLTreeSetNodePtr node = new AVLTreeSetNode(a[mid]);
            if (l != mid) {
                node->left = _build(_build, l, mid);
                node->left->par = node;
            }
            if (mid+1 != r) {
                node->right = _build(_build, mid+1, r);
                node->right->par = node;
            }
            node->update();
            return node;
        };

        if (a.empty()) return nullptr;
        int n = a.size();
        bool is_sorted = true;
        for (int i = 0; i < n-1; ++i) {
            if (!(a[i] < a[i+1])) {
                is_sorted = false;
                break;
            }
        }
        if (!is_sorted) {
            sort(a.begin(), a.end());
            a.erase(unique(a.begin(), a.end()), a.end());
        }
        return _build(_build, 0, a.size());
    }

    bool add(const T &key) {
        if (!root) {
            root = new AVLTreeSetNode(key);
            return true;
        }
        AVLTreeSetNodePtr pnode = nullptr;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) return false;
            pnode = node;
            node = key < node->key ? node->left : node->right;
        }
        if (key < pnode->key) {
            pnode->left = new AVLTreeSetNode(key);
            pnode->left->par = pnode;
        } else {
            pnode->right = new AVLTreeSetNode(key);
            pnode->right->par = pnode;
        }
        _add_balance(pnode);
        return true;
    }

    void remove_iter(AVLTreeSetNodePtr node) {
        AVLTreeSetNodePtr pnode = node->par;
        if (node->left && node->right) {
            pnode = node;
            AVLTreeSetNodePtr mnode = node->left;
            while (mnode->right) {
                pnode = mnode;
                mnode = mnode->right;
            }
            node->key = mnode->key;
            node = mnode;
        }
        AVLTreeSetNodePtr cnode = (!node->left) ? node->right : node->left;
        if (cnode) {
            cnode->par = pnode;
        }
        if (pnode) {
            if (node->key <= pnode->key) {
                pnode->left = cnode;
            } else {
                pnode->right = cnode;
            }
            _remove_balance(pnode);
        } else {
            root = cnode;
        }
    }

    bool discard(const T &key) {
        AVLTreeSetNodePtr node = find_key(key);
        if (!node) return false;
        remove_iter(node);
        return true;
    }

    void remove(const T &key) {
        AVLTreeSetNodePtr node = find_key(key);
        assert(node);
        remove_iter(node);
    }

    T le(const T &key) const {
        T res = missing;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) {
                res = node->key;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                res = node->key;
                node = node->right;
            }
        }
        return res;
    }

    T lt(const T &key) const {
        T res = missing;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key <= node->key) {
                node = node->left;
            } else {
                res = node->key;
                node = node->right;
            }
        }
        return res;
    }

    T ge(const T &key) const {
        T res = missing;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) {
                res = node->key;
                break;
            }
            if (key < node->key) {
                res = node->key;
                node = node->left;
            } else {
                node = node->right;
            }
        }
        return res;
    }

    T gt(const T &key) const {
        T res = missing;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key < node->key) {
                res = node->key;
                node = node->left;
            } else {
                node = node->right;
            }
        }
        return res;
    }

    int index(const T &key) const {
        int k = 0;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) {
                k += node->left ? node->left->size : 0;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                k += node->left ? (node->left->size + 1) : 1;
                node = node->right;
            }
        }
        return k;
    }

    int index_right(const T &key) const {
        int k = 0;
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) {
                k += node->left ? (node->left->size + 1) : 1;
                break;
            }
            if (key < node->key) {
                node = node->left;
            } else {
                k += node->left ? (node->left->size + 1) : 1;
                node = node->right;
            }
        }
        return k;
    }

    int range_count(int l, int r) const {
        return index(r) - index(l);
    }

    AVLTreeSetNodePtr find_key(const T &key) const {
        AVLTreeSetNodePtr node = root;
        while (node) {
            if (key == node->key) return node;
            node = key < node->key ? node->left : node->right;
        }
        return nullptr;
    }

    AVLTreeSetNodePtr find_kth(int k) const {
        AVLTreeSetNodePtr node = root;
        while (true) {
            int t = node->left ? node->left->size : 0;
            if (t == k) return node;
            if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    T pop(int k=-1) {
        AVLTreeSetNodePtr node = find_kth(k);
        T key = node->key;
        remove_iter(node);
        return key;
    }

    vector<T> tovector() const {
        vector<T> a;
        a.reserve(len());
        stack<AVLTreeSetNodePtr> st;
        AVLTreeSetNodePtr node = root;
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

    bool contains(T key) const {
        return find_key(key) != nullptr;
    }

    T get(int k) const {
        AVLTreeSetNodePtr node = root;
        while (true) {
            int t = node->left ? node->left->size : 0;
            if (t == k) return node->key;
            if (t < k) {
                k -= t + 1;
                node = node->right;
            } else {
                node = node->left;
            }
        }
    }

    int len() const {
        return root ? root->size : 0;
    }

    void check() const {
        if (!root) {
            cout << "height=0" << endl;;
            cout << "check ok empty." << endl;;
            return;
        }
        cout << "height=" << root->height << endl;

        auto dfs = [&] (auto &&dfs, AVLTreeSetNodePtr node) -> void {
            int h = 0;
            int b = 0;
            int s = 1;
            if (node->left) {
                assert(node->left->par == node);
                assert(node->key > node->left->key);
                dfs(dfs, node->left);
                h = max(h, node->left->height);
                b += node->left->height;
                s += node->left->size;
            }
            if (node->right) {
                assert(node->right->par == node);
                assert(node->key < node->right->key);
                dfs(dfs, node->right);
                h = max(h, node->right->height);
                b -= node->right->height;
                s += node->right->size;
            }
            assert(node->height == h+1);
            assert(-1 <= b && b <= 1);
            assert(node->size == s);
        };
        dfs(dfs, root);
        cout << "check ok." << endl;
    }

    friend ostream& operator<<(ostream& os, const titan23::AVLTreeSet<T>& s) {
        vector<T> a = s.tovector();
        int n = a.size();
        os << "{";
        for (int i = 0; i < n - 1; ++i) {
            os << a[i] << ", ";
        }
        if (n > 0) os << a.back();
        os << "}";
        return os;
    }
};
}
