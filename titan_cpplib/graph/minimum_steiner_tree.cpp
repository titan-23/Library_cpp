/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/minimum_steiner_tree.cpp
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ds/union_find.cpp"
using namespace std;
namespace titan23 {
/// @brief シュタイナー木を求める方式
enum class SteinerTreeMethod { cached_prim, incremental_prim, mehlhorn, mst_prune };

/// @brief 木の辺 (u, v)、総コスト、端点が連結であったかを保持する
template <class T>
struct SteinerTreeResult {
    vector<pair<int, int>> edges;
    T total_cost = T(0);
    bool feasible = true;
};

/// @brief 辺を更新できる非負重みの無向グラフで、端点をすべて含む木を近似的に求める
template <class T>
class MinimumSteinerTree {
    static_assert(is_arithmetic_v<T> && !is_same_v<T, bool>);
    struct Edge { int u, v; T weight; };
    struct Arc { int to, edge; };
    struct Cache {
        vector<T> dist;
        vector<int> parent; // 整数重みは親頂点、浮動小数点重みは復元時に加算する辺の番号
        // 緩和を再開する頂点と、親辺を失った部分木の根。変更がたまった行は次回作り直す。
        array<int, 8> pending{};
        int count = 0;
        bool remember(int v) {
            for (int i = 0; i < count; ++i) if (pending[i] == v) return true;
            if (count == int(pending.size())) return false;
            pending[count++] = v;
            return true;
        }
    };
    struct Candidate { int terminal, closest; T distance; const T* row; };
    using Result = SteinerTreeResult<T>;
    // 葉は頂点番号順に並べる。各段には次の段のグループごとの最小距離を持つ。
    // decrease と pop は O(log n)、領域は O(n)。距離が同じなら最小の頂点番号を返す。
    class VertexMinQueue {
        static constexpr size_t width = is_floating_point_v<T> ? 16 : 32;
        vector<size_t> levels_;
        vector<T> keys_;

    public:
        void reset(int n, T inf) {
            levels_.clear();
            size_t total = 0;
            for (size_t count = n; count > 1;) {
                levels_.push_back(total);
                count = (count + width - 1) / width;
                total += count * width;
            }
            levels_.push_back(total);
            keys_.assign(total + 1, inf);
        }

        void decrease(int vertex, T distance) {
            size_t slot = vertex;
            for (size_t level : levels_) {
                T& previous = keys_[level + slot];
                if (distance >= previous) break;
                previous = distance;
                slot /= width;
            }
        }

        T minimum() const { return keys_.back(); }

        int pop_minimum(T distance, T inf) {
            size_t slot = 0;
            for (size_t i = levels_.size() - 1; i > 0; --i) {
                slot *= width;
                const T* group = keys_.data() + levels_[i - 1] + slot;
                size_t j = 0;
                while (group[j] != distance) ++j;
                slot += j;
            }
            const int vertex = int(slot);
            keys_[slot] = inf;
            for (size_t i = 1; i < levels_.size(); ++i) {
                slot /= width;
                const T* group = keys_.data() + levels_[i - 1] + slot * width;
                T best = group[0];
                for (size_t j = 1; j < width; ++j) best = min(best, group[j]);
                T& previous = keys_[levels_[i] + slot];
                if (best == previous) break;
                previous = best;
            }
            return vertex;
        }
    };
    VertexMinQueue queue_;
    int n_ = 0;
    T inf_ = T(0);
    vector<Edge> edges_;
    vector<vector<Arc>> graph_;
    UnionFind components_{0};
    vector<uint8_t> forest_edge_; // 連結成分の判定に使った全域森の辺
    bool components_dirty_ = false;
    vector<Cache> cache_;
    vector<int> cached_sources_;
    int ready_sources_ = 0;
    vector<int> detached_vertices_;
    vector<T> dense_weights_; // 密グラフで距離表を新規作成する際に確保する
    unsigned dense_rows_seen_ = 0;
    struct ReducedGraph {
        vector<T> hub_distance;
        vector<int> hub_parent, next_dirty;
        vector<vector<Arc>> graph;
        int dirty_head = -1;
        size_t edits_until_retry = 0;
    };
    ReducedGraph reduced_;
    vector<uint32_t> terminal_mark_, tree_mark_;
    vector<int> unique_terminals_;
    vector<Candidate> candidates_;
    vector<pair<int, int>> chosen_edges_;
    vector<int> added_vertices_;
    bool mst_ready_ = false;
    vector<int> mst_parent_edge_, mst_depth_;
    T mst_weight_bound_ = T(0);
    uint32_t generation_ = 0;

    static bool finite(T value) {
        if constexpr (is_floating_point_v<T>) return isfinite(value);
        return true;
    }
    T add_distance(T a, T b) const {
        if (a >= inf_ || b >= inf_ || a >= inf_ - b) return inf_;
        return a + b;
    }
    bool relax_distance(T from, T weight, T& distance) const {
        if constexpr (is_integral_v<T> && is_signed_v<T>) {
            // 非負の符号付き整数 2 個の和は対応する符号なし整数に収まる。
            using U = make_unsigned_t<T>;
            auto sum = U(from) + U(weight);
            if (sum >= U(distance)) return false;
            distance = T(sum);
        } else if constexpr (is_integral_v<T>) {
            T sum = T(from + weight);
            if (sum < from || sum >= distance) return false;
            distance = sum;
        } else {
            if (from >= inf_ - weight) return false;
            T next = from + weight;
            if (next >= distance) return false;
            distance = next;
        }
        return true;
    }
    int other(int edge, int vertex) const { return edges_[edge].u ^ edges_[edge].v ^ vertex; }
    static int parent_link(int parent, int edge) {
        if constexpr (is_integral_v<T>) return parent;
        else return edge;
    }
    bool in_tree(int vertex) const { return tree_mark_[vertex] == generation_; }
    bool is_terminal(int vertex) const { return terminal_mark_[vertex] == generation_; }
    void next_generation() {
        if (++generation_ == 0) {
            fill(terminal_mark_.begin(), terminal_mark_.end(), 0);
            fill(tree_mark_.begin(), tree_mark_.end(), 0);
            generation_ = 1;
        }
    }
    // 端子に重複がある問い合わせだけ、重複を取り除いた列を作る。
    const vector<int>& prepare(const vector<int>& terminals) {
        next_generation();
        size_t i = 0;
        for (; i < terminals.size(); ++i) {
            int t = terminals[i];
            if (t < 0 || t >= n_) throw invalid_argument("steiner: terminal out of range");
            if (is_terminal(t)) break;
            terminal_mark_[t] = generation_;
        }
        if (i == terminals.size()) return terminals;
        unique_terminals_.assign(terminals.begin(), terminals.begin() + i);
        for (++i; i < terminals.size(); ++i) {
            int t = terminals[i];
            if (t < 0 || t >= n_) throw invalid_argument("steiner: terminal out of range");
            if (!is_terminal(t)) { terminal_mark_[t] = generation_; unique_terminals_.push_back(t); }
        }
        return unique_terminals_;
    }
    bool connected(const vector<int>& terminals) {
        if (components_dirty_) {
            components_.clear();
            for (size_t id = 0; id < edges_.size(); ++id) {
                const Edge& e = edges_[id];
                forest_edge_[id] = components_.unite(e.u, e.v);
            }
            components_dirty_ = false;
        }
        if (components_.group_count() == 1) return true;
        int root = components_.root(terminals.front());
        for (int t : terminals) if (components_.root(t) != root) return false;
        return true;
    }
    auto arc_position(int u, int v) {
        return lower_bound(graph_[u].begin(), graph_[u].end(), v,
                           [](const Arc& arc, int to) { return arc.to < to; });
    }
    void check_vertex(int v) const {
        if (v < 0 || v >= n_) throw invalid_argument("steiner: vertex out of range");
    }
    bool in_cached_mst(int u, int v, int id) const {
        return mst_ready_ && (mst_parent_edge_[u] == id || mst_parent_edge_[v] == id);
    }
    void record_distance_update(int u, int v, int id, T weight, bool shortening) {
        if (reduced_.edits_until_retry) --reduced_.edits_until_retry;
        size_t kept = 0;
        ready_sources_ = 0;
        for (int s : cached_sources_) {
            auto& row = cache_[s];
            int seed = -1;
            if (shortening) {
                if (add_distance(row.dist[u], weight) < row.dist[v]) seed = u;
                else if (add_distance(row.dist[v], weight) < row.dist[u]) seed = v;
            } else {
                if (row.parent[u] == parent_link(v, id)) seed = u;
                else if (row.parent[v] == parent_link(u, id)) seed = v;
                if (seed >= 0) row.parent[seed] = -1;
            }
            if (seed >= 0 && !row.remember(seed)) { row = {}; continue; }
            cached_sources_[kept++] = s;
            if (row.count == 0) ++ready_sources_;
        }
        cached_sources_.resize(kept);
    }
    // 複数の配列を同時に伸ばす前に、必要な領域をすべて確保する。
    template<class Value>
    static void reserve_one(vector<Value>& items) {
        if (items.size() == items.capacity()) items.reserve(items.size() + items.size() / 2 + 1);
    }
    // 親を失った部分木だけを切り離し、外から入る辺で探索を再開する。
    void seed_repairs(Cache& row, int source) {
        auto& detached = detached_vertices_;
        detached.clear();
        for (int i = 0; i < row.count; ++i) {
            int v = row.pending[i];
            if (v != source && row.parent[v] == -1) {
                if (detached.empty()) detached.reserve(n_);
                row.dist[v] = inf_;
                detached.push_back(v);
            }
        }
        for (size_t i = 0; i < detached.size(); ++i) {
            for (auto arc : graph_[detached[i]]) if (row.parent[arc.to] == parent_link(detached[i], arc.edge)) {
                row.parent[arc.to] = -1;
                row.dist[arc.to] = inf_;
                detached.push_back(arc.to);
            }
        }
        for (int v : detached) {
            for (auto arc : graph_[v]) {
                if (relax_distance(row.dist[arc.to], edges_[arc.edge].weight, row.dist[v]))
                    row.parent[v] = parent_link(arc.to, arc.edge);
            }
            if (row.dist[v] < inf_) queue_.decrease(v, row.dist[v]);
        }
        for (int i = 0; i < row.count; ++i) {
            int v = row.pending[i];
            if (row.dist[v] < inf_) queue_.decrease(v, row.dist[v]);
        }
    }
    void propagate(Cache& row, const vector<vector<Arc>>& graph) {
        while (queue_.minimum() < inf_) {
            T d = queue_.minimum();
            int v = queue_.pop_minimum(d, inf_);
            for (const auto& arc : graph[v]) {
                if (relax_distance(d, edges_[arc.edge].weight, row.dist[arc.to])) {
                    row.parent[arc.to] = parent_link(v, arc.edge);
                    queue_.decrease(arc.to, row.dist[arc.to]);
                }
            }
        }
    }
    void set_dense_weight(int u, int v, T weight) {
        dense_weights_[size_t(u) * n_ + v] = weight;
        dense_weights_[size_t(v) * n_ + u] = weight;
    }
    bool use_dense_distances() {
        if (!dense_weights_.empty()) return true;
        uint64_t cells = uint64_t(n_) * n_;
        if (4ULL * edges_.size() < cells - n_ || cells > dense_weights_.max_size()) return false;
        // 少数の始点だけを調べる場合は行列の初期化を省く。
        if (dense_rows_seen_ < 4) { ++dense_rows_seen_; return false; }
        try {
            dense_weights_.assign(size_t(cells), inf_);
        } catch (const bad_alloc&) {
            return false;
        }
        for (const auto& e : edges_) set_dense_weight(e.u, e.v, e.weight);
        return true;
    }
    void dense_dijkstra(Cache& row, int source) {
        vector<T> active(n_, inf_);
        active[source] = row.dist[source] = T(0);
        for (;;) {
            T d = inf_;
            for (T key : active) d = min(d, key);
            if (d >= inf_) break;
            int u = int(find(active.begin(), active.end(), d) - active.begin());
            const T* weights = dense_weights_.data() + size_t(u) * n_;
            for (int v = 0; v < n_; ++v) {
                if (relax_distance(d, weights[v], row.dist[v])) {
                    active[v] = row.dist[v];
                    row.parent[v] = u;
                }
            }
            active[u] = inf_;
        }
    }
    // 中継点を経由する既知の経路より重い辺は、整数重みの最短路に含まれない。
    bool keep_for_distances(int u, int v, T weight) const {
        return weight <= add_distance(reduced_.hub_distance[u], reduced_.hub_distance[v]);
    }
    void rebuild_reduced_row(int u) {
        auto& row = reduced_.graph[u];
        row.clear();
        for (auto arc : graph_[u]) {
            if (keep_for_distances(u, arc.to, edges_[arc.edge].weight)) row.push_back(arc);
        }
    }
    void prepare_reduction() {
        if (!reduced_.hub_distance.empty() || reduced_.edits_until_retry || cached_sources_.size() < 4 ||
            edges_.size() / size_t(n_) < 8) return;
        const Cache& hub = cache_[cached_sources_.back()];
        if (hub.count) return;
        // 辺を十分に省けそうか標本で見積もる。実際に除外する辺はすべて個別に判定する。
        int retained = 0;
        for (size_t i = 0; i < 64; ++i) {
            const Edge& e = edges_[uint64_t(i) * edges_.size() / 64];
            if (e.weight <= add_distance(hub.dist[e.u], hub.dist[e.v])) ++retained;
        }
        if (retained >= 32) {
            reduced_.edits_until_retry = edges_.size() / 8;
            return;
        }
        try {
            reduced_.hub_distance = hub.dist;
            reduced_.hub_parent = hub.parent;
            reduced_.graph.resize(n_);
            reduced_.next_dirty.assign(n_, -2);
            reduced_.dirty_head = -1;
            for (int v = 0; v < n_; ++v) rebuild_reduced_row(v);
        } catch (const bad_alloc&) {
            reduced_ = {};
            reduced_.edits_until_retry = edges_.size() / 8;
        }
    }
    const vector<vector<Arc>>& distance_graph() {
        if (reduced_.hub_distance.empty()) return graph_;
        while (reduced_.dirty_head >= 0) {
            int v = reduced_.dirty_head;
            try {
                rebuild_reduced_row(v);
            } catch (const bad_alloc&) {
                reduced_ = {};
                reduced_.edits_until_retry = edges_.size() / 8;
                return graph_;
            }
            reduced_.dirty_head = reduced_.next_dirty[v];
            reduced_.next_dirty[v] = -2;
        }
        return reduced_.graph;
    }
    void mark_reduced_endpoints(int u, int v) {
        for (int x : {u, v}) if (reduced_.next_dirty[x] == -2) {
            reduced_.next_dirty[x] = reduced_.dirty_head;
            reduced_.dirty_head = x;
        }
    }
    void update_reduction(int u, int v, T before, T after) {
        if (reduced_.hub_distance.empty()) return;
        if (after > before && (reduced_.hub_parent[u] == v || reduced_.hub_parent[v] == u)) {
            reduced_.hub_distance.clear();
            return;
        }
        bool was_kept = before < inf_ && keep_for_distances(u, v, before);
        bool kept = after < inf_ && keep_for_distances(u, v, after);
        if (kept != was_kept) mark_reduced_endpoints(u, v);
    }
    Cache make_distances(int source) {
        Cache row;
        row.dist.assign(n_, inf_); row.parent.assign(n_, -1);
        if constexpr (is_integral_v<T>) {
            prepare_reduction();
            if (reduced_.hub_distance.empty() && use_dense_distances()) {
                dense_dijkstra(row, source);
                return row;
            }
        }
        queue_.reset(n_, inf_);
        row.dist[source] = 0;
        queue_.decrease(source, T(0));
        propagate(row, distance_graph());
        return row;
    }
    const Cache& distances(int source) {
        Cache& cached = cache_[source];
        if (!cached.dist.empty()) {
            if (cached.count) {
                const auto& graph = distance_graph();
                queue_.reset(n_, inf_);
                seed_repairs(cached, source);
                propagate(cached, graph);
                cached.count = 0;
                ++ready_sources_;
            }
            return cached;
        }
        Cache row = make_distances(source);
        cached_sources_.push_back(source);
        cached = move(row);
        ++ready_sources_;
        return cached;
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
    // 辺 (u,v) を加えた閉路で、この辺が最も重ければ現在の最小全域森を使い続けられる。
    bool preserves_mst(int u, int v, T weight) {
        if (weight >= mst_weight_bound_ && !components_dirty_) return components_.same(u, v);
        while (u != v) {
            if (mst_depth_[u] < mst_depth_[v]) swap(u, v);
            int id = mst_parent_edge_[u];
            if (id < 0 || edges_[id].weight > weight) return false;
            u = other(id, u);
        }
        return true;
    }
    void prepare_mst() {
        if (mst_ready_) return;
        // Prim 法。重みが同じ辺は辺番号で比較し、Kruskal 法と同じ森を選ぶ。
        constexpr int unreached = -2;
        mst_parent_edge_.assign(n_, unreached);
        mst_depth_.assign(n_, 0);
        mst_weight_bound_ = T(0);
        vector<int> candidate(n_, -1);
        struct Entry { T weight; int edge, to; };
        auto later = [](const Entry& a, const Entry& b) {
            return a.weight != b.weight ? a.weight > b.weight : a.edge > b.edge;
        };
        vector<Entry> heap;
        heap.reserve(n_);
        for (int root = 0; root < n_; ++root) {
            if (mst_parent_edge_[root] != unreached) continue;
            heap.push_back({T(0), -1, root});
            while (!heap.empty()) {
                pop_heap(heap.begin(), heap.end(), later);
                auto [weight, id, v] = heap.back();
                heap.pop_back();
                if (mst_parent_edge_[v] != unreached) continue;
                mst_parent_edge_[v] = id;
                if (id >= 0) {
                    mst_depth_[v] = mst_depth_[other(id, v)] + 1;
                    mst_weight_bound_ = max(mst_weight_bound_, weight);
                }
                for (auto arc : graph_[v]) {
                    if (mst_parent_edge_[arc.to] != unreached) continue;
                    int old = candidate[arc.to];
                    Entry next{edges_[arc.edge].weight, arc.edge, arc.to};
                    if (old >= 0 && !later({edges_[old].weight, old, arc.to}, next)) continue;
                    candidate[arc.to] = arc.edge;
                    heap.push_back(next);
                    push_heap(heap.begin(), heap.end(), later);
                }
            }
        }
        mst_ready_ = true;
    }
    // 最小全域森の中で端子を結ぶ部分木を返す / 準備後 O(端子数 + 最初の端子の深さ + 結果の辺数)
    Result mst_prune(const vector<int>& terminals) {
        prepare_mst();
        int first = terminals.front(), common = first;
        for (int v = first;;) {
            tree_mark_[v] = generation_;
            int id = mst_parent_edge_[v];
            if (id < 0) break;
            v = other(id, v);
        }
        vector<int> ids;
        ids.reserve(terminals.size() - 1);
        for (int t : terminals) {
            int v = t;
            while (!in_tree(v)) {
                tree_mark_[v] = generation_;
                int id = mst_parent_edge_[v];
                ids.push_back(id);
                v = other(id, v);
            }
            // 既に加えた枝はすべて common の下にある。上で合流したときだけ共通祖先を更新する。
            if (mst_depth_[v] < mst_depth_[common]) common = v;
        }
        for (int v = first; v != common;) {
            int id = mst_parent_edge_[v];
            ids.push_back(id);
            v = other(id, v);
        }
        return finish(ids);
    }
    // 端点ごとの最短路表を使用し、木に最も近い端点を順に加える
    Result cached_prim(const vector<int>& terminals, int root, T bound) {
        auto& remaining = candidates_;
        // 返却・枝刈り・例外のどの経路でも、距離表へのポインタを今回の探索内に限定する。
        struct ClearCandidates {
            vector<Candidate>& list;
            ~ClearCandidates() { list.clear(); }
        } cleanup{remaining};
        size_t selected = 0;
        T nearest = inf_;
        for (int t : terminals) if (t != root) {
            const T* row = distances(t).dist.data();
            T distance = row[root];
            if (distance < nearest) { nearest = distance; selected = remaining.size(); }
            remaining.push_back({t, root, distance, row});
        }
        tree_mark_[root] = generation_;
        chosen_edges_.clear();
        auto& vertices = added_vertices_;
        T total = 0;
        while (!remaining.empty()) {
            auto current = remaining[selected];
            remaining[selected] = remaining.back();
            remaining.pop_back();
            vertices.clear();
            if (!in_tree(current.terminal)) {
                if (current.distance >= inf_) throw overflow_error("steiner: shortest path must be below INF");
                // 木までの最短距離は、追加する経路の総重みに等しい。
                if constexpr (is_integral_v<T>) {
                    total = add_distance(total, current.distance);
                    if (bound < inf_ && total >= bound) return {{}, T(0), false};
                    if (total >= inf_) throw overflow_error("steiner: tree cost must be below INF");
                }
                const size_t previous_size = chosen_edges_.size();
                const T previous_cost = total;
                const auto& parent = cache_[current.terminal].parent;
                for (int v = current.closest; v != current.terminal;) {
                    int link = parent[v], previous = v;
                    if (link < 0) throw logic_error("steiner: invalid cached path");
                    if constexpr (is_integral_v<T>) v = link;
                    else v = other(link, v);
                    if (in_tree(v)) {
                        // 最後に木と交わった頂点より後ろの経路だけを加える
                        chosen_edges_.erase(chosen_edges_.begin() + previous_size, chosen_edges_.end());
                        vertices.clear();
                        if constexpr (is_floating_point_v<T>) total = previous_cost;
                    } else {
                        if constexpr (is_integral_v<T>) {
                            chosen_edges_.emplace_back(min(previous, v), max(previous, v));
                        } else {
                            const Edge& edge = edges_[link];
                            chosen_edges_.emplace_back(edge.u, edge.v);
                            total = add_distance(total, edge.weight);
                        }
                        vertices.push_back(v);
                    }
                }
                if constexpr (is_floating_point_v<T>) {
                    if (bound < inf_ && total >= bound) return {{}, T(0), false};
                    if (total >= inf_) throw overflow_error("steiner: tree cost must be below INF");
                }
                for (int v : vertices) tree_mark_[v] = generation_;
            }
            if (remaining.empty()) break;
            selected = 0;
            nearest = inf_;
            // 新しい頂点が端子だけなら、整数の無向距離の対称性を使って 1 行を走査する。
            if (is_integral_v<T> && vertices.size() == 1) {
                const auto* row = current.row;
                for (size_t i = 0; i < remaining.size(); ++i) {
                    auto& candidate = remaining[i];
                    T d = row[candidate.terminal];
                    if (d < candidate.distance) {
                        candidate.distance = d;
                        candidate.closest = current.terminal;
                    }
                    if (candidate.distance < nearest) { nearest = candidate.distance; selected = i; }
                }
            } else {
                for (size_t i = 0; i < remaining.size(); ++i) {
                    auto& candidate = remaining[i];
                    const auto* row = candidate.row;
                    T best = candidate.distance;
                    int closest = candidate.closest;
                    for (int v : vertices) if (row[v] < best) { best = row[v]; closest = v; }
                    candidate.distance = best;
                    candidate.closest = closest;
                    if (best < nearest) { nearest = best; selected = i; }
                }
            }
        }
        return {chosen_edges_, total, true};
    }
    vector<T> growth_dist_;
    vector<int> growth_parent_, growth_edges_;

    template<bool bounded = false>
    Result incremental_prim(const vector<int>& terminals, int root, T budget = T(0)) {
        auto& dist = growth_dist_;
        auto& parent = growth_parent_;
        auto& ids = growth_edges_;
        dist.assign(n_, inf_);
        parent.resize(n_);
        ids.clear();
        queue_.reset(n_, inf_);
        dist[root] = 0; tree_mark_[root] = generation_;
        queue_.decrease(root, T(0));
        int remaining = int(terminals.size()) - 1;
        while (remaining > 0) {
            T d = queue_.minimum();
            if (d >= inf_) break;
            if constexpr (bounded && is_integral_v<T>) {
                // 次に接続する端子までの距離は未処理頂点の最小距離以上。
                if (d >= budget) return {{}, T(0), false};
            }
            int v = queue_.pop_minimum(d, inf_);
            if (is_terminal(v) && !in_tree(v)) {
                if constexpr (bounded && is_integral_v<T>) budget -= d;
                // 経路上の頂点を距離 0 の始点として加え、探索を続ける
                for (int u = v; !in_tree(u);) {
                    int id = parent[u];
                    if (id < 0) throw logic_error("steiner: invalid incremental path");
                    ids.push_back(id); tree_mark_[u] = generation_;
                    if (is_terminal(u)) --remaining;
                    dist[u] = 0; queue_.decrease(u, T(0));
                    u = other(id, u);
                }
                continue;
            }
            for (const auto& arc : graph_[v]) {
                if (relax_distance(d, edges_[arc.edge].weight, dist[arc.to])) {
                    parent[arc.to] = arc.edge;
                    queue_.decrease(arc.to, dist[arc.to]);
                }
            }
        }
        if (remaining > 0) throw overflow_error("steiner: shortest path must be below INF");
        return finish(ids);
    }
    Result mehlhorn(const vector<int>& terminals) {
        const int k = int(terminals.size());
        vector<T> dist(n_, inf_);
        vector<int> owner(n_, -1), parent(n_, -1);
        queue_.reset(n_, inf_);
        for (int i = 0; i < k; ++i) {
            int t = terminals[i];
            dist[t] = T(0);
            owner[t] = i;
            queue_.decrease(t, T(0));
        }
        while (queue_.minimum() < inf_) {
            T d = queue_.minimum();
            int v = queue_.pop_minimum(d, inf_);
            for (const auto& arc : graph_[v]) {
                if (relax_distance(d, edges_[arc.edge].weight, dist[arc.to])) {
                    owner[arc.to] = owner[v];
                    parent[arc.to] = arc.edge;
                    queue_.decrease(arc.to, dist[arc.to]);
                }
            }
        }

        // 端子対の表が O(m) に収まる場合は、同じ領域対を結ぶ最小の候補だけを残す。
        vector<pair<T, int>> boundary;
        bool compact = uint64_t(k) * k <= edges_.size();
        vector<int> pair_slot(compact ? size_t(k) * k : 0, -1);
        if (compact) boundary.reserve(size_t(k) * (k - 1) / 2);
        else if (components_.group_count() == 1 && edges_.size() > 8ULL * n_) boundary.reserve(edges_.size());
        for (int id = 0; id < int(edges_.size()); ++id) {
            const Edge& edge = edges_[id];
            int a = owner[edge.u], b = owner[edge.v];
            if (a < 0 || b < 0 || a == b) continue;
            T cost;
            if constexpr (is_integral_v<T> && is_signed_v<T>) {
                using U = make_unsigned_t<T>;
                auto sum = U(dist[edge.u]) + U(dist[edge.v]);
                if (sum >= U(inf_ - edge.weight)) continue;
                cost = T(sum) + edge.weight;
            } else {
                cost = add_distance(add_distance(dist[edge.u], edge.weight), dist[edge.v]);
                if (cost >= inf_) continue;
            }
            if (compact) {
                if (a > b) swap(a, b);
                int& slot = pair_slot[size_t(a) * k + b];
                if (slot >= 0) {
                    if (cost < boundary[slot].first) boundary[slot] = {cost, id};
                    continue;
                }
                slot = int(boundary.size());
            }
            boundary.emplace_back(cost, id);
        }

        // 各領域が初めて接続先を持つ重みまでを先にソートする。
        // この範囲で全体がつながれば、残りの辺の順位は使わない。
        auto middle = boundary.end();
        if (boundary.size() > 6ULL * k) {
            vector<T> lightest(k, inf_);
            for (auto [cost, id] : boundary) {
                const Edge& edge = edges_[id];
                int a = owner[edge.u], b = owner[edge.v];
                lightest[a] = min(lightest[a], cost);
                lightest[b] = min(lightest[b], cost);
            }
            T cutoff = *max_element(lightest.begin(), lightest.end());
            middle = partition(boundary.begin(), boundary.end(),
                               [cutoff](const auto& candidate) { return candidate.first <= cutoff; });
        }
        sort(boundary.begin(), middle);
        UnionFind components(k);
        vector<int> ids;
        int remaining = k - 1;
        for (auto it = boundary.begin(); it != boundary.end(); ++it) {
            if (it == middle) sort(middle, boundary.end());
            int id = it->second;
            const Edge& edge = edges_[id];
            if (!components.unite(owner[edge.u], owner[edge.v])) continue;
            ids.push_back(id);
            // 領域内の葉は端子か、選んだ境界辺の端点。境界辺を加えると非端子の葉は残らない。
            for (int endpoint : {edge.u, edge.v}) {
                for (int v = endpoint; parent[v] >= 0 && !in_tree(v); v = other(parent[v], v)) {
                    tree_mark_[v] = generation_;
                    ids.push_back(parent[v]);
                }
            }
            if (--remaining == 0) break;
        }
        if (remaining > 0) throw overflow_error("steiner: terminal connection cost must be below INF");
        return finish(ids);
    }
    // 3 端子を結ぶ木の分岐点を全頂点から選ぶ。各頂点で 3 本の最短距離の和を比較する。
    Result exact_three(const vector<int>& terminals, bool cache_distances) {
        Cache local[3];
        const Cache* rows[3];
        for (int i = 0; i < 3; ++i) {
            int t = terminals[i];
            if (cache_distances || !cache_[t].dist.empty()) rows[i] = &distances(t);
            else {
                local[i] = make_distances(t);
                rows[i] = &local[i];
            }
        }
        T best = inf_;
        int center = -1;
        for (int v = 0; v < n_; ++v) {
            T sum = add_distance(add_distance(rows[0]->dist[v], rows[1]->dist[v]), rows[2]->dist[v]);
            if (sum < best) { best = sum; center = v; }
        }
        if (center < 0) throw overflow_error("steiner: tree cost must be below INF");

        // 端子 0 を根にする。合流した経路は、先に訪れたときの親を残す。
        vector<int> parent(n_, -1);
        const int root = terminals[0];
        for (int v = center; v != root;) {
            parent[v] = rows[0]->parent[v];
            v = parent[v];
        }
        for (int i = 1; i < 3; ++i) {
            for (int v = center; v != terminals[i];) {
                int previous = v;
                v = rows[i]->parent[v];
                if (v != root && parent[v] < 0) parent[v] = previous;
            }
        }
        tree_mark_[root] = generation_;
        Result result;
        result.total_cost = best;
        for (int i = 1; i < 3; ++i) {
            for (int v = terminals[i]; !in_tree(v); v = parent[v]) {
                tree_mark_[v] = generation_;
                result.edges.emplace_back(min(v, parent[v]), max(v, parent[v]));
            }
        }
        return result;
    }
    // 現在の解に含まれる頂点上で MST を作り直し、全端子を結ぶ部分木だけを残す
    void refine_induced_mst(const vector<int>& terminals, Result& best) {
        auto& key = growth_dist_;
        key.assign(n_, T(0));
        for (auto [u, v] : best.edges) key[u] = key[v] = inf_;
        auto& parent = growth_parent_;
        parent.assign(n_, -1);
        auto& order = growth_edges_;
        order.clear();
        order.reserve(best.edges.size() + 1);
        queue_.reset(n_, inf_);
        int root = terminals.front();
        key[root] = T(0);
        queue_.decrease(root, T(0));
        while (queue_.minimum() < inf_) {
            int v = queue_.pop_minimum(queue_.minimum(), inf_);
            order.push_back(v);
            // 誘導部分グラフの外と確定済みの頂点には、非負の辺で key を改善できない
            key[v] = T(0);
            for (const auto& arc : graph_[v]) {
                T weight = edges_[arc.edge].weight;
                if (weight < key[arc.to]) {
                    key[arc.to] = weight;
                    parent[arc.to] = arc.edge;
                    queue_.decrease(arc.to, weight);
                }
            }
        }
        vector<char> needed(n_, false);
        for (int t : terminals) needed[t] = true;
        T cost = T(0);
        vector<int> retained;
        retained.reserve(order.size() - 1);
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            int v = *it, id = parent[v];
            if (!needed[v] || id < 0) continue;
            cost = add_distance(cost, edges_[id].weight);
            if (cost >= best.total_cost) return;
            needed[other(id, v)] = true;
            retained.push_back(id);
        }
        best.edges.clear();
        for (int id : retained) best.edges.emplace_back(edges_[id].u, edges_[id].v);
        best.total_cost = cost;
    }
    static vector<pair<int, int>> require_feasible(Result result) {
        if (!result.feasible) throw runtime_error("steiner: terminals are disconnected");
        return move(result.edges);
    }
    static int checked_size(size_t count, T inf) {
        if (count > size_t(numeric_limits<int>::max())) throw invalid_argument("steiner: too many vertices");
        if (!finite(inf) || inf <= 0) throw invalid_argument("steiner: INF must be positive and finite");
        return int(count);
    }

public:
    MinimumSteinerTree() = default;
    /// @brief n 頂点、辺のないグラフを構築する / O(n)
    MinimumSteinerTree(size_t n, T inf)
        : n_(checked_size(n, inf)), inf_(inf), graph_(n_), components_(n_), cache_(n_),
          terminal_mark_(n_), tree_mark_(n_) {}
    /// @brief 隣接リスト graph から構築する / 重みは 0 以上 inf 未満 / 各辺は両方向に与え、両方向の最小重みを一致させる
    MinimumSteinerTree(const vector<vector<pair<int, T>>>& graph, T inf) : MinimumSteinerTree(graph.size(), inf) {
        // 各頂点に、大きい頂点から入る辺を始点順に集める。平行な逆向きの辺は連続する。
        vector<vector<pair<int, T>>> incoming(n_);
        for (int u = 0; u < n_; ++u) for (auto [v, weight] : graph[u]) {
            if (v < 0 || v >= n_ || !finite(weight) || weight < 0 || weight >= inf_)
                throw invalid_argument("steiner: invalid endpoint or weight");
            if (v < u) incoming[v].emplace_back(u, weight);
        }
        vector<T> forward(n_, inf_);
        for (int u = 0; u < n_; ++u) {
            int unmatched = 0;
            for (auto [v, weight] : graph[u]) if (v > u) {
                if (forward[v] == inf_) ++unmatched;
                forward[v] = min(forward[v], weight);
            }
            const auto& reverse = incoming[u];
            for (size_t i = 0; i < reverse.size();) {
                auto [v, weight] = reverse[i++];
                while (i < reverse.size() && reverse[i].first == v) {
                    weight = min(weight, reverse[i++].second);
                }
                if (weight != forward[v]) throw invalid_argument("steiner: graph must be undirected with symmetric minimum weights");
                forward[v] = inf_;
                --unmatched;
                if (edges_.size() >= size_t(numeric_limits<int>::max())) throw invalid_argument("steiner: too many edges");
                int id = int(edges_.size());
                edges_.push_back({u, v, weight});
                graph_[u].push_back({v, id}); graph_[v].push_back({u, id});
                forest_edge_.push_back(components_.group_count() > 1 && components_.unite(u, v));
            }
            if (unmatched) throw invalid_argument("steiner: graph must be undirected with symmetric minimum weights");
        }
    }

    /// @brief 無向辺を追加するか既存辺の重みを置き換える / 変化した場合に true / 自己ループは無視する
    bool set_edge(int u, int v, T weight) {
        check_vertex(u); check_vertex(v);
        if (!finite(weight) || weight < 0 || weight >= inf_) throw invalid_argument("steiner: invalid weight");
        if (u == v) return false;
        if (u > v) swap(u, v);
        auto it = arc_position(u, v);
        if (it != graph_[u].end() && it->to == v) {
            int id = it->edge;
            T old = edges_[id].weight;
            if (weight == old) return false;
            bool shortening = weight < old;
            record_distance_update(u, v, id, weight, shortening);
            // 木辺の短縮と非木辺の延長は、この最小全域森の最適性を保つ。
            if (mst_ready_) {
                bool tree_edge = in_cached_mst(u, v, id);
                if (shortening && !tree_edge) mst_ready_ = preserves_mst(u, v, weight);
                if (!shortening && tree_edge) mst_ready_ = false;
            }
            update_reduction(u, v, old, weight);
            edges_[id].weight = weight;
            if (!dense_weights_.empty()) set_dense_weight(u, v, weight);
            return true;
        }
        if (edges_.size() >= size_t(numeric_limits<int>::max())) throw length_error("steiner: too many edges");
        int id = int(edges_.size());
        auto pos = it - graph_[u].begin();
        reserve_one(graph_[u]); reserve_one(graph_[v]); reserve_one(edges_);
        reserve_one(forest_edge_);
        graph_[u].insert(graph_[u].begin() + pos, {v, id});
        graph_[v].insert(arc_position(v, u), {u, id});
        edges_.push_back({u, v, weight});
        if (mst_ready_) mst_ready_ = preserves_mst(u, v, weight);
        forest_edge_.push_back(!components_dirty_ && components_.unite(u, v));
        update_reduction(u, v, inf_, weight);
        record_distance_update(u, v, id, weight, true);
        if (!dense_weights_.empty()) set_dense_weight(u, v, weight);
        return true;
    }

    /// @brief 頂点対に対応する無向辺を削除する / 存在していた場合に true
    bool remove_edge(int u, int v) {
        check_vertex(u); check_vertex(v);
        if (u == v) return false;
        auto it = arc_position(u, v);
        if (it == graph_[u].end() || it->to != v) return false;
        int id = it->edge;
        update_reduction(u, v, edges_[id].weight, inf_);
        record_distance_update(u, v, id, inf_, false);
        if (in_cached_mst(u, v, id)) mst_ready_ = false;
        if (forest_edge_[id]) components_dirty_ = true;
        graph_[u].erase(it);
        graph_[v].erase(arc_position(v, u));
        // 末尾の辺を空いた番号に移し、その辺を使う親ポインタを付け替える。
        int last = int(edges_.size()) - 1;
        if (id != last) {
            const Edge& e = edges_[last];
            if (!reduced_.hub_distance.empty() && keep_for_distances(e.u, e.v, e.weight))
                mark_reduced_endpoints(e.u, e.v);
            for (int vtx : {e.u, e.v}) {
                arc_position(vtx, other(last, vtx))->edge = id;
                if (mst_ready_ && mst_parent_edge_[vtx] == last) mst_parent_edge_[vtx] = id;
                if constexpr (is_floating_point_v<T>) for (int s : cached_sources_) {
                    int& parent = cache_[s].parent[vtx];
                    if (parent == last) parent = id;
                }
            }
            edges_[id] = e;
            forest_edge_[id] = forest_edge_[last];
        }
        edges_.pop_back(); forest_edge_.pop_back();
        if (!dense_weights_.empty()) {
            if (8ULL * edges_.size() < uint64_t(n_) * (n_ - 1)) {
                vector<T>().swap(dense_weights_);
                dense_rows_seen_ = 0;
            }
            else set_dense_weight(u, v, inf_);
        }
        return true;
    }

    /// @brief terminals を結ぶ木を method で求める / 端点が非連結なら feasible = false
    Result solve(const vector<int>& terminals, SteinerTreeMethod method = SteinerTreeMethod::cached_prim) {
        const auto& unique = prepare(terminals);
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

    /// @brief すべての端点を根として cached_prim を試し、最良の木の辺を返す / 端点が非連結なら runtime_error
    vector<pair<int, int>> build_prim_fullsearch(const vector<int>& terminals) {
        const auto& unique = prepare(terminals);
        if (unique.size() <= 1) return {};
        if (!connected(unique)) throw runtime_error("steiner: terminals are disconnected");
        if (unique.size() == size_t(n_)) return mst_prune(unique).edges;
        Result best; best.total_cost = inf_;
        for (int root : unique) {
            // cached_prim は木への所属だけを参照する。
            next_generation();
            auto candidate = cached_prim(unique, root, best.total_cost);
            if (candidate.feasible && candidate.total_cost < best.total_cost) best = move(candidate);
            if (best.total_cost == 0) break;
        }
        return move(best.edges);
    }

    /// @brief mehlhorn の解と、最大 starts 個の根から growth で構築した解のうち最良のものを返す / growth には Prim 法の 2 方式のどちらかを指定する / 最初の根は terminals.back()
    /// 整数重みで starts > 0 かつ端子が 2、3 個なら厳密解を返す
    /// 複数の構築解を比較したときは、最良候補の頂点に誘導した部分グラフの MST を枝刈りして改善する
    Result solve_best(const vector<int>& terminals, int starts = 8, uint32_t seed = 0,
                      SteinerTreeMethod growth = SteinerTreeMethod::incremental_prim) {
        if (starts < 0) throw invalid_argument("steiner: starts must be nonnegative");
        if (growth != SteinerTreeMethod::cached_prim && growth != SteinerTreeMethod::incremental_prim)
            throw invalid_argument("steiner: growth method must be a Prim variant");
        const auto& unique = prepare(terminals);
        if (unique.size() <= 1) return {};
        if (!connected(unique)) return {{}, T(0), false};
        if (unique.size() == size_t(n_)) return mst_prune(unique);
        if constexpr (is_integral_v<T>) {
            if (starts > 0) {
                if (unique.size() == 2) {
                    int root = terminals.back();
                    return growth == SteinerTreeMethod::cached_prim ? cached_prim(unique, root, inf_)
                                                                    : incremental_prim(unique, root);
                }
                if (unique.size() == 3) return exact_three(unique, growth == SteinerTreeMethod::cached_prim);
            }
        }
        auto best = mehlhorn(unique);
        if (starts == 0 || best.total_cost == 0) return best;
        vector<int> roots = unique;
        swap(roots[0], *find(roots.begin(), roots.end(), terminals.back()));
        Random random(seed);
        for (int i = 0; i < min(starts, int(roots.size())); ++i) {
            if (i > 0) swap(roots[i], roots[random.randrange(i, roots.size())]);
            next_generation();
            for (int t : unique) terminal_mark_[t] = generation_;
            auto candidate = growth == SteinerTreeMethod::cached_prim ? cached_prim(unique, roots[i], best.total_cost) : incremental_prim<true>(unique, roots[i], best.total_cost);
            if (candidate.feasible && candidate.total_cost < best.total_cost) best = move(candidate);
            if (best.total_cost == 0) break;
        }
        if (unique.size() >= 3 && best.total_cost > T(0)) refine_induced_mst(unique, best);
        return best;
    }
    /// @brief 最短路表を作成済みの始点の数を返す
    int cached_source_count() const { return ready_sources_; }
    /// @brief 最短路表と最小全域森を破棄する
    void clear_cache() {
        reduced_ = {};
        for (int s : cached_sources_) cache_[s] = {};
        vector<int>().swap(cached_sources_);
        vector<T>().swap(dense_weights_);
        dense_rows_seen_ = 0;
        ready_sources_ = 0;
        mst_ready_ = false;
        vector<int>().swap(mst_parent_edge_); vector<int>().swap(mst_depth_);
    }
};
}
