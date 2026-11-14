/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/string/dynamic_aho_corasick.cpp
#pragma once
#include <vector>
#include <string>
#include "titan_cpplib/string/aho_corasick.cpp"
using namespace std;

namespace titan23 {
/// @brief パターンの追加と削除ができる Aho-Corasick 法 / 文字は B から 26 種類
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

    // 二進数の繰り上がりと同じ要領で、同じ大きさのブロックを併合する
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
            a[i].aho = titan23::AhoCorasick(B);
        }
        a.push_back(b);
    }

public:
    DynamicAhoCorasick() {}

    /// @brief 文字 B を先頭の文字として、空のパターン集合を作る
    DynamicAhoCorasick(char B) : B(B) {}

    /// @brief パターン文字列 p を追加する
    void add_string(const string &p) {
        add_block(p, pos);
    }

    /// @brief パターン文字列 p を削除する / p はすでに存在している必要がある
    void delete_string(const string &p) {
        add_block(p, neg);
    }

    /// @brief 文字列 s に含まれるパターンの出現回数の総和を返す
    long long search(const string &s) const {
        long long ans = 0;
        for (const Block &b : pos) if (!b.empty) {
            int now = b.aho.root;
            for (char c : s) {
                now = b.aho.next(now, c);
                ans += b.aho.node[now].cnt;
            }
        }
        for (const Block &b : neg) if (!b.empty) {
            // 削除したパターンの出現回数を引く
            int now = b.aho.root;
            for (char c : s) {
                now = b.aho.next(now, c);
                ans -= b.aho.node[now].cnt;
            }
        }
        return ans;
    }
};
} // namespace titan23
