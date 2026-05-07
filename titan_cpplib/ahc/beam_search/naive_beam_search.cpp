#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
using namespace std;

namespace flying_squirrel {

/// @brief 愚直ビームサーチ
/// @note 要件: init, try_op, apply_op, get_actions
/// @tparam ScoreType スコアの型
/// @tparam HashType ハッシュの型
/// @tparam Action Action
/// @tparam State State
/// @tparam INF INF(NOTE -INFが成り立つ)
/// @tparam record_history 途中状態を可視化する用のログ出力を出すかどうか
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF, bool record_history=false>
class NaiveBeamSearch {
private:
    struct BeamCandidate {
        int par_idx;
        ScoreType score;
        Action action;
    };

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
        // 次のビーム候補を保持する配列
        vector<BeamCandidate> next_beam;

        Candidates() {}

        int size() const { return entry; }

        /// @brief 現在のworstを返す / これ以上なら意味ない
        ScoreType threshold() const { return entry < beam_width ? INF : seg[1].first; }

        /// 追加できたらtrueを返す
        bool push(ScoreType score, HashType hash, int par, const Action &action) {
            if (entry == beam_width && score >= seg[1].first) {
                return false;
            }
            auto dat = func.get_pos(hash);
            int idx = func.inner_get(dat, -1);
            if (idx != -1) {
                if (score < seg[idx+s].first) {
                    next_beam[idx] = {par, score, move(action)};
                    set(idx, {score, idx});
                    return true;
                }
                return false;
            }
            if (entry < beam_width) {
                func.inner_set(dat, hash, entry);
                next_beam[entry] = {par, score, move(action)};
                hashidx[entry] = hash;
                set(entry, {score, entry});
                entry++;
                return true;
            }
            auto [_, i] = seg[1];
            next_beam[i] = {par, score, move(action)};
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
            fill(seg.begin(), seg.begin()+(2*s), make_pair(-INF, -1));
            if (hashidx.size() < w) {
                hashidx.resize(w);
                next_beam.resize(w);
            }
            func.clear();
            if (func.inner_len() == 1) {
                func = titan23::HashDict<int>(beam_width*8);
            }
            entry = 0;
        }

        void shuffle(titan23::Random &rnd) {
            for (int i = 0; i < entry-1; ++i) {
                int j = rnd.randrange(i, entry);
                swap(next_beam[i], next_beam[j]);
            }
        }

        BeamCandidate get_best() {
            return *min_element(next_beam.begin(), next_beam.begin() + entry, [&] (const BeamCandidate &left, const BeamCandidate &right) {
                return left.score < right.score;
            });
        }
    } candidates;

    struct HistoryNode {
        int parent_id;
        Action action;
    };

    struct Node {
        State state;
        ScoreType score;
        int history_id;
    };

    titan23::Random rnd;
    int node_id_counter;
    bool found_finished;
    ScoreType best_finished_score;
    vector<HistoryNode> history_nodes;
    titan23::Timer beam_timer;

    void init_bs(const BeamParam &param) {
        beam_timer.reset();
        rnd = titan23::Random();
        node_id_counter = 0;
        found_finished = false;
        best_finished_score = INF;
    }

    vector<Action> build_history(int last_id) const {
        vector<Action> res;
        int now = last_id;
        while (now != -1) {
            res.emplace_back(history_nodes[now].action);
            now = history_nodes[now].parent_id;
        }
        reverse(res.begin(), res.end());
        return res;
    }

public:
    vector<Action> search(BeamParam &param, const bool verbose=false, const string& history_file = "") {
        init_bs(param);
        vector<Node> beam, next_beam;
        State initial_state;
        initial_state.init();
        beam.push_back({initial_state, 0, -1});
        int best_finished_history_id = -1;
        vector<Action> actions;
        for (int turn = 0; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();
            if (verbose) cerr << "\n[BeamSearch] Info: # turn : " << turn+1 << " | " << now_time << " [ms]" << endl;
            const int width = param.get_beam_width();
            if (verbose) cerr << "\n[BeamSearch] Info: \twidth = " << w << endl;
            candidates.reset(turn, width);
            for (int i = 0; i < (int)beam.size(); ++i) {
                State &state = beam[i].state;
                state.get_actions(actions, turn);
                for (const Action &action : actions) {
                    auto [score, hash, finished] = state.try_op(action, candidates.threshold());
                    if (score == INF) continue;
                    if (finished) {
                        if (!found_finished || score < best_finished_score) {
                            found_finished = true;
                            best_finished_score = score;
                            int new_history_id = history_nodes.size();
                            history_nodes.emplace_back(beam[i].history_id, action);
                            best_finished_history_id = new_history_id;
                        }
                    } else {
                        candidates.push(score, hash, i, action);
                    }
                }
            }
            if (found_finished) {
                if (verbose) cerr << to_green("[BeamSearch] Info: find valid solution.") << endl;
                break;
            }
            if (candidates.size() == 0) {
                cerr << to_red("[BeamSearch] Error: \t次の候補が見つかりませんでした") << endl;
                assert(candidates.size() > 0);
            }
            if (verbose) {
                BeamCandidate bests = candidates.get_best();
                cerr << "[BeamSearch] Info: \tbest_score = " << bests.score << endl;
            }
            next_beam.clear();
            for (int i = 0; i < (int)candidates.size(); ++i) {
                const BeamCandidate &cand = candidates.next_beam[i];
                State next_state = beam[cand.par_idx].state;
                next_state.apply_op(cand.action);
                int new_history_id = history_nodes.size();
                history_nodes.emplace_back(beam[cand.par_idx].history_id, cand.action);
                next_beam.emplace_back(next_state, cand.score, new_history_id);
            }
            swap(beam, next_beam);
            param.timestamp(beam.size(), candidates.size(), beam_timer.elapsed()-now_time);
        }
        if (found_finished) {
            param.report();
            return build_history(best_finished_history_id);
        }
        if (verbose) {
            cerr << to_green("[BeamSearch] Info: max_turn finished.") << endl;
            param.report();
        }
        int best_idx = 0;
        for (int i = 1; i < (int)beam.size(); ++i) {
            if (beam[i].score < beam[best_idx].score) {
                best_idx = i;
            }
        }
        return build_history(beam[best_idx].history_id);
    }
};
} // namespace flying_squirrel
