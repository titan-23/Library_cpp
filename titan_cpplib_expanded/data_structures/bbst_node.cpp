// #include "titan_cpplib/data_structures/bbst_node.cpp"
using namespace std;

// BBSTNode
namespace titan23 {

    template<typename NodePtr>
    class BBSTNode {
      public:
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

        static NodePtr rotate_LR(NodePtr node) {
            node->left = rotate_left(node->left);
            return rotate_right(node);
        }

        static NodePtr rotate_RL(NodePtr node) {
            node->right = rotate_right(node->right);
            return rotate_left(node);
        }

        static NodePtr _min(NodePtr node) {
            while (node->left) node = node->left;
            return node;
        }

        static NodePtr _max(NodePtr node) {
            while (node->right) node = node->right;
            return node;
        }

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

