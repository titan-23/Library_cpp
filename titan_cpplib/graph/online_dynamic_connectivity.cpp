/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/online_dynamic_connectivity.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
#include <utility>
using namespace std;

namespace titan23 {

// Holm-de Lichtenberg-Thorup の fully dynamic connectivity
// add_edge が返す辺 ID ごとに多重辺を区別する
class OnlineDynamicConnectivity {
private:
    static constexpr int MAX_LEVELS = 24;
    static constexpr uint8_t DEAD = 0, NON_TREE = 1, TREE_EDGE = 2, LOOP = 3;

    struct Incidence { int nxt, prv; };

    struct Edge {
        int u, v;
        uint8_t lev, state;
        union {
            int copy;
            Incidence inc[2];
        };
    };

    class Forest {
    private:
        static constexpr uint32_t TREE_MARK = 1, ADJ_MARK = 2;
        static constexpr int SIZE_BITS = 24;
        static constexpr uint32_t SIZE_MASK = (1U << SIZE_BITS) - 1;
        static constexpr uint32_t SELF_MASK = 3U << SIZE_BITS;
        static constexpr uint32_t ALL_MASK = SELF_MASK << 2;
        static_assert(((SELF_MASK | ALL_MASK) & SIZE_MASK) == 0);

        struct Node { int l = 0, r = 0, p = 0; uint32_t meta = 1; };

        int base, pair_count, free_head;
        vector<Node> nodes;
        vector<int> owner, lower;

        int node_size(int v) const {
            return nodes[v].meta & SIZE_MASK;
        }

        static Node make_node(uint32_t self = 0) {
            return {0, 0, 0, 1U | self << SIZE_BITS | self << (SIZE_BITS + 2)};
        }

        void pull(int v) {
            Node &x = nodes[v];
            uint32_t lm = nodes[x.l].meta;
            uint32_t rm = nodes[x.r].meta;
            uint32_t self = x.meta & SELF_MASK;
            uint32_t all = ((lm | rm) & ALL_MASK) | self << 2;
            // mark は 2^SIZE_BITS の倍数なので加算後に mask すると size の和だけが残る
            x.meta = ((lm + rm + 1) & SIZE_MASK) | self | all;
        }

        void rotate_root(int v, int p) {
            Node &x = nodes[v];
            Node &y = nodes[p];
            bool d = y.r == v;
            int c = d ? x.l : x.r;
            x.p = 0;
            if (d) {
                y.r = c;
                x.l = p;
            } else {
                y.l = c;
                x.r = p;
            }
            nodes[c].p = p;
            y.p = v;
            pull(p);
        }

        int rotate2(int v, int p, int g) {
            Node &x = nodes[v];
            Node &y = nodes[p];
            Node &z = nodes[g];
            int up = z.p;
            bool a = y.r == v, b = z.r == p;
            (nodes[up].r == g ? nodes[up].r : nodes[up].l) = v;
            x.p = up;
            if (a == b) {
                if (a) {
                    int c = y.l, d = x.l;
                    z.r = c;
                    nodes[c].p = g;
                    y.r = d;
                    nodes[d].p = p;
                    x.l = p;
                    y.p = v;
                    y.l = g;
                    z.p = p;
                } else {
                    int c = x.r, d = y.r;
                    y.l = c;
                    nodes[c].p = p;
                    z.l = d;
                    nodes[d].p = g;
                    x.r = p;
                    y.p = v;
                    y.r = g;
                    z.p = p;
                }
                pull(g);
                pull(p);
            } else if (a) {
                int c = x.l, d = x.r;
                y.r = c;
                nodes[c].p = p;
                z.l = d;
                nodes[d].p = g;
                x.l = p;
                y.p = v;
                x.r = g;
                z.p = v;
                pull(p);
                pull(g);
            } else {
                int c = x.l, d = x.r;
                z.r = c;
                nodes[c].p = g;
                y.l = d;
                nodes[d].p = p;
                x.l = g;
                z.p = v;
                x.r = p;
                y.p = v;
                pull(g);
                pull(p);
            }
            return up;
        }

#if defined(__GNUC__) && !defined(__clang__) && defined(__OPTIMIZE__)
        [[gnu::optimize("O3")]]
#endif
        void splay(int v, bool finish = true) {
            int p = nodes[v].p;
            while (p) {
                int g = nodes[p].p;
                if (g) p = rotate2(v, p, g);
                else {
                    rotate_root(v, p);
                    p = 0;
                }
            }
            if (finish) pull(v);
        }

        // Build a balanced sequence; recursion depth is O(log n), even for a path.
        int build_tour(const vector<int>& tour, int l, int r, int parent) {
            if (l == r) return 0;
            int mid = (l + r) / 2, v = tour[mid];
            nodes[v].p = parent;
            nodes[v].l = build_tour(tour, l, mid, v);
            nodes[v].r = build_tour(tour, mid + 1, r, v);
            pull(v);
            return v;
        }

        int rightmost(int v) {
            assert(nodes[v].p == 0);
            while (nodes[v].r) v = nodes[v].r;
            splay(v, false);
            return v;
        }

        int split_right(int v) {
            splay(v, false);
            int l = nodes[v].l;
            nodes[v].l = 0;
            nodes[l].p = 0;
            pull(v);
            return l;
        }

        int merge(int l, int r) {
            if (!l) return r;
            if (!r) return l;
            l = rightmost(l);
            assert(nodes[r].p == 0);
            nodes[l].r = r;
            nodes[r].p = l;
            pull(l);
            return l;
        }

        int reroot(int v) {
            int l = split_right(v);
            if (!l) return v;
            int r = rightmost(v);
            nodes[r].r = l;
            nodes[l].p = r;
            pull(r);
            return r;
        }

        int alloc_pair(int id, int down, uint32_t self) {
            int slot;
            if (free_head == -1) {
                slot = pair_count++;
                owner.push_back(id);
                lower.push_back(down);
                nodes.push_back(make_node(self));
                nodes.push_back(make_node());
            } else {
                slot = free_head;
                free_head = owner[slot];
                owner[slot] = id;
                lower[slot] = down;
                int uv = base + slot * 2;
                nodes[uv] = make_node(self);
                nodes[uv + 1] = make_node();
            }
            return slot;
        }

        void release_pair(int slot) {
            owner[slot] = free_head;
            free_head = slot;
        }

        // Return the new root if a splay occurred, or zero if the root is unchanged.
        // Stop propagation when the subtree aggregate is unchanged. A bounded
        // walk followed by splay retains the amortized logarithmic bound.
        int set_mark(int v, uint32_t mark, bool on) {
            uint32_t mask = mark << SIZE_BITS;
            if (on) nodes[v].meta |= mask;
            else nodes[v].meta &= ~mask;
            for (int step = 0; step < 4; ++step) {
                uint32_t before = nodes[v].meta & ALL_MASK;
                pull(v);
                if ((nodes[v].meta & ALL_MASK) == before || nodes[v].p == 0) return 0;
                v = nodes[v].p;
            }
            splay(v);
            return v;
        }

    public:
        static constexpr uint32_t TREE = TREE_MARK, ADJ = ADJ_MARK;

        Forest(int size, int cap) : base(size + 1), pair_count(0), free_head(-1) {
            int r = min(size, cap);
            nodes.reserve(base + r * 2);
            nodes.resize(base);
            nodes[0].meta = 0;
            owner.reserve(r);
            lower.reserve(r);
        }

        // All vertices are still isolated here. Incidence flags are already set.
        // Enumerate each tree iteratively, then build its Euler tour in linear time.
        void build_initial(vector<Edge>& edges) {
            assert(pair_count == 0);
            int n = base - 1;
            vector<int> first(n, -1), next, tour;
            vector<pair<int, int>> stack;
            next.reserve(2 * n);
            tour.reserve(3 * n);
            for (int id = 0; id < (int)edges.size(); ++id) {
                Edge &e = edges[id];
                if (e.state != TREE_EDGE) continue;
                int slot = alloc_pair(id, -1, TREE_MARK);
                e.copy = slot;
                next.push_back(first[e.u]);
                next.push_back(first[e.v]);
                first[e.u] = 2 * slot;
                first[e.v] = 2 * slot + 1;
            }
            for (int root = 0; root < n; ++root) {
                if (first[root] == -2) continue;
                tour.clear();
                tour.push_back(root + 1);
                stack.emplace_back(root, -1);
                while (!stack.empty()) {
                    auto [v, parent_arc] = stack.back();
                    int arc = first[v];
                    if (arc == -1) {
                        first[v] = -2;
                        stack.pop_back();
                        if (parent_arc != -1) tour.push_back(base + (parent_arc ^ 1));
                        continue;
                    }
                    first[v] = next[arc];
                    if (parent_arc != -1 && arc == (parent_arc ^ 1)) continue;
                    const Edge &e = edges[owner[arc >> 1]];
                    int w = e.u ^ e.v ^ v;
                    tour.push_back(base + arc);
                    tour.push_back(w + 1);
                    stack.emplace_back(w, arc);
                }
                build_tour(tour, 0, tour.size(), 0);
            }
        }

        void reserve_edges(int cap) {
            int r = min(base - 1, cap);
            nodes.reserve(base + r * 2);
            owner.reserve(r);
            lower.reserve(r);
        }

        bool same(int u, int v) {
            if (u == v) return true;
            ++u;
            ++v;
            // A singleton Euler tour is an isolated vertex in this forest.
            // Avoid accessing the other, potentially much larger, tour.
            if ((nodes[u].l | nodes[u].r | nodes[u].p) == 0 ||
                (nodes[v].l | nodes[v].r | nodes[v].p) == 0) return false;
            splay(u);
            splay(v);
            return nodes[u].p != 0;
        }

        bool same_from_root(int root, int v) {
            ++v;
            assert(nodes[root].p == 0);
            if (root == v) return true;
            if ((nodes[root].l | nodes[root].r) == 0 ||
                (nodes[v].l | nodes[v].r | nodes[v].p) == 0) return false;
            splay(v);
            return nodes[root].p != 0;
        }

        int size(int v) {
            ++v;
            splay(v);
            return (node_size(v) + 2) / 3;
        }

        int link(int u, int v, int id, bool exact, int down) {
            ++u;
            ++v;
            int slot = alloc_pair(id, down, exact ? TREE_MARK : 0);
            int uv = base + slot * 2, vu = uv + 1;
            int a = reroot(u), b = reroot(v);
            nodes[uv].l = a;
            nodes[a].p = uv;
            nodes[uv].r = vu;
            nodes[vu].p = uv;
            nodes[vu].l = b;
            nodes[b].p = vu;
            pull(vu);
            pull(uv);
            return slot;
        }

        // Return the root of the smaller component, which reconnect can reuse.
        int cut(int slot) {
            int uv = base + slot * 2, vu = uv + 1;
            splay(uv, false);
            int a = nodes[uv].l, b = nodes[uv].r;
            nodes[a].p = 0;
            nodes[b].p = 0;
            splay(vu, false);
            bool in_b = b && (b == vu || nodes[b].p != 0);
            assert((a && (a == vu || nodes[a].p != 0)) != in_b);
            int l = nodes[vu].l, r = nodes[vu].r;
            nodes[l].p = 0;
            nodes[r].p = 0;
            int small;
            if (in_b) {
                bool u_small = node_size(r) + node_size(a) <= node_size(l);
                int joined = merge(r, a);
                small = u_small ? joined : l;
            } else {
                bool u_small = node_size(r) <= node_size(b) + node_size(l);
                int joined = merge(b, l);
                small = u_small ? r : joined;
            }
            release_pair(slot);
            return small;
        }

        int set_adj(int v, bool on) {
            return set_mark(v + 1, ADJ_MARK, on);
        }

        void clear_tree(int node) {
            assert(nodes[node].p == 0);
            nodes[node].meta &= ~(TREE_MARK << SIZE_BITS);
            pull(node);
        }

        int find_mark(int v, uint32_t mark, bool finish = true) {
            if (nodes[v].p) splay(v);
            int x = v;
            uint32_t self_mask = mark << SIZE_BITS;
            uint32_t all_mask = mark << (SIZE_BITS + 2);
            if (!(nodes[x].meta & all_mask)) return -1;
            while (true) {
                int l = nodes[x].l;
                if (nodes[l].meta & all_mask) {
                    x = l;
                    continue;
                }
                if (nodes[x].meta & self_mask) {
                    splay(x, finish);
                    return x;
                }
                x = nodes[x].r;
            }
        }

        int edge_id(int node) const {
            return owner[(node - base) >> 1];
        }

        int vertex_node(int v) const { return v + 1; }

        int vertex_id(int node) const { return node - 1; }

        int lower_slot(int slot) const {
            return lower[slot];
        }
    };

    int n, groups, active, issued, level_count, reserve_cap;
    vector<Edge> edges;
    vector<Forest> forests;
    vector<vector<int>> heads;
    // Exact component cache until the first actual split. Non-tree deletions
    // and successful replacement edges leave this partition unchanged.
    vector<int> partition_parent;
    bool partition_cache_valid = true;

    int partition_root(int v) {
        while (partition_parent[v] >= 0) {
            int p = partition_parent[v];
            if (partition_parent[p] >= 0) partition_parent[v] = partition_parent[p];
            v = p;
        }
        return v;
    }

    // Called only on insertion. Update the cached partition when available.
    bool merge_partition(int u, int v) {
        if (groups == 1) return false;
        if (!partition_cache_valid) return !forests[0].same(u, v);
        u = partition_root(u); v = partition_root(v);
        if (u == v) return false;
        if (partition_parent[u] > partition_parent[v]) swap(u, v);
        partition_parent[u] += partition_parent[v];
        partition_parent[v] = u;
        return true;
    }

    static uint8_t to_level(int lev) {
        assert(0 <= lev && lev < 256);
        return static_cast<uint8_t>(lev);
    }

    void ensure_level(int lev) {
        assert(lev < level_count);
        while ((int)forests.size() <= lev) {
            forests.emplace_back(n, max(reserve_cap, active));
            heads.emplace_back(n, -1);
        }
    }

    int endpoint(int id, int side) const {
        return side ? edges[id].v : edges[id].u;
    }

    void insert_incidence(int id, int side, int lev) {
        Edge &e = edges[id];
        int v = endpoint(id, side), code = id * 2 + side;
        int h = heads[lev][v];
        e.inc[side].prv = -1;
        e.inc[side].nxt = h;
        if (h != -1) edges[h >> 1].inc[h & 1].prv = code;
        heads[lev][v] = code;
        if (h == -1) forests[lev].set_adj(v, true);
    }

    // Return the new forest root only if updating the flag moved it.
    int erase_incidence(int id, int side, int lev) {
        Edge &e = edges[id];
        int v = endpoint(id, side), p = e.inc[side].prv, q = e.inc[side].nxt;
        if (p == -1) heads[lev][v] = q;
        else edges[p >> 1].inc[p & 1].nxt = q;
        if (q != -1) edges[q >> 1].inc[q & 1].prv = p;
        e.inc[side].prv = e.inc[side].nxt = -1;
        if (heads[lev][v] == -1) return forests[lev].set_adj(v, false);
        return 0;
    }

    void insert_non_tree(int id, int lev) {
        edges[id].lev = to_level(lev);
        insert_incidence(id, 0, lev);
        insert_incidence(id, 1, lev);
    }

    int erase_non_tree(int id) {
        int lev = edges[id].lev;
        int a = erase_incidence(id, 0, lev);
        int b = erase_incidence(id, 1, lev);
        return b ? b : a;
    }

    int promote_non_tree(int id) {
        int lev = edges[id].lev;
        int root = erase_non_tree(id);
        insert_non_tree(id, lev + 1);
        return root;
    }

    void use_replacement(int id, int lev) {
        erase_non_tree(id);
        Edge &e = edges[id];
        e.state = TREE_EDGE;
        e.lev = to_level(lev);
        e.copy = -1;
        for (int i = 0; i <= lev; ++i) {
            e.copy = forests[i].link(e.u, e.v, id, i == lev, e.copy);
        }
        --groups;
    }

    void reconnect(int top, const int* small_roots) {
        for (int lev = top; lev >= 0; --lev) {
            int rep = small_roots[lev];
            // Promotion is only needed before moving internal non-tree edges
            // to the next level. With no incidence there is nothing to search;
            // leaving tree levels unchanged preserves all size invariants.
            int adjacent = forests[lev].find_mark(rep, Forest::ADJ);
            if (adjacent == -1) continue;

            // Try one candidate before promoting the smaller tree. A crossing
            // edge can reconnect immediately. Limiting this probe to one keeps
            // its extra cost O(log n) per level, even on adversarial inputs.
            int probe_code = heads[lev][forests[lev].vertex_id(adjacent)];
            int candidate = probe_code >> 1;
            int other_vertex = endpoint(candidate, (probe_code & 1) ^ 1);
            if (!forests[lev].same_from_root(adjacent, other_vertex)) {
                use_replacement(candidate, lev);
                return;
            }
            rep = forests[lev].vertex_node(other_vertex);
            bool upper_ready = (int)forests.size() > lev + 1;
            while (true) {
                int node = forests[lev].find_mark(rep, Forest::TREE, false);
                if (node == -1) break;
                int id = forests[lev].edge_id(node);
                forests[lev].clear_tree(node);
                rep = node;
                edges[id].lev = to_level(lev + 1);
                if (!upper_ready) {
                    ensure_level(lev + 1);
                    upper_ready = true;
                }
                Edge &e = edges[id];
                e.copy = forests[lev + 1].link(e.u, e.v, id, true, e.copy);
            }
            while (true) {
                int x = forests[lev].find_mark(rep, Forest::ADJ);
                if (x == -1) break;
                rep = x;
                int vertex = forests[lev].vertex_id(x);
                int code = heads[lev][vertex];
                int root = x;
                while (code != -1) {
                    int id = code >> 1;
                    int y = endpoint(id, (code & 1) ^ 1);
                    if (forests[lev].same_from_root(root, y)) {
                        root = forests[lev].vertex_node(y);
                        if (!upper_ready) {
                            ensure_level(lev + 1);
                            upper_ready = true;
                        }
                        int next_root = promote_non_tree(id);
                        if (next_root) root = next_root;
                        code = heads[lev][vertex];
                        continue;
                    }
                    use_replacement(id, lev);
                    return;
                }
                rep = root;
            }
        }
    }

    OnlineDynamicConnectivity(int size, int cap, bool) :
            n(size), groups(size), active(0), issued(0), level_count(1), reserve_cap(cap) {
        assert(size >= 0 && cap >= 0);
        // Euler tour の一成分は最大 3 * size - 2 ノード
        assert(3LL * size - 2 <= (1U << 24) - 1LL);
        for (int x = size; x > 1; x >>= 1) ++level_count;
        assert(level_count <= MAX_LEVELS);
        edges.reserve(reserve_cap);
        forests.reserve(level_count);
        heads.reserve(level_count);
        partition_parent.assign(n, -1);
    }

public:
    OnlineDynamicConnectivity() : OnlineDynamicConnectivity(0, 0, true) {}

    // 頂点数 size の空グラフを作る / O(size)、動的木は必要時に構築
    explicit OnlineDynamicConnectivity(int size) : OnlineDynamicConnectivity(size, 0, true) {}

    // 頂点数 size の空グラフを作り cap 本分を予約する / O(size)
    OnlineDynamicConnectivity(int size, int cap) : OnlineDynamicConnectivity(size, cap, true) {}

    // 動的木を一括構築する / 初回 O(n + 発行済みID数)、以後 O(1)
    // 木の辺の初回削除時にも自動実行する。削除時の初期化を避けたい場合に呼ぶ。
    void prepare_dynamic() {
        if (!forests.empty()) return;
        ensure_level(0);
        for (int id = 0; id < issued; ++id)
            if (edges[id].state == NON_TREE) insert_non_tree(id, 0);
        forests[0].build_initial(edges);
    }

    // 発行する辺 ID の個数を cap まで予約する
    void reserve_edges(int cap) {
        assert(cap >= issued);
        reserve_cap = max(reserve_cap, cap);
        edges.reserve(reserve_cap);
        for (Forest &f : forests) f.reserve_edges(reserve_cap);
    }

    // 辺を追加して新しい辺 ID を返す / 償却 O(log n)
    int add_edge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        int id = issued++;
        Edge edge;
        edge.u = u;
        edge.v = v;
        edge.lev = 0;
        edge.state = DEAD;
        edge.copy = -1;
        edges.push_back(edge);
        ++active;
        if (u == v) {
            edges[id].state = LOOP;
        } else if (merge_partition(u, v)) {
            edges[id].state = TREE_EDGE;
            if (!forests.empty()) edges[id].copy = forests[0].link(u, v, id, true, -1);
            --groups;
        } else {
            edges[id].state = NON_TREE;
            if (!forests.empty()) insert_non_tree(id, 0);
        }
        return id;
    }

    // 辺 ID を削除する / 更新列全体での探索費用は償却 O(log^2 n)
    // 未準備で木の辺を削除する場合は prepare_dynamic() の構築費用もかかる。
    // 新しい階層の配列初期化 O(n) は別。単一操作の最悪時間の保証ではない。
    // 削除済みまたは範囲外なら false を返す
    bool erase_edge(int id) {
        if (id < 0 || id >= issued || edges[id].state == DEAD) return false;
        if (forests.empty() && edges[id].state == TREE_EDGE) prepare_dynamic();
        Edge &e = edges[id];
        --active;
        if (e.state == LOOP) {
            e.state = DEAD;
            return true;
        }
        if (e.state == NON_TREE) {
            if (!forests.empty()) erase_non_tree(id);
            e.state = DEAD;
            return true;
        }
        int top = e.lev;
        int slot = e.copy;
        int small_roots[MAX_LEVELS];
        for (int lev = top; lev >= 0; --lev) {
            int down = forests[lev].lower_slot(slot);
            small_roots[lev] = forests[lev].cut(slot);
            slot = down;
        }
        assert(slot == -1);
        e.state = DEAD;
        e.copy = -1;
        int old_groups = groups;
        ++groups;
        reconnect(top, small_roots);
        if (groups != old_groups) partition_cache_valid = false;
        return true;
    }

    // u と v が連結なら true を返す / 償却 O(log n)
    bool same(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v || groups == 1) return true;
        if (groups == n) return false;
        if (partition_cache_valid) return partition_root(u) == partition_root(v);
        return forests[0].same(u, v);
    }

    // v の連結成分の頂点数を返す / 償却 O(log n)
    int size(int v) {
        assert(0 <= v && v < n);
        if (groups == 1) return n;
        if (groups == n) return 1;
        if (partition_cache_valid) return -partition_parent[partition_root(v)];
        return forests[0].size(v);
    }

    // 連結成分数を返す / O(1)
    int group_count() const {
        return groups;
    }

    // 有効な辺数を返す / O(1)
    int edge_count() const {
        return active;
    }

    // 発行済みの辺 ID 数を返す / O(1)
    int edge_id_count() const {
        return issued;
    }

    // 辺 ID が有効なら true を返す / O(1)
    bool edge_active(int id) const {
        return 0 <= id && id < issued && edges[id].state != DEAD;
    }
};

} // namespace titan23
