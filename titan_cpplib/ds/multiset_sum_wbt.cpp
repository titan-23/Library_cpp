/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/multiset_sum_wbt.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
using namespace std;

// MultisetSum
namespace titan23 {

template<typename T>
class MultisetSum {
private:
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;

    struct Node {
        T key, data;
        int left, right, size, val, valsize;

        Node() : key(0), data(0), left(0), right(0), size(0), val(0), valsize(0) {}
        Node(const T &key, int val) : key(key), data(key*val), left(0), right(0), size(1), val(val), valsize(val) {}
    };

    T missing, min_key, max_key;
    vector<Node> nodes;
    int root, free;

    int weight(int node) const {
        return node ? nodes[node].size + 1 : 1;
    }

    void update(int node) {
        Node &n = nodes[node];
        n.size = 1 + nodes[n.left].size + nodes[n.right].size;
        n.valsize = n.val + nodes[n.left].valsize + nodes[n.right].valsize;
        n.data = n.key*n.val + nodes[n.left].data + nodes[n.right].data;
    }

    int make_node(const T &key, int val) {
        if (free) {
            int node = free;
            free = nodes[node].left;
            nodes[node] = Node(key, val);
            return node;
        }
        if (nodes.empty()) nodes.emplace_back();
        int node = nodes.size();
        nodes.emplace_back(key, val);
        return node;
    }

    void release_node(int node) {
        nodes[node].left = free;
        free = node;
    }

    int rotate_left(int node) {
        int u = nodes[node].right;
        nodes[node].right = nodes[u].left;
        nodes[u].left = node;
        update(node);
        update(u);
        return u;
    }

    int rotate_right(int node) {
        int u = nodes[node].left;
        nodes[node].left = nodes[u].right;
        nodes[u].right = node;
        update(node);
        update(u);
        return u;
    }

    int balance(int node) {
        update(node);
        int left = nodes[node].left;
        int right = nodes[node].right;
        if ((long long)weight(left)*DELTA < weight(right)) {
            if ((long long)weight(nodes[right].right)*GAMMA <= weight(nodes[right].left)) {
                nodes[node].right = rotate_right(right);
            }
            return rotate_left(node);
        }
        if ((long long)weight(right)*DELTA < weight(left)) {
            if ((long long)weight(nodes[left].left)*GAMMA <= weight(nodes[left].right)) {
                nodes[node].left = rotate_left(left);
            }
            return rotate_right(node);
        }
        return node;
    }

    int build_nodes(const vector<T> &keys, const vector<int> &vals, int l, int r) {
        if (l == r) return 0;
        int mid = (l + r) / 2;
        int node = make_node(keys[mid], vals[mid]);
        int left = build_nodes(keys, vals, l, mid);
        int right = build_nodes(keys, vals, mid+1, r);
        nodes[node].left = left;
        nodes[node].right = right;
        update(node);
        return node;
    }

    void build(const vector<T> &a) {
        root = 0;
        free = 0;
        nodes.clear();
        if (a.empty()) return;

        vector<T> sorted;
        const vector<T> *source = &a;
        if (!is_sorted(a.begin(), a.end())) {
            sorted = a;
            sort(sorted.begin(), sorted.end());
            source = &sorted;
        }

        vector<T> keys;
        vector<int> vals;
        keys.reserve(source->size());
        vals.reserve(source->size());
        for (const T &key : *source) {
            if (!keys.empty() && keys.back() == key) {
                ++vals.back();
            } else {
                keys.emplace_back(key);
                vals.emplace_back(1);
            }
        }

        nodes.reserve(keys.size() + 1);
        nodes.emplace_back();
        root = build_nodes(keys, vals, 0, keys.size());
        min_key = keys.front();
        max_key = keys.back();
    }

    int add_node(int node, const T &key, int val, bool &inserted) {
        if (!node) {
            inserted = true;
            return make_node(key, val);
        }
        if (key == nodes[node].key) {
            nodes[node].val += val;
            update(node);
            return node;
        }
        if (key < nodes[node].key) {
            int child = add_node(nodes[node].left, key, val, inserted);
            nodes[node].left = child;
        } else {
            int child = add_node(nodes[node].right, key, val, inserted);
            nodes[node].right = child;
        }
        if (inserted) return balance(node);
        update(node);
        return node;
    }

    int pop_max_node(int node, T &key, int &val) {
        if (!nodes[node].right) {
            key = nodes[node].key;
            val = nodes[node].val;
            int child = nodes[node].left;
            release_node(node);
            return child;
        }
        nodes[node].right = pop_max_node(nodes[node].right, key, val);
        return balance(node);
    }

    int remove_node(int node, const T &key, int val, bool &found, bool &erased) {
        if (!node) return 0;
        if (key < nodes[node].key) {
            nodes[node].left = remove_node(nodes[node].left, key, val, found, erased);
            if (!found) return node;
            if (erased) return balance(node);
            update(node);
            return node;
        }
        if (nodes[node].key < key) {
            nodes[node].right = remove_node(nodes[node].right, key, val, found, erased);
            if (!found) return node;
            if (erased) return balance(node);
            update(node);
            return node;
        }

        found = true;
        if (nodes[node].val > val) {
            nodes[node].val -= val;
            update(node);
            return node;
        }

        erased = true;
        int left = nodes[node].left;
        int right = nodes[node].right;
        if (!left || !right) {
            int child = left ? left : right;
            release_node(node);
            return child;
        }

        T new_key;
        int new_val;
        nodes[node].left = pop_max_node(left, new_key, new_val);
        nodes[node].key = new_key;
        nodes[node].val = new_val;
        return balance(node);
    }

    T leftmost_key() const {
        int node = root;
        while (nodes[node].left) node = nodes[node].left;
        return nodes[node].key;
    }

    T rightmost_key() const {
        int node = root;
        while (nodes[node].right) node = nodes[node].right;
        return nodes[node].key;
    }

    int find_key(const T &key) const {
        int node = root;
        while (node) {
            if (key == nodes[node].key) return node;
            node = key < nodes[node].key ? nodes[node].left : nodes[node].right;
        }
        return 0;
    }

    int find_kth(int k) const {
        assert(0 <= k && k < len());
        int node = root;
        while (node) {
            int left_size = nodes[nodes[node].left].valsize;
            if (k < left_size) {
                node = nodes[node].left;
            } else if (k < left_size + nodes[node].val) {
                return node;
            } else {
                k -= left_size + nodes[node].val;
                node = nodes[node].right;
            }
        }
        return 0;
    }

public:
    MultisetSum() : missing(), min_key(), max_key(), root(0), free(0) {}

    MultisetSum(T missing) : missing(missing), min_key(), max_key(), root(0), free(0) {}

    MultisetSum(const vector<T> &a, T missing=-1) : missing(missing), min_key(), max_key(), root(0), free(0) {
        build(a);
    }

    void add(const T &key, int val=1) {
        bool was_empty = !root;
        bool inserted = false;
        root = add_node(root, key, val, inserted);
        if (was_empty) {
            min_key = key;
            max_key = key;
        } else if (inserted) {
            if (key < min_key) min_key = key;
            if (max_key < key) max_key = key;
        }
    }

    bool discard(const T &key, int val=1) {
        bool update_min = root && key == min_key;
        bool update_max = root && key == max_key;
        bool found = false;
        bool erased = false;
        root = remove_node(root, key, val, found, erased);
        if (!found) return false;
        if (!root) return true;
        if (erased && update_min) min_key = leftmost_key();
        if (erased && update_max) max_key = rightmost_key();
        return true;
    }

    void remove(const T &key, int val=1) {
        bool update_min = root && key == min_key;
        bool update_max = root && key == max_key;
        bool found = false;
        bool erased = false;
        root = remove_node(root, key, val, found, erased);
        assert(found);
        if (!root) return;
        if (erased && update_min) min_key = leftmost_key();
        if (erased && update_max) max_key = rightmost_key();
    }

    T le(const T &key) const {
        T result = missing;
        int node = root;
        while (node) {
            if (key == nodes[node].key) return nodes[node].key;
            if (key < nodes[node].key) {
                node = nodes[node].left;
            } else {
                result = nodes[node].key;
                node = nodes[node].right;
            }
        }
        return result;
    }

    T lt(const T &key) const {
        T result = missing;
        int node = root;
        while (node) {
            if (key <= nodes[node].key) {
                node = nodes[node].left;
            } else {
                result = nodes[node].key;
                node = nodes[node].right;
            }
        }
        return result;
    }

    T ge(const T &key) const {
        T result = missing;
        int node = root;
        while (node) {
            if (key == nodes[node].key) return nodes[node].key;
            if (key < nodes[node].key) {
                result = nodes[node].key;
                node = nodes[node].left;
            } else {
                node = nodes[node].right;
            }
        }
        return result;
    }

    T gt(const T &key) const {
        T result = missing;
        int node = root;
        while (node) {
            if (key < nodes[node].key) {
                result = nodes[node].key;
                node = nodes[node].left;
            } else {
                node = nodes[node].right;
            }
        }
        return result;
    }

    int index(const T &key) const {
        int result = 0;
        int node = root;
        while (node) {
            if (key <= nodes[node].key) {
                node = nodes[node].left;
            } else {
                result += nodes[nodes[node].left].valsize + nodes[node].val;
                node = nodes[node].right;
            }
        }
        return result;
    }

    int index_right(const T &key) const {
        int result = 0;
        int node = root;
        while (node) {
            if (key < nodes[node].key) {
                node = nodes[node].left;
            } else {
                result += nodes[nodes[node].left].valsize + nodes[node].val;
                node = nodes[node].right;
            }
        }
        return result;
    }

    int count(const T &key) const {
        int node = find_key(key);
        return node ? nodes[node].val : 0;
    }

    bool contains(const T &key) const {
        return find_key(key);
    }

    // high未満
    T sum(T high) const {
        if (!root || high <= min_key) return 0;
        if (max_key < high) return nodes[root].data;
        T result = 0;
        int node = root;
        while (node) {
            if (high <= nodes[node].key) {
                node = nodes[node].left;
            } else {
                result += nodes[nodes[node].left].data + nodes[node].key*nodes[node].val;
                node = nodes[node].right;
            }
        }
        return result;
    }

    T all_prod() const {
        return root ? nodes[root].data : 0;
    }

    // 総和がwを超えないように先頭からとるとき、いくつとれるか？
    long long bisect_left_sum(T w) const {
        long long result = 0;
        T remain = w;
        int node = root;
        while (node) {
            int left = nodes[node].left;
            if (left) {
                if (nodes[left].data <= remain) {
                    result += nodes[left].valsize;
                    remain -= nodes[left].data;
                } else {
                    node = left;
                    continue;
                }
            }
            T node_sum = nodes[node].key*nodes[node].val;
            if (node_sum <= remain) {
                result += nodes[node].val;
                remain -= node_sum;
                node = nodes[node].right;
            } else {
                if (nodes[node].key == 0) {
                    result += nodes[node].val;
                    node = nodes[node].right;
                    continue;
                }
                long long count = (long long)(remain / nodes[node].key);
                if (count > nodes[node].val) count = nodes[node].val;
                if (count < 0) count = 0;
                return result + count;
            }
        }
        return result;
    }

    T get(int k) const {
        return nodes[find_kth(k)].key;
    }

    T operator[](int k) const {
        return get(k);
    }

    T pop(int k=-1) {
        if (k < 0) k += len();
        T key = get(k);
        remove(key);
        return key;
    }

    int len() const {
        return root ? nodes[root].valsize : 0;
    }

    int size() const {
        return len();
    }

    vector<T> tovector() const {
        vector<T> result;
        result.reserve(len());
        vector<int> stack;
        int node = root;
        while (node || !stack.empty()) {
            if (node) {
                stack.emplace_back(node);
                node = nodes[node].left;
            } else {
                node = stack.back();
                stack.pop_back();
                for (int i = 0; i < nodes[node].val; ++i) result.emplace_back(nodes[node].key);
                node = nodes[node].right;
            }
        }
        return result;
    }

    void print() const {
        vector<T> a = tovector();
        cout << "{";
        for (int i = 0; i < a.size(); ++i) {
            if (i) cout << ", ";
            cout << a[i];
        }
        cout << "}" << endl;
    }

    void check() const {
        if (!root) return;
        auto dfs = [&] (auto &&dfs, int node) -> void {
            int left = nodes[node].left;
            int right = nodes[node].right;
            if (left) {
                assert(nodes[left].key < nodes[node].key);
                dfs(dfs, left);
            }
            if (right) {
                assert(nodes[node].key < nodes[right].key);
                dfs(dfs, right);
            }
            assert(nodes[node].size == 1 + nodes[left].size + nodes[right].size);
            assert(nodes[node].valsize == nodes[node].val + nodes[left].valsize + nodes[right].valsize);
            assert(nodes[node].data == nodes[node].key*nodes[node].val + nodes[left].data + nodes[right].data);
            assert((long long)weight(left)*DELTA >= weight(right));
            assert((long long)weight(right)*DELTA >= weight(left));
        };
        dfs(dfs, root);
        assert(min_key == leftmost_key());
        assert(max_key == rightmost_key());
    }

    friend ostream& operator<<(ostream& os, const titan23::MultisetSum<T> &multiset) {
        vector<T> a = multiset.tovector();
        os << "{";
        for (int i = 0; i < a.size(); ++i) {
            if (i) os << ", ";
            os << a[i];
        }
        os << "}";
        return os;
    }
};
} // namespace titan23
