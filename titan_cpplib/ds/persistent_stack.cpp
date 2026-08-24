/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_stack.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
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

    pair<PersistentStack<T>, T> pop() const {
        assert(node);
        T res = node->key;
        PersistentStack<T> s(node->prev);
        return {s, res};
    }

    PersistentStack<T> copy() const {
        NodePtr new_node = node ? (new Node(node->key, node->prev)) : nullptr;
        PersistentStack<T> s(new_node);
        return s;
    }

    PersistentStack<T> push(const T &key) const {
        NodePtr new_node = new Node(key);
        new_node->prev = this->node;
        PersistentStack<T> s(new_node);
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

    friend ostream& operator<<(ostream& os, const PersistentStack<T> &s) {
        return os << s.tovector();
    }
};
}  // namespase titan23
