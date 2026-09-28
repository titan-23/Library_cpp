/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/mo_tree_vertex.cpp
#pragma once

#include "titan_cpplib/graph/tree/mo_tree_base.cpp"

namespace titan23 {

/// @brief 木上の Mo（頂点属性）
class MoTreeVertex : public MoTreeBase<true> {
public:
    using MoTreeBase<true>::MoTreeBase;

    /// @brief 全クエリを処理する / O(N√Q + QlogQ + (N+Q)α(N))
    /// add(v), del(v): 頂点 v の追加・削除 / out(i): クエリ i の回答
    template<typename ADD, typename DEL, typename OUT>
    void run(ADD &&add, DEL &&del, OUT &&out) {
        auto noop = [] (int) {};
        this->template run_impl<false>(add, del, noop, noop, out);
    }
};

}
