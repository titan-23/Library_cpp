/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/mo_tree_vertex_edge.cpp
#pragma once

#include "titan_cpplib/graph/tree/mo_tree_base.cpp"

namespace titan23 {

/// @brief 木上の Mo（頂点・辺属性）
class MoTreeVertexEdge : public MoTreeBase<true> {
public:
    using MoTreeBase<true>::MoTreeBase;

    /// @brief 全クエリを処理する / O(N√Q + QlogQ + (N+Q)α(N))
    /// add_vertex(v), del_vertex(v): 頂点 v の追加・削除
    /// add_edge(e), del_edge(e): 辺番号 e の追加・削除 / out(i): クエリ i の回答
    template<typename AV, typename DV, typename AE, typename DE, typename OUT>
    void run(AV &&add_vertex, DV &&del_vertex, AE &&add_edge, DE &&del_edge, OUT &&out) {
        this->template run_impl<true>(add_vertex, del_vertex, add_edge, del_edge, out);
    }
};

}
