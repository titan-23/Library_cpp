#include "titan_cpplib/ds/index_set.cpp"

using namespace std;

namespace titan23 {
class UsedSet {
public:
    titan23::IndexSet used, unused;
    int u;

    UsedSet() {}
    UsedSet(int u) : u(u), used(u), unused(u) {
        for (int i = 0; i < u; ++i) {
            unused.add(i);
        }
    }

    void use(int v) {
        used.add(v);
        unused.remove(v);
    }

    void unuse(int v) {
        used.remove(v);
        unused.add(v);
    }

    void all_use() {
        if (unused.empty()) return;
        for (int v : unused.que) {
            used.add(v);
            unused.pos[v] = -1;
        }
        unused.que.clear();
    }

    void all_unuse() {
        if (used.empty()) return;
        for (int v : used.que) {
            unused.add(v);
            used.pos[v] = -1;
        }
        used.que.clear();
    }

    int get_use(int i) { return used.get(i); }
    int get_unuse(int i) { return unused.get(i); }
    bool contains_use(int v) { return used.contains(v); }
    bool contains_unuse(int v) { return unused.contains(v); }
    bool empty_use() const { return used.empty(); }
    bool empty_unuse() const { return unused.empty(); }
    int len_use() const { return used.len(); }
    int len_unuse() const { return unused.len(); }

    friend ostream& operator<<(ostream& os, const titan23::UsedSet &ust) {
        os << "used : " << ust.used << " / " << ust.unused;
        return os;
    }
};
} // namespace titan23
