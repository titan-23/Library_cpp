#include <iostream>
#include <vector>
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
  class PersistentLazyWBTree {

   private:
    class Node;
    using NodePtr = shared_ptr<Node>;
    using MyPersistentLazyWBTree = PersistentLazyWBTree<T, F, op, mapping, composition, e, id>;
    static constexpr double ALPHA = 1 - sqrt(2) / 2;
    static constexpr double BETA = (1 - 2 * ALPHA) / (1 - ALPHA);

    class Node {

     public:
      T key, data;
      NodePtr left;
      NodePtr right;
      F lazy;
      int rev, size;

      Node(T key, F lazy) : key(key), data(key), left(nullptr), right(nullptr), lazy(lazy), rev(0), size(1) {}

      NodePtr copy() const {
        NodePtr node = make_shared<Node>(key, lazy);
        node->data = data;
        node->left = left;
        node->right = right;
        node->rev = rev;
        node->size = size;
        return node;
      }

      double balance() const {
        return ((left ? left->size : 0) + 1.0) / (size + 1.0);
      }

      void propagate() {
        if (rev) {
          NodePtr l = (left)? left->copy(): nullptr;
          NodePtr r = (right)? right->copy(): nullptr;
          left = r;
          right = l;
          if (left) left->rev ^= 1;
          if (right) right->rev ^= 1;
          rev = 0;
        }
        if (lazy != id()) {
          if (left) {
            left = left->copy();
            left->key = mapping(lazy, left->key);
            left->data = mapping(lazy, left->data);
            left->lazy = composition(lazy, left->lazy);
          }
          if (right) {
            right = right->copy();
            right->key = mapping(lazy, right->key);
            right->data = mapping(lazy, right->data);
            right->lazy = composition(lazy, right->lazy);
          }
          lazy = id();
        }
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
        NodePtr node = make_shared<Node>(a[mid], id());
        if (l != mid) node->left = build(build, l, mid);
        if (mid+1 != r) node->right = build(build, mid+1, r);
        node->update();
        return node;
      };
      root = build(build, 0, (int)a.size());
    }

    NodePtr _rotate_right(NodePtr &node) {
      NodePtr u = node->left->copy();
      node->left = u->right;
      u->right = node;
      node->update();
      u->update();
      return u;
    }

    NodePtr _rotate_left(NodePtr &node) {
      NodePtr u = node->right->copy();
      node->right = u->left;
      u->left = node;
      node->update();
      u->update();
      return u;
    }

    NodePtr _balance_left(NodePtr &node) {
      node->right->propagate();
      node->right = node->right->copy();
      NodePtr u = node->right;
      if (u->balance() >= BETA) {
        u->left->propagate();
        node->right = _rotate_right(u);
      }
      u = _rotate_left(node);
      return u;
    }

    NodePtr _balance_right(NodePtr &node) {
      node->left->propagate();
      node->left = node->left->copy();
      NodePtr u = node->left;
      if (u->balance() <= 1.0 - BETA) {
        u->right->propagate();
        node->left = _rotate_left(u);
      }
      u = _rotate_right(node);
      return u;
    }

    NodePtr _merge_with_root(NodePtr l, NodePtr root, NodePtr r) {
      int ls = l? l->size: 0;
      int rs = r? r->size: 0;
      double diff = (double)(ls+1.0) / (ls+rs+2.0);
      if (diff > 1.0-ALPHA) {
        l->propagate();
        l = l->copy();
        l->right = _merge_with_root(l->right, root, r);
        l->update();
        if (!(ALPHA <= l->balance() && l->balance() <= 1.0-ALPHA)) {
          return _balance_left(l);
        }
        return l;
      }
      if (diff < ALPHA) {
        r->propagate();
        r = r->copy();
        r->left = _merge_with_root(l, root, r->left);
        r->update();
        if (!(ALPHA <= r->balance() && r->balance() <= 1.0-ALPHA)) {
          return _balance_right(r);
        }
        return r;
      }
      root = root->copy();
      root->left = l;
      root->right = r;
      root->update();
      return root;
    }

    pair<NodePtr, NodePtr> _pop_right(NodePtr &node) {
      vector<NodePtr> path;
      node->propagate();
      node = node->copy();
      NodePtr mx = node;
      while (node->right) {
        path.emplace_back(node);
        node->right->propagate();
        node = node->right->copy();
        mx = node;
      }
      path.emplace_back(node->left? node->left->copy(): nullptr);
      while ((int)path.size() > 1) {
        node = path.back();
        path.pop_back();
        if (!node) {
          path.back()->right = nullptr;
          path.back()->update();
          continue;
        }
        double b = node->balance();
        if (ALPHA <= b && b <= 1.0-ALPHA) {
          path.back()->right = node;
        } else if (b > 1.0-ALPHA) {
          path.back()->right = _balance_right(node);
        } else {
          path.back()->right = _balance_left(node);
        }
        path.back()->update();
      }
      if (path[0]) {
        double b = path[0]->balance();
        if (b > 1.0-ALPHA) {
          path[0] = _balance_right(path[0]);
        } else if (b < ALPHA) {
          path[0] = _balance_left(path[0]);
        }
      }
      mx->left = nullptr;
      mx->update();
      return {path[0], mx};
    }

    NodePtr _merge_node(NodePtr l, NodePtr r) {
      if ((!l) && (!r)) {return nullptr;}
      if (!l) {return r->copy();}
      if (!r) {return l->copy();}
      l = l->copy();
      r = r->copy();
      auto [l_, root_] = _pop_right(l);
      return _merge_with_root(l_, root_, r);
    }

    pair<NodePtr, NodePtr> _split_node(NodePtr &node, int k) {
      if (!node) {return {nullptr, nullptr};}
      node->propagate();
      int tmp = node->left? k-node->left->size: k;
      if (tmp == 0) {
        return {node->left, _merge_with_root(nullptr, node, node->right)};
      } else if (tmp < 0) {
        auto [l, r] = _split_node(node->left, k);
        return {l, _merge_with_root(r, node, node->right)};
      } else {
        auto [l, r] = _split_node(node->right, tmp-1);
        return {_merge_with_root(node->left, node, l), r};
      }
    }

    MyPersistentLazyWBTree _new(NodePtr root) {
      MyPersistentLazyWBTree p(root);
      return p;
    }

    PersistentLazyWBTree(NodePtr root) : root(root) {}

   public:
    NodePtr root;

    PersistentLazyWBTree() : root(nullptr) {}

    PersistentLazyWBTree(vector<T> &a) { _build(a); }

    MyPersistentLazyWBTree merge(MyPersistentLazyWBTree other) {
      NodePtr root = _merge_node(this->root, other.root);
      return _new(root);
    }

    pair<MyPersistentLazyWBTree, MyPersistentLazyWBTree> split(int k) {
      auto [l, r] = _split_node(this->root, k);
      return {_new(l), _new(r)};
    }

    MyPersistentLazyWBTree apply(int l, int r, F f) {
      if (l >= r) return _new(this->root ? root->copy() : nullptr);
      auto dfs = [&] (auto &&dfs, NodePtr node, int left, int right) -> NodePtr {
        if (right <= l || r <= left) return node;
        node->propagate();
        NodePtr nnode = node->copy();
        if (l <= left && right < r) {
          nnode->key = mapping(f, nnode->key);
          nnode->data = mapping(f, nnode->data);
          nnode->lazy = composition(f, nnode->lazy);
          return nnode;
        }
        int lsize = nnode->left ? nnode->left->size : 0;
        if (nnode->left) nnode->left = dfs(dfs, nnode->left, left, left+lsize);
        if (l <= left+lsize && left+lsize < r) nnode->key = mapping(f, nnode->key);;
        if (nnode->right) nnode->right = dfs(dfs, nnode->right, left+lsize+1, right);
        nnode->update();
        return nnode;
      };
      return _new(dfs(dfs, root, 0, len()));
    }

    T prod(int l, int r) {
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

    MyPersistentLazyWBTree insert(int k, T key) {
      assert(0 <= k && k <= len());
      auto [s, t] = _split_node(root, k);
      NodePtr new_node = make_shared<Node>(key, id());
      return _new(_merge_with_root(s, new_node, t));
    }

    pair<MyPersistentLazyWBTree, T> pop(int k) {
      assert(0 <= k && k < len());
      auto [s_, t] = _split_node(this->root, k+1);
      auto [s, tmp] = _pop_right(s_);
      NodePtr root = _merge_node(s, t);
      return {_new(root), tmp->key};
    }

    MyPersistentLazyWBTree reverse(int l, int r) {
      assert(0 <= l && l <= r && r <= len());
      if (l >= r) return _new(root? root->copy(): nullptr);
      auto [s_, t] = _split_node(root, r);
      auto [u, s] = _split_node(s_, l);
      s->rev ^= 1;
      NodePtr root = _merge_node(_merge_node(u, s), t);
      return _new(root);
    }

    vector<T> tovector() {
      NodePtr node = root;
      vector<NodePtr> stack;
      vector<T> a;
      a.reserve(len());
      while ((!stack.empty()) || node) {
        if (node) {
          node->propagate();
          stack.emplace_back(node);
          node = node->left;
        } else {
          node = stack.back();
          stack.pop_back();
          a.emplace_back(node->key);
          node = node->right;
        }
      }
      return a;
    }

    MyPersistentLazyWBTree copy() const {
      return _new(root? root->copy(): nullptr);
    }

    MyPersistentLazyWBTree set(int k, T v) {
      if (k < 0) k += len();
      assert(0 <= k && k < len());
      NodePtr node = root->copy();
      NodePtr root = node;
      NodePtr pnode = nullptr;
      int d = 0;
      vector<NodePtr> path = {node};
      while (1) {
        node->propagate();
        int t = node->left? node->left->size: 0;
        if (t == k) {
          node = node->copy();
          node->key = v;
          path.emplace_back(node);
          if (d) pnode->left = node;
          else pnode->right = node;
          while (!path.empty()) {
            update(path.back());
            path.pop_back();
          }
          return _new(root);
        }
        pnode = node;
        if (t < k) {
          k -= t + 1;
          node = node->right->copy();
          d = 0;
        } else {
          d = 1;
          node = node->left->copy();
        }
        path.emplace_back(node);
        if (d) pnode->left = node;
        else pnode->right = node;
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

    void debug() {
      auto dfs = [&] (auto &dfs, NodePtr node) -> int {
        int h = 0;
        int s = 1;
        node->propagate();
        if (node->left) {
          h = max(h, dfs(dfs, node->left));
          s += node->left->size;
        }
        if (node->right) {
          h = max(h, dfs(dfs, node->right));
          s += node->right->size;
        }
        assert(node->size == s);
        return h + 1;
      };
      if (!this->root) {
        cerr << "debug: nullptr" << endl;
        return;
      }
      int h = dfs(dfs, this->root);
      cerr << "height=" << h << endl;
    }
  };
}  // namespace titan23

