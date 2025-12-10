#include <algorithm>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>
#include <type_traits>

using namespace std;

namespace titan23 {

template<class T>
class SternBrocotTree {
public:
    struct Node {
        // 開区間 (p/q, r/s)
        // (p+r)/(q+s) がこのノードの値
        T p, q, r, s;

        /// @brief 分母
        T den() const {
            return q + s;
        }

        /// @brief 分子
        T num() const {
            return p + r;
        }

        /// @brief 左の子へd進む
        Node left(T d) const {
            return {p, q, r + p * d, s + q * d};
        }

        /// @brief 右の子へd進む
        Node right(T d) const {
            return {p + r * d, q + s * d, r, s};
        }

        /// @brief 開区間を返す
        pair<pair<T, T>, pair<T, T>> range() const {
            return {{p, q}, {r, s}};
        }

        friend bool operator<(const Node &a, const Node &b) { return a.num()*b.den() < b.num()*a.den(); }
        friend bool operator>(const Node &a, const Node &b) { return b < a; }
        friend bool operator<=(const Node &a, const Node &b) { return !(b < a); }
        friend bool operator>=(const Node &a, const Node &b) { return !(a < b); }
        friend bool operator==(const Node &a, const Node &b) { return a.num() == b.num() && a.den() == b.den(); }
        friend bool operator!=(const Node &a, const Node &b) { return !(a == b); }
        friend ostream& operator<<(ostream& os, const Node n) {
            os << n.num() << "/" << n.den();
            return os;
        }
    };

private:
    Node root() { return {0, 1, 1, 0}; }

    pair<char, T> get_parent_step(const Node &node) {
        T ls = node.p + node.q;
        T rs = node.r + node.s;
        if (ls == rs) return {'?', T(0)};
        if (ls < rs) {
            T k = rs / ls;
            if (rs % ls == 0) --k;
            return {'L', k};
        } else {
            T k = ls / rs;
            if (ls % rs == 0) --k;
            return {'R', k};
        }
    }

    constexpr T internal_gcd(T a, T b) {
        while (b != 0) {
            T temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

public:
    /// @brief p/qに対応するノードを返す
    Node get_node(T p, T q) {
        T g = internal_gcd(p, q);
        p /= g;
        q /= g;
        Node now = root();
        T tp = p, tq = q;
        while (tp != 1 || tq != 1) {
            if (tp == tq) break;
            if (tp > tq) {
                T k = tp / tq;
                if (tp % tq == 0) --k;
                now = now.right(k);
                tp -= tq * k;
            } else {
                T k = tq / tp;
                if (tq % tp == 0) --k;
                now = now.left(k);
                tq -= tp * k;
            }
        }
        return now;
    }

    vector<pair<char, T>> encode_path(const Node &a) {
        vector<pair<char, T>> path;
        Node now = a;
        while (1) {
            auto [dir, cnt] = get_parent_step(now);
            if (cnt == T(0)) break;
            path.emplace_back(dir, cnt);
            if (dir == 'L') {
                now.r -= now.p * cnt;
                now.s -= now.q * cnt;
            } else {
                now.p -= now.r * cnt;
                now.q -= now.s * cnt;
            }
        }
        reverse(path.begin(), path.end());
        return path;
    }

    Node decode_path(const vector<pair<char, T>> &P) {
        Node now = root();
        for (const auto &[d, c] : P) {
            now = (d == 'L') ? now.left(c) : now.right(c);
        }
        return now;
    }

    Node lca(const Node &a, const Node &b) {
        auto pathA = encode_path(a);
        auto pathB = encode_path(b);
        vector<pair<char, T>> res;
        int n = min(pathA.size(), pathB.size());
        for (int i = 0; i < n; ++i) {
            if (pathA[i].first != pathB[i].first) break;
            res.emplace_back(pathA[i].first, min(pathA[i].second, pathB[i].second));
            if (pathA[i].second != pathB[i].second) break;
        }
        return decode_path(res);
    }

    /// @brief nodeの祖先であって、深さがkのノードを返す
    pair<bool, Node> ancestor(const Node &node, T k) {
        auto path = encode_path(node);
        T dep = 0;
        for (auto &[_, c] : path) {
            dep += c;
        }
        if (k > dep) return {false, Node{}};
        Node res = root();
        T now = 0;
        for (const auto &[d, c] : path) {
            T need = k - now;
            if (need <= 0) break;
            if (need <= c) {
                res = d == 'L' ? res.left(need) : res.right(need);
                break;
            } else {
                res = d == 'L' ? res.left(c) : res.right(c);
                now += c;
            }
        }
        return {true, res};
    }

    /// @brief SBT上で単調性を持つ判定関数fの境界(true | false)を探索する
    /// @brief O(log d)
    /// @tparam F 判定関数 bool(T num, T den)
    /// @param f (0, INF) において(true, ..., true, false, ..., false)と変化する単調関数
    /// @param d 探索する分母の上限(含む)
    /// @return 境界の区間を持つノード
    /// node.p/node.q: fがtrueとなる最大の分数
    /// node.r/node.s: fがfalseとなる最小の分数
    template<class F>
    Node search(F f, T d) {
        Node now = root();
        while (1) {
            if (now.den() > d) break;
            bool toR = f(now.num(), now.den());
            T k = 1;
            while (1) {
                Node next = toR ? now.right(k*2) : now.left(k*2);
                if (next.den() > d) break;
                if (f(next.num(), next.den()) != toR) break;
                k *= 2;
            }
            Node ok1 = toR ? now.right(1) : now.left(1);
            if (ok1.den() > d) {
                if (toR) now = ok1;
                break;
            }
            if (f(ok1.num(), ok1.den()) != toR) {
                now = ok1;
                continue;
            }
            T ok = k, ng = k*2;
            while (ng - ok > 1) {
                T mid = ok + (ng-ok)/2;
                Node node = toR ? now.right(mid) : now.left(mid);
                if (node.den() <= d && f(node.num(), node.den()) == toR) {
                    ok = mid;
                } else {
                    ng = mid;
                }
            }
            now = toR ? now.right(ok) : now.left(ok);
        }
        return now;
    }
};
} // namespace titan23
