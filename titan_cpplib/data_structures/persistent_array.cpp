#include <vector>
using namespace std;

// PersistentArray
namespace titan23 {

/**
 * @brief 永続配列
 */
template<typename T>
class PersistentArray {
    private:
    struct Node;
    using NodePtr = Node*;

    NodePtr root;
    int n;

    struct Node {
        T key;
        NodePtr left, right;

        Node() : left(nullptr), right(nullptr) {}
        Node(T key) : key(key), left(nullptr), right(nullptr) {}

        NodePtr copy() {
            NodePtr node = new Node(this->key);
            node->left = this->left;
            node->right = this->right;
            return node;
        }
    };

    int bit_length(const int n) const {
        return n == 0 ? 0 : 32 - __builtin_clz(n);
    }

    void _build(const vector<T> &a) {
        int n = a.size();
        this->n = n;
        if (n == 0) {
            this->root = nullptr;
            return;
        }
        vector<NodePtr> pool(n);
        for (int i = 0; i < n; ++i) {
            pool[i] = new Node(a[i]);
        }
        for (int i = 1; i < n+1; ++i) {
            if (2*i-1 < n) {
                pool[i-1]->left = pool[2*i-1];
            }
            if (2*i   < n) {
                pool[i-1]->right = pool[2*i];
            } else {
                break;
            }
        }
        this->root = pool[0];
    }

    PersistentArray<T> _new(NodePtr root) const {
        PersistentArray<T> res;
        res.root = root;
        res.n = this->n;
        return res;
    }

    public:
    PersistentArray() : root(nullptr), n(0) {}

    //! 配列 `a` をもとに構築する / `O(n)`
    PersistentArray(const vector<T> a) {
        _build(a);
    }

    //! 位置 `k` を `v` に変更した永続配列を返す / `O(logn)` time, `O(logn)` space
    PersistentArray<T> set(int k, T v) const{
        assert(0 <= k && k < this->n);
        assert(this->root);
        NodePtr node = this->root;
        NodePtr new_node = node->copy();
        PersistentArray<T> res = _new(new_node);
        k++;
        int b = bit_length(k);
        for (int i = b-2; i >= 0; --i) {
            if ((k >> i) & 1) {
                node = node->right;
                new_node->right = node->copy();
                new_node = new_node->right;
            } else {
                node = node->left;
                new_node->left = node->copy();
                new_node = new_node->left;
            }
        }
        new_node->key = v;
        return res;
    }

    //! 位置 `k` の値を返す / `O(logn)` time, `O(1)` space
    T get(int k) const {
        assert(0 <= k && k < this->n);
        assert(this->root);
        NodePtr node = this->root;
        k++;
        int b = bit_length(k);
        for (int i = b-2; i >= 0; --i) {
            if ((k >> i) & 1) {
                node = node->right;
            } else {
                node = node->left;
            }
        }
        return node->key;
    }

    //! 永続配列全体をコピーして返す / `O(1)` time, `O(1)` space
    PersistentArray<T> copy() const {
        return _new(this->root ? this->root->copy() : nullptr);
    }

    //! `vector` にして返す / `O(n)`
    vector<T> tovector() const {
        vector<T> a(this->n);
        vector<NodePtr> q = {this->root};
        for (int i = 0; i < (int)q.size(); ++i) {
            NodePtr node = q[i];
            a[i] = node->key;
            if (node->left) q.emplace_back(node->left);
            if (node->right) q.emplace_back(node->right);
        }
        return a;
    }

    //! 要素数を返す / `O(1)`
    int len() const {
        return this->n;
    }

    void print() const {
        vector<T> a = tovector();
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
