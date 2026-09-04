/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/online_dynamic_connectivity.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
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

        void set_mark(int v, uint32_t mark, bool on) {
            splay(v, false);
            uint32_t mask = mark << SIZE_BITS;
            if (on) nodes[v].meta |= mask;
            else nodes[v].meta &= ~mask;
            pull(v);
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
            splay(u);
            splay(v);
            return nodes[u].p != 0;
        }

        bool same_from_root(int root, int v) {
            ++v;
            assert(nodes[root].p == 0);
            if (root == v) return true;
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

        bool cut(int slot) {
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
            bool u_small;
            if (in_b) {
                u_small = node_size(r) + node_size(a) <= node_size(l);
                merge(r, a);
            } else {
                u_small = node_size(r) <= node_size(b) + node_size(l);
                merge(b, l);
            }
            release_pair(slot);
            return u_small;
        }

        void set_adj(int v, bool on) {
            set_mark(v + 1, ADJ_MARK, on);
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

    bool erase_incidence(int id, int side, int lev) {
        Edge &e = edges[id];
        int v = endpoint(id, side), p = e.inc[side].prv, q = e.inc[side].nxt;
        if (p == -1) heads[lev][v] = q;
        else edges[p >> 1].inc[p & 1].nxt = q;
        if (q != -1) edges[q >> 1].inc[q & 1].prv = p;
        e.inc[side].prv = e.inc[side].nxt = -1;
        bool empty = heads[lev][v] == -1;
        if (empty) forests[lev].set_adj(v, false);
        return empty;
    }

    void insert_non_tree(int id, int lev) {
        edges[id].lev = to_level(lev);
        insert_incidence(id, 0, lev);
        insert_incidence(id, 1, lev);
    }

    int erase_non_tree(int id) {
        int lev = edges[id].lev;
        bool a = erase_incidence(id, 0, lev);
        bool b = erase_incidence(id, 1, lev);
        return b ? edges[id].v : a ? edges[id].u : -1;
    }

    int promote_non_tree(int id) {
        int lev = edges[id].lev;
        int root = erase_non_tree(id);
        insert_non_tree(id, lev + 1);
        return root;
    }

    void reconnect(int u, int v, int top, uint32_t u_small) {
        for (int lev = top; lev >= 0; --lev) {
            int s = (u_small >> lev & 1) ? u : v;
            int rep = forests[lev].vertex_node(s);
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
                    Edge &e = edges[id];
                    int y = endpoint(id, (code & 1) ^ 1);
                    if (forests[lev].same_from_root(root, y)) {
                        root = forests[lev].vertex_node(y);
                        if (!upper_ready) {
                            ensure_level(lev + 1);
                            upper_ready = true;
                        }
                        int next_root = promote_non_tree(id);
                        if (next_root != -1) root = forests[lev].vertex_node(next_root);
                        code = heads[lev][vertex];
                        continue;
                    }
                    erase_non_tree(id);
                    e.state = TREE_EDGE;
                    e.lev = to_level(lev);
                    e.copy = -1;
                    for (int i = 0; i <= lev; ++i) {
                        e.copy = forests[i].link(e.u, e.v, id, i == lev, e.copy);
                    }
                    --groups;
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
        ensure_level(0);
    }

public:
    OnlineDynamicConnectivity() : OnlineDynamicConnectivity(0, 0, true) {}

    // 頂点数 size の空グラフを作る / O(size)
    explicit OnlineDynamicConnectivity(int size) : OnlineDynamicConnectivity(size, 0, true) {}

    // 頂点数 size の空グラフを作り cap 本分を予約する / O(size)
    OnlineDynamicConnectivity(int size, int cap) : OnlineDynamicConnectivity(size, cap, true) {}

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
        } else if (groups != 1 && !forests[0].same(u, v)) {
            edges[id].state = TREE_EDGE;
            edges[id].copy = forests[0].link(u, v, id, true, -1);
            --groups;
        } else {
            edges[id].state = NON_TREE;
            insert_non_tree(id, 0);
        }
        return id;
    }

    // 辺 ID を削除する / 償却 O(log^2 n)
    // 削除済みまたは範囲外なら false を返す
    bool erase_edge(int id) {
        if (id < 0 || id >= issued || edges[id].state == DEAD) return false;
        Edge &e = edges[id];
        --active;
        if (e.state == LOOP) {
            e.state = DEAD;
            return true;
        }
        if (e.state == NON_TREE) {
            erase_non_tree(id);
            e.state = DEAD;
            return true;
        }
        int u = e.u, v = e.v, top = e.lev;
        int slot = e.copy;
        uint32_t u_small = 0;
        for (int lev = top; lev >= 0; --lev) {
            int down = forests[lev].lower_slot(slot);
            if (forests[lev].cut(slot)) u_small |= 1U << lev;
            slot = down;
        }
        assert(slot == -1);
        e.state = DEAD;
        e.copy = -1;
        ++groups;
        reconnect(u, v, top, u_small);
        return true;
    }

    // u と v が連結なら true を返す / 償却 O(log n)
    bool same(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v || groups == 1) return true;
        if (groups == n) return false;
        return forests[0].same(u, v);
    }

    // v の連結成分の頂点数を返す / 償却 O(log n)
    int size(int v) {
        assert(0 <= v && v < n);
        if (groups == 1) return n;
        if (groups == n) return 1;
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
