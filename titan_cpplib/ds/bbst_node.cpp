/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/bbst_node.cpp
#pragma once

using namespace std;

namespace titan23 {

/// @brief 平衡二分探索木のノードを回転、走査する静的関数の集まり / ノードは left、right、par と update() を持つこと
template<typename NodePtr>
class BBSTNode {
    public:
    /// @brief node を右回転し、新しい部分木の根を返す / O(1)
    static NodePtr rotate_right(NodePtr node) {
        NodePtr u = node->left;
        u->par = node->par;
        node->left = u->right;
        if (u->right) u->right->par = node;
        u->right = node;
        node->par = u;
        node->update();
        u->update();
        return u;
    }

    /// @brief node を左回転し、新しい部分木の根を返す / O(1)
    static NodePtr rotate_left(NodePtr node) {
        NodePtr u = node->right;
        u->par = node->par;
        node->right = u->left;
        if (u->left) u->left->par = node;
        u->left = node;
        node->par = u;
        node->update();
        u->update();
        return u;
    }

    /// @brief 左の子を左回転してから node を右回転し、新しい部分木の根を返す / O(1)
    static NodePtr rotate_LR(NodePtr node) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    /// @brief 右の子を右回転してから node を左回転し、新しい部分木の根を返す / O(1)
    static NodePtr rotate_RL(NodePtr node) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    /// @brief node を根とする部分木の最も左のノードを返す / O(木の高さ)
    static NodePtr _min(NodePtr node) {
        while (node->left) node = node->left;
        return node;
    }

    /// @brief node を根とする部分木の最も右のノードを返す / O(木の高さ)
    static NodePtr _max(NodePtr node) {
        while (node->right) node = node->right;
        return node;
    }

    /// @brief 通りがけ順で node の次のノードを返す / 次のノードがあること / O(木の高さ)
    static NodePtr _next(NodePtr node) {
        NodePtr now = node;
        NodePtr pre = nullptr;
        bool flag = now->right == pre;
        while (now->right == pre) {
            pre = now;
            now = now->par;
        }
        if (!now) return nullptr;
        return (flag && pre == now->left) ? now : now->right->_min();
    }

    /// @brief 通りがけ順で node の前のノードを返す / 前のノードがあること / O(木の高さ)
    static NodePtr _prev(NodePtr node) {
        NodePtr now = node;
        NodePtr pre = nullptr;
        bool flag = now->left == pre;
        while (now->left == pre) {
            pre = now;
            now = now->par;
        }
        if (!now) return nullptr;
        return (flag && pre == now->right) ? now : now->right->_max();
    }
};
}
