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

    void set(int k, T v) {
        k += s;
        seg[k] = v;
        while (k > 1) {
            k >>= 1;
            seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
        }
    }

public:
    vector<BeamCandidate<ScoreType, Action>> next_beam;

    Candidates() {}

    int size() const { return entry; }

    ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

    bool push(
        ScoreType score, HashType hash,
        int parent_leaf, Action action
    ) {
        if (entry == beam_width && score >= seg[1].first) {
            return false;
        }
        auto dat = func.get_pos(hash);
        int idx = func.inner_get(dat, -1);
        if (idx != -1) {
            if (score < seg[idx+s].first) {
                next_beam[idx] = {parent_leaf, score, move(action)};
                set(idx, {score, idx});
                return true;
            }
            return false;
        }
        if (entry < beam_width) {
            func.inner_set(dat, hash, entry);
            next_beam[entry] = {parent_leaf, score, move(action)};
            hashidx[entry] = hash;
            set(entry, {score, entry});
            entry++;
            return true;
        }
        auto [_, i] = seg[1];
        next_beam[i] = {parent_leaf, score, move(action)};
        func.set(hashidx[i], -1);
        func.inner_set(dat, hash, i);
        hashidx[i] = hash;
        set(i, {score, i});
        return true;
    }

    void reset(int turn, int w) {
        beam_width = w;
        while (s < w) {
            s <<= 1;
        }
        if (seg.size() < 2*s) {
            seg.resize(2*s);
        }
        fill(seg.begin(), seg.begin()+(2*w), make_pair(-INF, -1));
        if (hashidx.size() < w) {
            hashidx.resize(w);
            next_beam.resize(w);
        }
        func.clear(); // TODO
        if (func.inner_len() == 1) {
            func = titan23::HashDict<int>(beam_width*8);
        }
        entry = 0;
    }

    BeamCandidate<ScoreType, Action> get_best() {
        return *min_element(next_beam.begin(), next_beam.begin() + entry, [&] (const BeamCandidate<ScoreType, Action> &left, const BeamCandidate<ScoreType, Action> &right) {
            return left.score < right.score;
        });
    }
};
} // namespace flying_squirrel
