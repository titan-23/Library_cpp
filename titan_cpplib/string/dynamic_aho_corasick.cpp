#pragma once
#include <vector>
#include <string>
#include "titan_cpplib/string/aho_corasick.cpp"
using namespace std;

namespace titan23 {
class DynamicAhoCorasick {
private:
    char B;

    struct Block {
        bool empty;
        vector<string> S;
        titan23::AhoCorasick aho;
        Block() : empty(true) {}
        Block(char B, const string &s) : empty(false), aho(B) {
            S.push_back(s);
            aho.add_string(s);
            aho.build();
        }
    };

    vector<Block> pos, neg;

    void add_block(const string &p, vector<Block> &a) {
        Block b(B, p);
        for (int i = 0; i < (int)a.size(); ++i) {
            if (a[i].empty) {
                a[i] = b;
                return;
            }
            Block nxt;
            nxt.empty = false;
            nxt.aho = titan23::AhoCorasick(B);
            nxt.S.insert(nxt.S.end(), make_move_iterator(b.S.begin()), make_move_iterator(b.S.end()));
            nxt.S.insert(nxt.S.end(), make_move_iterator(a[i].S.begin()), make_move_iterator(a[i].S.end()));
            for (const string &q : nxt.S) {
                nxt.aho.add_string(q);
            }
            nxt.aho.build();
            b = nxt;
            a[i].empty = true;
            a[i].S.clear();
        }
        a.push_back(b);
    }

public:
    DynamicAhoCorasick() {}
    DynamicAhoCorasick(char B) : B(B) {}

    /// @brief パターン文字列 p を追加する
    void add_string(const string &p) {
        add_block(p, pos);
    }

    /// @brief パターン文字列 p を削除する / p はすでに存在している必要がある
    void delete_string(const string &p) {
        add_block(p, neg);
    }

    void search(const string &s) const {
        for (const Block &b : pos) {
            // TODO b.ahoを使ってクエリに答える
        }
        for (const Block &b : neg) {
            // TODO b.ahoを使ってクエリに答える
        }
    }
};
} // namespace titan23
