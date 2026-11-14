/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_stack.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 永続スタック / ノードは解放しない
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
    /// @brief 空のスタックを構築する / O(1)
    PersistentStack() : node(nullptr) {}

    /// @brief 最後に積んだ値を返す / O(1)
    T top() const {
        assert(node);
        return node->key;
    }

    /// @brief 最後に積んだ値を取り除いたスタックとその値を返す / O(1)
    pair<PersistentStack<T>, T> pop() const {
        assert(node);
        T res = node->key;
        PersistentStack<T> s(node->prev);
        return {s, res};
    }

    /// @brief コピーを返す / O(1)
    PersistentStack<T> copy() const {
        NodePtr new_node = node ? (new Node(node->key, node->prev)) : nullptr;
        PersistentStack<T> s(new_node);
        return s;
    }

    /// @brief key を積んだスタックを返す / O(1)
    PersistentStack<T> push(const T &key) const {
        NodePtr new_node = new Node(key);
        new_node->prev = this->node;
        PersistentStack<T> s(new_node);
        return s;
    }

    /// @brief 積んだ順に並べた配列を返す / O(n)
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

    /// @brief 全要素をストリームへ出力する / O(n)
    friend ostream& operator<<(ostream& os, const PersistentStack<T> &s) {
        return os << s.tovector();
    }
};
}  // namespase titan23
