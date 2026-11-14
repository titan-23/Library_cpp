/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/minimum_steiner_tree.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ds/union_find.cpp"
using namespace std;
namespace titan23 {
/// @brief シュタイナー木を作る方式
enum class SteinerTreeMethod { cached_prim, incremental_prim, mehlhorn, mst_prune };

/// @brief 木の辺 (u, v)、総費用、端点が連結だったかを持つ
template <class T>
struct SteinerTreeResult {
    vector<pair<int, int>> edges;
    T total_cost = T(0);
    bool feasible = true;
};

/// @brief 端点を全て含む木を近似的に求める / 非負重みの無向グラフで、構築後は変えない
template <class T>
class MinimumSteinerTree {
    static_assert(is_arithmetic_v<T> && !is_same_v<T, bool>);
    struct Edge { int u, v; T weight; };
    struct Arc { int to, edge; };
    struct Cache { vector<T> dist; vector<int> parent_edge; };
    struct Candidate { int terminal, closest; T distance; };
    using Result = SteinerTreeResult<T>;
    // 頂点ごとに要素を 1 個だけ持つ二分ヒープ。要素数は n 以下に収まる
    class Heap {
        const vector<T>& distance_;
        vector<int> heap_, position_;
        bool less(int a, int b) const {
            return distance_[a] < distance_[b] || (distance_[a] == distance_[b] && a < b);
        }
        void exchange(int a, int b) {
            swap(heap_[a], heap_[b]);
            position_[heap_[a]] = a; position_[heap_[b]] = b;
        }
    public:
        explicit Heap(const vector<T>& distance) : distance_(distance), position_(distance.size(), -1) { heap_.reserve(distance.size()); }
        bool empty() const { return heap_.empty(); }
        pair<T, int> top() const { return {distance_[heap_[0]], heap_[0]}; }
        void emplace(T, int vertex) {
            int pos = position_[vertex];
            if (pos < 0) { pos = heap_.size(); position_[vertex] = pos; heap_.push_back(vertex); }
            while (pos > 0) {
                int parent = (pos - 1) / 2;
                if (!less(heap_[pos], heap_[parent])) break;
                exchange(pos, parent); pos = parent;
            }
        }
        void pop() {
            int last = int(heap_.size()) - 1;
            exchange(0, last); position_[heap_.back()] = -1; heap_.pop_back();
            for (size_t pos = 0; 2 * pos + 1 < heap_.size();) {
                size_t child = 2 * pos + 1;
                if (child + 1 < heap_.size() && less(heap_[child + 1], heap_[child])) ++child;
                if (!less(heap_[child], heap_[pos])) break;
                exchange(pos, child); pos = child;
            }
        }
    };
    int n_ = 0, cached_count_ = 0;
    T inf_ = T(0);
    vector<Edge> edges_;
    vector<vector<Arc>> graph_;
    vector<int> component_;
    vector<Cache> cache_;
    vector<uint32_t> terminal_mark_, tree_mark_, edge_mark_;
    vector<Candidate> candidates_;
    vector<int> chosen_edges_, path_, added_vertices_;
    bool mst_ready_ = false;
    vector<int> mst_parent_edge_, mst_order_, mst_counts_;
    uint32_t generation_ = 0;

    static bool finite(T value) {
        if constexpr (is_floating_point_v<T>) return isfinite(value);
        return true;
    }
    T add_distance(T a, T b) const {
        if (a >= inf_ || b >= inf_ || a >= inf_ - b) return inf_;
        return a + b;
    }
    int other(int edge, int vertex) const { return edges_[edge].u ^ edges_[edge].v ^ vertex; }
    bool in_tree(int vertex) const { return tree_mark_[vertex] == generation_; }
    bool is_terminal(int vertex) const { return terminal_mark_[vertex] == generation_; }
    vector<int> prepare(const vector<int>& terminals) {
        if (++generation_ == 0) {
            fill(terminal_mark_.begin(), terminal_mark_.end(), 0);
            fill(tree_mark_.begin(), tree_mark_.end(), 0);
            fill(edge_mark_.begin(), edge_mark_.end(), 0);
            generation_ = 1;
        }
        vector<int> unique;
        unique.reserve(min(terminals.size(), size_t(n_)));
        for (int t : terminals) {
            if (t < 0 || t >= n_) throw invalid_argument("steiner: terminal out of range");
            if (!is_terminal(t)) { terminal_mark_[t] = generation_; unique.push_back(t); }
        }
        return unique;
    }
    bool connected(const vector<int>& terminals) const {
        for (int t : terminals) if (component_[t] != component_[terminals[0]]) return false;
        return true;
    }
    const Cache& distances(int source) {
        Cache& row = cache_[source];
        if (!row.dist.empty()) return row;
        row.dist.assign(n_, inf_); row.parent_edge.assign(n_, -1);
        Heap heap(row.dist); row.dist[source] = 0; heap.emplace(T(0), source);
        while (!heap.empty()) {
            auto [d, v] = heap.top(); heap.pop();
            if (d != row.dist[v]) continue;
            for (const auto& arc : graph_[v]) {
                T next = add_distance(d, edges_[arc.edge].weight);
                if (next < row.dist[arc.to]) {
                    row.dist[arc.to] = next; row.parent_edge[arc.to] = arc.edge;
                    heap.emplace(next, arc.to);
                }
            }
        }
        ++cached_count_;
        return row;
    }
    Result finish(const vector<int>& ids) const {
        Result result;
        result.edges.reserve(ids.size());
        for (int id : ids) {
            const Edge& edge = edges_[id];
            result.total_cost = add_distance(result.total_cost, edge.weight);
            if (result.total_cost >= inf_) throw overflow_error("steiner: tree cost must be below INF");
            result.edges.emplace_back(edge.u, edge.v);
        }
        return result;
    }
    void prepare_mst() {
        if (mst_ready_) return;
        vector<pair<T, int>> sorted;
        sorted.reserve(edges_.size());
        for (int id = 0; id < (int)edges_.size(); ++id) sorted.emplace_back(edges_[id].weight, id);
        sort(sorted.begin(), sorted.end());
        UnionFind components(n_);
        vector<vector<Arc>> tree(n_);
        for (auto [weight, id] : sorted) {
            const auto& edge = edges_[id];
            if (!components.unite(edge.u, edge.v)) continue;
            tree[edge.u].push_back({edge.v, id}); tree[edge.v].push_back({edge.u, id});
        }
        mst_parent_edge_.assign(n_, -1);
        mst_order_.clear(); mst_order_.reserve(n_);
        for (int root = 0; root < n_; ++root) if (mst_parent_edge_[root] == -1) {
            size_t begin = mst_order_.size();
            mst_parent_edge_[root] = -2; mst_order_.push_back(root);
            for (size_t i = begin; i < mst_order_.size(); ++i) {
                int v = mst_order_[i];
                for (auto arc : tree[v]) if (mst_parent_edge_[arc.to] == -1) {
                    mst_parent_edge_[arc.to] = arc.edge; mst_order_.push_back(arc.to);
                }
            }
        }
        for (int& edge : mst_parent_edge_) if (edge == -2) edge = -1;
        mst_counts_.assign(n_, 0);
        mst_ready_ = true;
    }
    // 全体の最小全域森の中で端点を結ぶ最小の部分木を返す / 準備後 O(n+k) / 定数近似の保証はない
    Result mst_prune(const vector<int>& terminals) {
        prepare_mst();
        fill(mst_counts_.begin(), mst_counts_.end(), 0);
        for (int t : terminals) mst_counts_[t] = 1;
        vector<int> ids;
        for (auto it = mst_order_.rbegin(); it != mst_order_.rend(); ++it) {
            int v = *it, id = mst_parent_edge_[v];
            if (id < 0) continue;
            if (mst_counts_[v] > 0 && mst_counts_[v] < (int)terminals.size()) ids.push_back(id);
            mst_counts_[other(id, v)] += mst_counts_[v];
        }
        return finish(ids);
    }
    // 端点ごとの最短路表を使い、木に最も近い端点を順に加える
    Result cached_prim(const vector<int>& terminals, int root, T bound) {
        auto& remaining = candidates_;
        remaining.clear();
        for (int t : terminals) if (t != root) remaining.push_back({t, root, distances(t).dist[root]});
        tree_mark_[root] = generation_;
        auto& ids = chosen_edges_;
        auto& path = path_;
        auto& vertices = added_vertices_;
        ids.clear();
        T total = 0;
        while (!remaining.empty()) {
            size_t selected = 0;
            for (size_t i = 1; i < remaining.size(); ++i) if (remaining[i].distance < remaining[selected].distance) selected = i;
            auto current = remaining[selected];
            remaining[selected] = remaining.back();
            remaining.pop_back();
            if (in_tree(current.terminal)) continue;
            if (current.distance >= inf_) throw overflow_error("steiner: shortest path must be below INF");
            path.clear(); vertices.clear();
            const auto& parent = cache_[current.terminal].parent_edge;
            for (int v = current.closest; v != current.terminal;) {
                int id = parent[v];
                if (id < 0) throw logic_error("steiner: invalid cached path");
                v = other(id, v);
                // 閉路を作らないよう、木と最後に交わった点より後ろだけ残す
                if (in_tree(v)) { path.clear(); vertices.clear(); }
                else { path.push_back(id); vertices.push_back(v); }
            }
            for (int id : path) {
                T next = add_distance(total, edges_[id].weight);
                if (bound < inf_ && next >= bound) return {{}, T(0), false};
                if (next >= inf_) throw overflow_error("steiner: tree cost must be below INF");
                total = next; ids.push_back(id);
            }
            for (int v : vertices) tree_mark_[v] = generation_;
            for (auto& candidate : remaining) {
                const auto& dist = cache_[candidate.terminal].dist;
                for (int v : vertices) if (dist[v] < candidate.distance) { candidate.distance = dist[v]; candidate.closest = v; }
            }
        }
        return finish(ids);
    }
    Result incremental_prim(const vector<int>& terminals, int root) {
        vector<T> dist(n_, inf_);
        vector<int> parent(n_, -1), ids;
        Heap heap(dist);
        dist[root] = 0; tree_mark_[root] = generation_;
        heap.emplace(T(0), root);
        int remaining = int(terminals.size()) - 1;
        while (remaining > 0 && !heap.empty()) {
            auto [d, v] = heap.top(); heap.pop();
            if (d != dist[v]) continue;
            if (is_terminal(v) && !in_tree(v)) {
                // 経路上の頂点を距離 0 の始点として加え、探索を続ける
                for (int u = v; !in_tree(u);) {
                    int id = parent[u];
                    if (id < 0) throw logic_error("steiner: invalid incremental path");
                    ids.push_back(id); tree_mark_[u] = generation_;
                    if (is_terminal(u)) --remaining;
                    dist[u] = 0; heap.emplace(T(0), u);
                    u = other(id, u);
                }
                continue;
            }
            for (const auto& arc : graph_[v]) {
                T next = add_distance(d, edges_[arc.edge].weight);
                if (next < dist[arc.to]) {
                    dist[arc.to] = next; parent[arc.to] = arc.edge;
                    heap.emplace(next, arc.to);
                }
            }
        }
        if (remaining > 0) throw overflow_error("steiner: shortest path must be below INF");
        return finish(ids);
    }
    Result mehlhorn(const vector<int>& terminals) {
        vector<T> dist(n_, inf_);
        vector<int> owner(n_, -1), parent(n_, -1);
        Heap heap(dist);
        for (int t : terminals) { dist[t] = 0; owner[t] = t; heap.emplace(T(0), t); }
        while (!heap.empty()) {
            auto [d, v] = heap.top(); heap.pop();
            if (d != dist[v]) continue;
            for (const auto& arc : graph_[v]) {
                T next = add_distance(d, edges_[arc.edge].weight);
                if (next < dist[arc.to]) {
                    dist[arc.to] = next; owner[arc.to] = owner[v]; parent[arc.to] = arc.edge;
                    heap.emplace(next, arc.to);
                }
            }
        }
        vector<pair<T, int>> boundary;
        for (int id = 0; id < (int)edges_.size(); ++id) {
            const Edge& edge = edges_[id];
            if (owner[edge.u] < 0 || owner[edge.v] < 0 || owner[edge.u] == owner[edge.v]) continue;
            T cost = add_distance(add_distance(dist[edge.u], edge.weight), dist[edge.v]);
            if (cost < inf_) boundary.emplace_back(cost, id);
        }
        sort(boundary.begin(), boundary.end());
        UnionFind components(n_);
        vector<int> ids;
        int remaining = int(terminals.size()) - 1;
        for (auto [cost, id] : boundary) {
            const Edge& edge = edges_[id];
            if (!components.unite(owner[edge.u], owner[edge.v])) continue;
            ids.push_back(id);
            for (int endpoint : {edge.u, edge.v}) {
                for (int v = endpoint; parent[v] >= 0 && !in_tree(v); v = other(parent[v], v)) {
                    tree_mark_[v] = generation_; ids.push_back(parent[v]);
                }
            }
            if (--remaining == 0) break;
        }
        if (remaining > 0) throw overflow_error("steiner: terminal connection cost must be below INF");
        // 展開後に閉路はないので、端点でない葉を刈るだけでよい
        vector<int> degree(n_, 0), leaves;
        for (int id : ids) { edge_mark_[id] = generation_; ++degree[edges_[id].u]; ++degree[edges_[id].v]; }
        for (int v = 0; v < n_; ++v) if (degree[v] == 1 && !is_terminal(v)) leaves.push_back(v);
        for (size_t head = 0; head < leaves.size(); ++head) {
            int v = leaves[head];
            if (degree[v] != 1) continue;
            for (auto arc : graph_[v]) if (edge_mark_[arc.edge] == generation_) {
                edge_mark_[arc.edge] = 0; --degree[v]; --degree[arc.to];
                if (degree[arc.to] == 1 && !is_terminal(arc.to)) leaves.push_back(arc.to);
                break;
            }
        }
        ids.erase(remove_if(ids.begin(), ids.end(), [&](int id) { return edge_mark_[id] != generation_; }), ids.end());
        return finish(ids);
    }
    static vector<pair<int, int>> require_feasible(Result result) {
        if (!result.feasible) throw runtime_error("steiner: terminals are disconnected");
        return move(result.edges);
    }

public:
    MinimumSteinerTree() = default;
    /// @brief 隣接リスト graph から構築する / 重みは 0 以上 inf 未満で、各辺を両向きに同じ最小重みで与える
    MinimumSteinerTree(const vector<vector<pair<int, T>>>& graph, T inf) : inf_(inf) {
        if (graph.size() > size_t(numeric_limits<int>::max())) throw invalid_argument("steiner: too many vertices");
        if (!finite(inf) || inf <= 0) throw invalid_argument("steiner: INF must be positive and finite");
        n_ = graph.size();
        struct InputEdge { int u, v; T weight; bool reverse; };
        vector<InputEdge> input;
        for (int u = 0; u < n_; ++u) for (auto [v, weight] : graph[u]) {
            if (v < 0 || v >= n_ || !finite(weight) || weight < 0 || weight >= inf_)
                throw invalid_argument("steiner: invalid endpoint or weight");
            if (u != v) input.push_back({min(u, v), max(u, v), weight, u > v});
        }
        sort(input.begin(), input.end(), [](const auto& a, const auto& b) { return pair(a.u, a.v) < pair(b.u, b.v); });
        graph_.resize(n_); cache_.resize(n_);
        UnionFind components(n_);
        for (size_t begin = 0; begin < input.size();) {
            size_t end = begin;
            T weights[2] = {inf_, inf_};
            while (end < input.size() && input[end].u == input[begin].u && input[end].v == input[begin].v) {
                auto edge = input[end++]; weights[edge.reverse] = min(weights[edge.reverse], edge.weight);
            }
            if (weights[0] != weights[1]) throw invalid_argument("steiner: graph must be undirected with symmetric minimum weights");
            if (edges_.size() >= size_t(numeric_limits<int>::max())) throw invalid_argument("steiner: too many edges");
            int u = input[begin].u, v = input[begin].v, id = edges_.size();
            edges_.push_back({u, v, weights[0]});
            graph_[u].push_back({v, id}); graph_[v].push_back({u, id});
            components.unite(u, v);
            begin = end;
        }
        component_.resize(n_);
        for (int v = 0; v < n_; ++v) component_[v] = components.root(v);
        terminal_mark_.assign(n_, 0); tree_mark_.assign(n_, 0); edge_mark_.assign(edges_.size(), 0);
    }

    /// @brief terminals を結ぶ木を method で求める / 端点が非連結なら feasible = false
    Result solve(const vector<int>& terminals, SteinerTreeMethod method = SteinerTreeMethod::cached_prim) {
        auto unique = prepare(terminals);
        if (unique.size() <= 1) return {};
        if (!connected(unique)) return {{}, T(0), false};
        if (unique.size() == size_t(n_)) return mst_prune(unique);
        if (method == SteinerTreeMethod::cached_prim) return cached_prim(unique, terminals.back(), inf_);
        if (method == SteinerTreeMethod::incremental_prim) return incremental_prim(unique, terminals.back());
        if (method == SteinerTreeMethod::mehlhorn) return mehlhorn(unique);
        if (method == SteinerTreeMethod::mst_prune) return mst_prune(unique);
        throw invalid_argument("steiner: unknown method");
    }
    /// @brief cached_prim で求めた木の辺を返す / 端点が非連結なら runtime_error
    vector<pair<int, int>> build_prim(const vector<int>& terminals) { return require_feasible(solve(terminals)); }
    /// @brief incremental_prim で求めた木の辺を返す / 端点が非連結なら runtime_error
    vector<pair<int, int>> build_prim_dijkstra(const vector<int>& terminals) { return require_feasible(solve(terminals, SteinerTreeMethod::incremental_prim)); }
    /// @brief mehlhorn で求めた木の辺を返す / 端点が非連結なら runtime_error
    vector<pair<int, int>> build_mehlhorn(const vector<int>& terminals) { return require_feasible(solve(terminals, SteinerTreeMethod::mehlhorn)); }
    /// @brief mst_prune で求めた木の辺を返す / 端点が非連結なら runtime_error
    vector<pair<int, int>> build_mst_prune(const vector<int>& terminals) { return require_feasible(solve(terminals, SteinerTreeMethod::mst_prune)); }

    /// @brief 全ての端点を根に cached_prim を試し、最良の木の辺を返す / 端点が非連結なら runtime_error
    vector<pair<int, int>> build_prim_fullsearch(const vector<int>& terminals) {
        auto unique = prepare(terminals);
        if (unique.size() <= 1) return {};
        if (!connected(unique)) throw runtime_error("steiner: terminals are disconnected");
        if (unique.size() == size_t(n_)) return mst_prune(unique).edges;
        Result best; best.total_cost = inf_;
        for (int root : unique) {
            prepare(unique);
            auto candidate = cached_prim(unique, root, best.total_cost);
            if (candidate.feasible && candidate.total_cost < best.total_cost) best = move(candidate);
            if (best.total_cost == 0) break;
        }
        return move(best.edges);
    }

    /// @brief mehlhorn の解と、最大 starts 個の根から growth で育てた解のうち最良を返す / growth は Prim の 2 方式のどちらか / 最初の根は terminals.back()
    Result solve_best(const vector<int>& terminals, int starts = 8, uint32_t seed = 0,
                      SteinerTreeMethod growth = SteinerTreeMethod::incremental_prim) {
        if (starts < 0) throw invalid_argument("steiner: starts must be nonnegative");
        if (growth != SteinerTreeMethod::cached_prim && growth != SteinerTreeMethod::incremental_prim)
            throw invalid_argument("steiner: growth method must be a Prim variant");
        auto unique = prepare(terminals);
        auto best = solve(unique, SteinerTreeMethod::mehlhorn);
        if (!best.feasible || unique.size() <= 1 || unique.size() == size_t(n_) || best.total_cost == 0) return best;
        vector<int> roots = unique;
        swap(roots[0], *find(roots.begin(), roots.end(), terminals.back()));
        Random random(seed);
        for (int i = 0; i < min(starts, int(roots.size())); ++i) {
            if (i > 0) swap(roots[i], roots[random.randrange(i, roots.size())]);
            prepare(unique);
            auto candidate = growth == SteinerTreeMethod::cached_prim ? cached_prim(unique, roots[i], best.total_cost) : incremental_prim(unique, roots[i]);
            if (candidate.feasible && candidate.total_cost < best.total_cost) best = move(candidate);
            if (best.total_cost == 0) break;
        }
        return best;
    }
    /// @brief 最短路表を作った始点の数を返す
    int cached_source_count() const { return cached_count_; }
    /// @brief 最短路表と最小全域森を捨てる
    void clear_cache() {
        for (auto& row : cache_) { vector<T>().swap(row.dist); vector<int>().swap(row.parent_edge); }
        cached_count_ = 0;
        mst_ready_ = false;
        vector<int>().swap(mst_parent_edge_); vector<int>().swap(mst_order_); vector<int>().swap(mst_counts_);
    }
};
}
