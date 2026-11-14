/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/mo_tree_vertex_value.cpp
#pragma once

#include "titan_cpplib/graph/tree/mo_tree_base.cpp"

namespace titan23 {

/// @brief パス上の頂点の値を出し入れしてパスクエリを処理する木上の Mo / add と del には頂点番号でなく値を渡す
template<typename T>
class MoTreeVertexValue : public MoTreeBase<true> {
private:
    vector<T> values;

public:
    /// @brief 頂点の値 values と根 root で初期化する
    MoTreeVertexValue(const vector<T> &values, int root = 0)
        : MoTreeBase<true>(values.size(), root), values(values) {}

    /// @brief 無向木 G、頂点の値 values と根 root から構築する
    MoTreeVertexValue(const vector<vector<int>> &G, const vector<T> &values, int root = 0)
        : MoTreeBase<true>(G, root), values(values) {}

    /// @brief 全クエリを処理する / O(N√Q + QlogQ + (N+Q)α(N))
    /// add(x), del(x): 頂点値 x の追加・削除 / out(i): クエリ i の回答
    template<typename ADD, typename DEL, typename OUT>
    void run(ADD &&add, DEL &&del, OUT &&out) {
        auto noop = [] (int) {};
        this->template run_impl<false, true>(values, add, del, noop, noop, out);
    }
};

}
