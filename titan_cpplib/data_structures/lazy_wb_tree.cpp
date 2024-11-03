#include <iostream>
#include <vector>
#include <stack>
#include <cmath>
#include <cassert>
#include <memory>
using namespace std;

namespace titan23 {

  template <class T,
            class F,
            T (*op)(T, T),
            T (*mapping)(F, T),
            F (*composition)(F, F),
            T (*e)(),
            F (*id)()>
  class LazyWBTree {

   private:
    class Node;
    using NodePtr = Node*;
    using MyLazyWBTree = LazyWBTree<T, F, op, mapping, composition, e, id>;
    static constexpr double ALPHA = 1 - sqrt(2) / 2;
    static constexpr double BETA = (1 - 2 * ALPHA) / (1 - ALPHA);

    class Node {

     public:
      T key, data;
      NodePtr left;
      NodePtr right;
      F lazy;
      bool rev;
      int size;

      Node(T key, F lazy) : key(key), data(key), left(nullptr), right(nullptr), lazy(lazy), rev(false), size(1) {}

      double balance() const {
        return ((left ? left->size : 0) + 1.0) / (size + 1.0);
      }

      void update() {
        size = 1;
        data = key;
        if (left) {
          size += left->size;
          data = op(left->data, key);
        }
        if (right) {
          size += right->size;
          data = op(data, right->data);
        }
      }

      void propagate() {
        if (rev) {
          swap(left, right);
          if (left) left->rev ^= 1;
          if (right) right->rev ^= 1;
          rev = 0;
        }
        if (lazy != id()) {
          if (left) {
            left->key = mapping(lazy, left->key);
            left->data = mapping(lazy, left->data);
            left->lazy = composition(lazy, left->lazy);
          }
          if (right) {
            right->key = mapping(lazy, right->key);
            right->data = mapping(lazy, right->data);
            right->lazy = composition(lazy, right->lazy);
          }
          lazy = id();
        }
      }

      void print() const {
        cout << "this : key=" << key << ", size=" << size << endl;
        if (left)  cout << "to-left" << endl;
        if (right) cout << "to-right" << endl;
        cout << endl;
        if (left)  left->print();
        if (right) right->print();
      }
    };

    void _build(vector<T> const &a) {
      auto build = [&] (auto &&build, int l, int r) -> NodePtr {
        int mid = (l + r) >> 1;
        NodePtr node = new Node(a[mid], id());
        if (l != mid) node->left = build(build, l, mid);
        if (mid+1 != r) node->right = build(build, mid+1, r);
        node->update();
        return node;
      };
      root = build(build, 0, (int)a.size());
    }

    NodePtr _rotate_right(NodePtr node) {
      NodePtr u = node->left;
      node->left = u->right;
      u->right = node;
      node->update();
      u->update();
      return u;
    }

    NodePtr _rotate_left(NodePtr node) {
      NodePtr u = node->right;
      node->right = u->left;
      u->left = node;
      node->update();
      u->update();
      return u;
    }

    NodePtr _balance_left(NodePtr node) {
      NodePtr u = node->right;
      u->propagate();
      if (u->balance() >= BETA) {
        u->left->propagate();
        node->right = _rotate_right(u);
      }
      return _rotate_left(node);
    }

    NodePtr _balance_right(NodePtr node) {
      NodePtr u = node->left;
      u->propagate();
      if (u->balance() <= 1.0 - BETA) {
        u->right->propagate();
        node->left = _rotate_left(u);
      }
      return _rotate_right(node);
    }

    NodePtr _merge_with_root(NodePtr l, NodePtr root, NodePtr r) {
      int ls = l ? l->size : 0;
      int rs = r ? r->size : 0;
      double diff = (double)(ls+1.0) / (ls+rs+2.0);
      if (diff > 1.0-ALPHA) {
        l->propagate();
        l->right = _merge_with_root(l->right, root, r);
        l->update();
        if (!(ALPHA <= l->balance() && l->balance() <= 1.0-ALPHA)) {
          return _balance_left(l);
        }
        return l;
      }
      if (diff < ALPHA) {
        r->propagate();
        r->left = _merge_with_root(l, root, r->left);
        r->update();
        if (!(ALPHA <= r->balance() && r->balance() <= 1.0-ALPHA)) {
          return _balance_right(r);
        }
        return r;
      }
      root->left = l;
      root->right = r;
      root->update();
      return root;
    }

    pair<NodePtr, NodePtr> _pop_right(NodePtr node) {
      stack<NodePtr> path;
      node->propagate();
      NodePtr mx = node;
      while (node->right) {
        path.emplace(node);
        node = node->right;
        node->propagate();
        mx = node;
      }
      path.emplace(node->left ? node->left : nullptr);
      while ((int)path.size() > 1) {
        node = path.top();
        path.pop();
        if (!node) {
          path.top()->right = nullptr;
          path.top()->update();
          continue;
        }
        double b = node->balance();
        if (ALPHA <= b && b <= 1.0-ALPHA) {
          path.top()->right = node;
        } else if (b > 1.0-ALPHA) {
          path.top()->right = _balance_right(node);
        } else {
          path.top()->right = _balance_left(node);
        }
        path.top()->update();
      }
      if (path.top()) {
        double b = path.top()->balance();
        if (b > 1.0-ALPHA) {
          path.top() = _balance_right(path.top());
        } else if (b < ALPHA) {
          path.top() = _balance_left(path.top());
        }
      }
      mx->left = nullptr;
      mx->update();
      return {path.top(), mx};
    }

    NodePtr _merge_node(NodePtr l, NodePtr r) {
      if ((!l) && (!r)) {return nullptr;}
      if (!l) {return r;}
      if (!r) {return l;}
      auto [l_, root_] = _pop_right(l);
      return _merge_with_root(l_, root_, r);
    }

    pair<NodePtr, NodePtr> _split_node(NodePtr node, int k) {
      if (!node) return {nullptr, nullptr};
      node->propagate();
      int tmp = node->left ? k-node->left->size : k;
      NodePtr nl = node->left;
      NodePtr nr = node->right;
      if (tmp == 0) {
        return {nl, _merge_with_root(nullptr, node, nr)};
      } else if (tmp < 0) {
        auto [l, r] = _split_node(nl, k);
        return {l, _merge_with_root(r, node, nr)};
      } else {
        auto [l, r] = _split_node(nr, tmp-1);
        return {_merge_with_root(node->left, node, l), r};
      }
    }

    LazyWBTree _new(NodePtr root) {
      LazyWBTree p(root);
      return p;
    }

    LazyWBTree(NodePtr &root): root(root) {}

   public:
    NodePtr root;

    LazyWBTree() : root(nullptr) {}

    LazyWBTree(vector<T> const &a) { _build(a); }

    void merge(MyLazyWBTree &other) {
      this->root = _merge_node(this->root, other.root);
    }

    pair<MyLazyWBTree, MyLazyWBTree> split(const int k) {
      auto [l, r] = _split_node(this->root, k);
      return {_new(l), _new(r)};
    }

    void all_apply(const F f) {
      if (!this->root) return;
      this->root->key = mapping(f, this->root->key);
      this->root->data = mapping(f, this->root->data);
      this->root->lazy = composition(f, this->root->lazy);
    }

    void apply(const int l, const int r, const F f) {
      if (l >= r) return;
      auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> void {
        if (right <= l || r <= left) return;
        node->propagate();
        if (l <= left && right < r) {
          node->key = mapping(f, node->key);
          node->data = mapping(f, node->data);
          node->lazy = composition(f, node->lazy);
          return;
        }
        int lsize = node->left ? node->left->size : 0;
        if (node->left) dfs(dfs, node->left, left, left+lsize);
        if (l <= left+lsize && left+lsize < r) node->key = mapping(f, node->key);;
        if (node->right) dfs(dfs, node->right, left+lsize+1, right);
        node->update();
      };
      dfs(dfs, root, 0, len());
    }

    T all_prod() const {
      return this->root ? this->root->data : e();
    }

    T prod(const int l, const int r) {
      if (l >= r) return e();
      auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> T {
        if (right <= l || r <= left) return e();
        node->propagate();
        if (l <= left && right < r) return node->data;
        int lsize = node->left ? node->left->size : 0;
        T res = e();
        if (node->left) res = dfs(dfs, node->left, left, left+lsize);
        if (l <= left+lsize && left+lsize < r) res = op(res, node->key);
        if (node->right) res = op(res, dfs(dfs, node->right, left+lsize+1, right));
        return res;
      };
      return dfs(dfs, root, 0, len());
    }

    void insert(int k, const T key) {
      auto [s, t] = _split_node(root, k);
      NodePtr new_node = new Node(key, id());
      this->root = _merge_with_root(s, new_node, t);
    }

    T pop(int k) {
      if (k < 0) k += len();
      assert(0 <= k && k < len());
      stack<NodePtr> path;
      stack<short> path_d;
      NodePtr node = this->root;
      T res;
      int d = 0;
      while (1) {
        node->propagate();
        int t = node->left? node->left->size: 0;
        if (t == k) {
          res = node->key;
          break;
        }
        int d = (t < k)? 0: 1;
        path.emplace(node);
        path_d.emplace(d);
        if (d) {
          node = node->left;
        } else {
          k -= t + 1;
          node = node->right;
        }
      }
      if (node->left && node->right) {
        node->propagate();
        NodePtr lmax = node->left;
        path.emplace(node);
        path_d.emplace(1);
        lmax->propagate();
        while (lmax->right) {
          path.emplace(lmax);
          path_d.emplace(0);
          lmax = lmax->right;
          lmax->propagate();
        }
        node->key = lmax->key;
        node = lmax;
      }
      NodePtr cnode = (node->left)? node->left: node->right;
      if (!path.empty()) {
        if (path_d.top()) path.top()->left = cnode;
        else path.top()->right = cnode;
      } else {
        this->root = cnode;
        return res;
      }
      while (!path.empty()) {
        NodePtr new_node = nullptr;
        node = path.top();
        node->update();
        path.pop();
        path_d.pop();
        double b = node->balance();
        if (b < ALPHA) {
          new_node = _balance_left(node);
        } else if (b > 1.0-ALPHA) {
          new_node = _balance_right(node);
        }
        if (new_node) {
          if (path.empty()) {
            this->root = new_node;
            break;
          }
          if (path_d.top()) {
            path.top()->left = new_node;
          } else {
            path.top()->right = new_node;
          }
        }
      }
      return res;
    }

    void reverse(const int l, const int r) {
      if (l >= r) return;
      auto [s_, t] = _split_node(root, r);
      auto [u, s] = _split_node(s_, l);
      s->rev ^= 1;
      this->root = _merge_node(_merge_node(u, s), t);
    }

    vector<T> tovector() {
      NodePtr node = root;
      stack<NodePtr> s;
      vector<T> a;
      a.reserve(len());
      while ((!s.empty()) || node) {
        if (node) {
          node->propagate();
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

    void set(int k, T v) {
      if (k < 0) k += len();
      assert(0 <= k && k < len());
      NodePtr node = root;
      NodePtr root = node;
      NodePtr pnode = nullptr;
      int d = 0;
      vector<NodePtr> path = {node};
      while (1) {
        node->propagate();
        int t = node->left? node->left->size: 0;
        if (t == k) {
          node->key = v;
          path.emplace_back(node);
          if (pnode) {
            if (d) pnode->left = node;
            else pnode->right = node;
          }
          while (!path.empty()) {
            path.back()->update();
            path.pop_back();
          }
          return;
        }
        pnode = node;
        d = (t < k)? 0: 1;
        if (d) {
          pnode->left = node = node->left;
        } else {
          k -= t + 1;
          pnode->right = node = node->right;
        }
        path.emplace_back(node);
      }
    }

    T get(int k) {
      if (k < 0) k += len();
      assert(0 <= k && k < len());
      NodePtr node = root;
      while (1) {
        node->propagate();
        int t = node->left? node->left->size: 0;
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

    void print() {
      vector<T> a = tovector();
      cout << "[";
      for (int i = 0; i < (int)a.size()-1; ++i) {
        cout << a[i] << ", ";
      }
      if (!a.empty()) cout << a.back();
      cout << "]" << endl;
    }

    int len() const {
      return root? root->size: 0;
    }

    void isok() {
      auto rec = [&] (auto &&rec, NodePtr node) -> pair<int, int> {
        int ls = 0, rs = 0;
        int height = 0;
        int h;
        if (node->left) {
          pair<int, int> res = rec(rec, node->left);
          ls = res.first;
          h = res.second;
          height = max(height, h);
        }
        if (node->right) {
          pair<int, int> res = rec(rec, node->right);
          rs = res.first;
          h = res.second;
          height = max(height, h);
        }
        int s = ls + rs + 1;
        double b = (double)(ls+1) / (s+1);
        assert(s == node->size);
        assert(ALPHA <= b && b <= 1-ALPHA);
        return {s, height+1};
      };
      if (root == nullptr) return;
      auto [_, h] = rec(rec, root);
      // printf("height=%d\n", h);
    }
  };
} // namespace titan23
