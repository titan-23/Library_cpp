/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/mo_tree_edge.cpp
#pragma once

#include "titan_cpplib/graph/tree/mo_tree_base.cpp"

namespace titan23 {

/// @brief パス上の辺を出し入れしてパスクエリを処理する木上の Mo
class MoTreeEdge : public MoTreeBase<false> {
public:
    using MoTreeBase<false>::MoTreeBase;

    /// @brief 全クエリを処理する / O(N√Q + QlogQ)
    /// add(e), del(e): 辺番号 e の追加・削除 / out(i): クエリ i の回答
    template<typename ADD, typename DEL, typename OUT>
    void run(ADD &&add, DEL &&del, OUT &&out) {
        auto noop = [] (int) {};
        this->template run_impl<true>(noop, noop, add, del, out);
    }
};

}
