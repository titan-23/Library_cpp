/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/candidates.cpp
#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"

namespace flying_squirrel {

template<typename ScoreType, class Action>
struct BeamCandidate {
    int parent_leaf;
    ScoreType score;
    Action action;
    int node_id = -1; // 履歴記録用
};

/// @brief ハッシュの重複を除き、上位候補をビーム幅まで保持する
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class Candidates {
private:
    using T = pair<ScoreType, int>;
    vector<HashType> hashidx;
    titan23::HashDict<int> func;
    int beam_width = 0, entry = 0;
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

    /// @brief 候補が採用された場合は true を返す
    /// @param node_id 履歴記録用の ID
    ///                候補の比較には使用しない
    bool push(ScoreType score, HashType hash, int parent_leaf, Action action, int node_id = -1) {
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

    /// @brief 採用が決まった候補だけ make() で Action を生成する
    template<class Make>
    bool push_lazy(ScoreType score, HashType hash, int parent_leaf, Make &&make, int node_id = -1) {
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
                next_beam[idx].parent_leaf = parent_leaf;
                next_beam[idx].score = score;
                next_beam[idx].action = make();
                next_beam[idx].node_id = node_id;
                if (is_built) {
                    set(idx, {score, idx});
                }
                return true;
            }
            return false;
        }
        if (entry < beam_width) {
            func.inner_set(dat, hash, entry);
            next_beam[entry].parent_leaf = parent_leaf;
            next_beam[entry].score = score;
            next_beam[entry].action = make();
            next_beam[entry].node_id = node_id;
            hashidx[entry] = hash;
            entry++;
            if (entry == beam_width) {
                build_segtree();
                is_built = true;
            }
            return true;
        }
        auto [_, i] = seg[1];
        next_beam[i].parent_leaf = parent_leaf;
        next_beam[i].score = score;
        next_beam[i].action = make();
        next_beam[i].node_id = node_id;
        func.set(hashidx[i], -1);
        func.inner_set(dat, hash, i);
        hashidx[i] = hash;
        set(i, {score, i});
        return true;
    }

    /// @brief 次ターンに向けて候補を初期化する
    /// @param hash_window_turns clear_hash が false のときにハッシュを破棄する間隔
    ///                          0 なら破棄しない
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
            bool periodic_clear = hash_window_turns > 0 && (turn % hash_window_turns == 0);
            if (periodic_clear) func.clear();
            // 前ターンの生存候補は、定期破棄時も予約済みとして引き継ぐ
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
        return *min_element(next_beam.begin(), next_beam.begin() + entry,
                            [] (const BeamCandidate<ScoreType, Action> &left,
                                const BeamCandidate<ScoreType, Action> &right) {
                                return left.score < right.score;
                            });
    }
};

using ActionId = int;
inline constexpr ActionId BAD_ID_FLAT = -1;

template<typename ScoreType, typename HashType>
struct BeamCandidateFlat {
    int parent_leaf;
    ScoreType score;
    HashType hash;
    ActionId aid;
    int node_id = -1; // 履歴記録用
};

/// @brief 候補の格納位置と、置き換えられた ActionId を表す
struct PushResult {
    int slot; // 棄却時は -1
    ActionId evicted_aid; // 置き換えなしなら BAD_ID_FLAT
};

/// @brief Action を ID で参照し、ハッシュの重複排除と上位候補の保持を行う
template<typename ScoreType, typename HashType, ScoreType INF>
class CandidatesFlat {
private:
    using T = pair<ScoreType, int>;
    vector<HashType> hashidx;
    titan23::HashDict<int> func;
    int beam_width = 0, entry = 0;
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
    vector<BeamCandidateFlat<ScoreType, HashType>> next_beam;

    CandidatesFlat() {}

    int size() const { return entry; }

    ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

    /// @brief ActionId 付きの候補を追加する
    /// @param aid 呼び出し側で確保した ActionId
    /// @return slot が -1 なら棄却
    ///         evicted_aid が BAD_ID_FLAT でなければ旧 ActionId が置き換えられている
    /// @note 棄却された aid と、置き換えられた evicted_aid は呼び出し側が解放する
    PushResult push(ScoreType score, HashType hash, int parent_leaf, ActionId aid, int node_id = -1) {
        if (is_built && score >= seg[1].first) {
            return {-1, BAD_ID_FLAT};
        }
        auto dat = func.get_pos(hash);
        int idx = func.inner_get(dat, -1);
        if (idx == -2) {
            return {-1, BAD_ID_FLAT};
        }
        if (idx != -1) {
            if (score < next_beam[idx].score) {
                ActionId old_aid = next_beam[idx].aid;
                next_beam[idx] = {parent_leaf, score, hash, aid, node_id};
                if (is_built) {
                    set(idx, {score, idx});
                }
                return {idx, old_aid};
            }
            return {-1, BAD_ID_FLAT};
        }
        if (entry < beam_width) {
            func.inner_set(dat, hash, entry);
            next_beam[entry] = {parent_leaf, score, hash, aid, node_id};
            hashidx[entry] = hash;
            int slot = entry;
            entry++;
            if (entry == beam_width) {
                build_segtree();
                is_built = true;
            }
            return {slot, BAD_ID_FLAT};
        }
        auto [_, i] = seg[1];
        ActionId old_aid = next_beam[i].aid;
        next_beam[i] = {parent_leaf, score, hash, aid, node_id};
        func.set(hashidx[i], -1);
        func.inner_set(dat, hash, i);
        hashidx[i] = hash;
        set(i, {score, i});
        return {i, old_aid};
    }

    /// @brief 次ターンに向けて候補を初期化する
    /// @param hash_window_turns clear_hash が false のときにハッシュを破棄する間隔
    ///                          0 なら破棄しない
    void reset(int turn, int w, bool clear_hash, int hash_window_turns = 0) {
        beam_width = w;
        while (s < w) {
            s <<= 1;
        }
        if ((int)seg.size() < 2*s) {
            seg.resize(2*s);
        }
        fill(seg.begin(), seg.begin()+(2*s), make_pair(-INF, -1));
        if ((int)hashidx.size() < w) {
            hashidx.resize(w);
            next_beam.resize(w);
        }
        if (clear_hash) {
            func.clear();
        } else {
            bool periodic_clear = hash_window_turns > 0 && (turn % hash_window_turns == 0);
            if (periodic_clear) func.clear();
            // 前ターンの生存候補は、定期破棄時も予約済みとして引き継ぐ
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

    BeamCandidateFlat<ScoreType, HashType> get_best() {
        return *min_element(next_beam.begin(), next_beam.begin() + entry, [] (const auto &l, const auto &r) {
            return l.score < r.score;
        });
    }
};
} // namespace flying_squirrel
