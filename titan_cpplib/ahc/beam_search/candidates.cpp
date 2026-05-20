#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"

namespace flying_squirrel {

template<typename ScoreType, class Action>
struct BeamCandidate {
    int parent_leaf;
    ScoreType score;
    Action action;
    int node_id = -1; // record_history 用
};

template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class Candidates {
private:
    using T = pair<ScoreType, int>;
    vector<HashType> hashidx;
    titan23::HashDict<int> func;
    int beam_width, entry;
    int s = 1;
    vector<T> seg;
    bool is_built = false;

    void set(int k, T v) {
        k += s;
        seg[k] = v;
        while (k > 1) {
            k >>= 1;
            T nv = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
            if (nv == seg[k]) break;
            seg[k] = nv;
        }
    }

    void build_segtree() {
        for (int i = 0; i < entry; ++i) {
            seg[i + s] = {next_beam[i].score, i};
        }
        for (int k = s - 1; k > 0; --k) {
            seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
        }
    }

public:
    vector<BeamCandidate<ScoreType, Action>> next_beam;

    Candidates() {}

    int size() const { return entry; }

    ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

    //! @param node_id record_history 用。比較・選択に使わないので探索挙動には影響しない
    bool push(
        ScoreType score, HashType hash,
        int parent_leaf, Action action, int node_id = -1
    ) {
        if (is_built && score >= seg[1].first) {
            return false;
        }
        auto dat = func.get_pos(hash);
        int idx = func.inner_get(dat, -1);
        if (idx == -2) {
            return false;
        }
        if (idx != -1) {
            if (score < next_beam[idx].score) {
                next_beam[idx] = {parent_leaf, score, move(action), node_id};
                if (is_built) {
                    set(idx, {score, idx});
                }
                return true;
            }
            return false;
        }
        if (entry < beam_width) {
            func.inner_set(dat, hash, entry);
            next_beam[entry] = {parent_leaf, score, move(action), node_id};
            hashidx[entry] = hash;
            entry++;
            if (entry == beam_width) {
                build_segtree();
                is_built = true;
            }
            return true;
        }
        auto [_, i] = seg[1];
        next_beam[i] = {parent_leaf, score, move(action), node_id};
        func.set(hashidx[i], -1);
        func.inner_set(dat, hash, i);
        hashidx[i] = hash;
        set(i, {score, i});
        return true;
    }

    //! @param hash_window_turns clear_hash=false のとき、K ターンに 1 回 hash dict を全 clear。
    //!                          0 なら従来通り無制限蓄積。
    void reset(int turn, int w, bool clear_hash, int hash_window_turns = 0) {
        beam_width = w;
        while (s < w) {
            s <<= 1;
        }
        if (seg.size() < 2*s) {
            seg.resize(2*s);
        }
        fill(seg.begin(), seg.begin()+(2*s), make_pair(-INF, -1));
        if (hashidx.size() < w) {
            hashidx.resize(w);
            next_beam.resize(w);
        }
        if (clear_hash) {
            func.clear();
        } else {
            // 周期 clear: K ターンに 1 回「古い -2 マーカーを全部捨てる」だけ。
            // 直後に今ターンの survivor だけ -2 を付け直すので、窓は常に
            // 「直近 1〜K ターン分の survivor のみ」になる。
            bool periodic_clear = hash_window_turns > 0
                                  && (turn % hash_window_turns == 0);
            if (periodic_clear) func.clear();
            for (int i = 0; i < entry; ++i) {
                func.set(hashidx[i], -2);
            }
        }
        if (func.inner_len() == 1) {
            func = titan23::HashDict<int>(beam_width*8);
        }
        entry = 0;
        is_built = false;
    }

    BeamCandidate<ScoreType, Action> get_best() {
        return *min_element(next_beam.begin(), next_beam.begin() + entry, [] (const BeamCandidate<ScoreType, Action> &left, const BeamCandidate<ScoreType, Action> &right) {
            return left.score < right.score;
        });
    }
};
} // namespace flying_squirrel
