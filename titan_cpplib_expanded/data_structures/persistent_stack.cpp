// #include "titan_cpplib/data_structures/persistent_stack.cpp"
#include <vector>
#include <cassert>
using namespace std;

// PersistentStack
namespace titan23 {

  template<typename T>
  class PersistentStack {
  private:
    struct Node;
    using NodePtr = Node*;
    NodePtr node;

    struct Node {
      T key;
      Node* prev;
      Node() {}
      Node(T key) : key(key), prev(nullptr) {}
      Node(T key, NodePtr prev) : key(key), prev(prev) {}
    };

    PersistentStack(NodePtr new_node) : node(new_node) {}

  public:
    PersistentStack() : node(nullptr) {}
    
    T top() const {
      assert(node);
      return node->key;
    }

    T pop() {
      T res = node->key;
      assert(node->prev);
      node = node->prev;
      return res;
    }

    PersistentStack copy() const {
      NodePtr new_node = node ? (new Node(node->key, node->prev)) : nullptr;
      PersistentStack s(new_node);
      return s;
    }

    PersistentStack push(T key) {
      NodePtr new_node = new Node(key);
      new_node->prev = this->node;
      PersistentStack s(new_node);
      return s;
    }

    vector<T> tovector() const {
      vector<T> a;
      NodePtr s = node;
      while (s) {
        a.emplace_back(s->key);
        s = s->prev;;
      }
      reverse(a.begin(), a.end());
      return a;
    }
  };
}  // namespase titan23

