#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ahc/beam_search/beam_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_log.cpp"
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
        bool push(ScoreType score, HashType hash, int par, Action action) {
            if (entry == beam_width && score >= seg[1].first) {
                return false;
            }
            auto dat = func.get_pos(hash);
            int idx = func.inner_get(dat, -1);
            if (idx == -2) {
                // 過去ターンで採用済みのハッシュ → 即 drop
                return false;
            }
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

        void reset(int turn, int w, bool clear_hash) {
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
                for (int i = 0; i < entry; ++i) {
                    func.set(hashidx[i], -2);
                }
            }
            if (func.inner_len() == 1) {
                func = titan23::HashDict<int>(beam_width*8);
            }
            entry = 0;
        }

        BeamCandidate get_best() {
            return *min_element(next_beam.begin(), next_beam.begin() + entry, [] (const BeamCandidate &left, const BeamCandidate &right) {
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
        Action last_action;
    };

    bool found_finished;
    Action DUMMY_ACTION;
    ScoreType best_finished_score;
    vector<HistoryNode> history_nodes;
    titan23::Timer beam_timer;

    void init_bs(const BeamParam &param) {
        beam_timer.reset();
        found_finished = false;
        best_finished_score = INF;
        history_nodes.clear();
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
        if (verbose) {
            beam_log::start_banner(cerr, "NaiveBeamSearch", param);
            if (param.is_adjusting) beam_log::warn(cerr, "dynamic beam width is experimental");
        }
        vector<Node> beam, next_beam;
        State initial_state;
        initial_state.init();
        beam.push_back({initial_state, 0, -1, DUMMY_ACTION});
        int best_finished_history_id = -1;
        int turns_done = 0;
        vector<Action> actions;
        for (int turn = 0; turn < param.max_turn; ++turn) {
            double now_time = beam_timer.elapsed();
            const int width = param.get_beam_width(param.max_turn - turn, (int)beam.size(), param.time_limit - beam_timer.elapsed());
            candidates.reset(turn, width, param.clear_hash_every_turn);
            for (int i = 0; i < (int)beam.size(); ++i) {
                State &state = beam[i].state;
                actions.clear();
                state.get_actions(actions, turn, beam[i].last_action, candidates.threshold());
                for (Action &action : actions) {
                    auto [score, hash, finished] = state.try_op(action, candidates.threshold());
                    if (score >= INF) continue;
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
            if (candidates.size() == 0) {
                if (found_finished) {
                    turns_done = turn + 1;
                    if (verbose) beam_log::info(cerr, "no more candidates, returning best finished");
                    break;
                }
                beam_log::on_no_candidates(cerr, turn);
                assert(candidates.size() > 0);
            }
            if (verbose) {
                BeamCandidate bests = candidates.get_best();
                beam_log::turn_line(cerr, turn + 1, param.max_turn, now_time,
                                    width, (int)beam.size(), (int)candidates.size(), bests.score);
            }
            next_beam.clear();
            for (int i = 0; i < (int)candidates.size(); ++i) {
                const BeamCandidate &cand = candidates.next_beam[i];
                State next_state = beam[cand.par_idx].state;
                next_state.apply_op(cand.action);
                int new_history_id = history_nodes.size();
                history_nodes.emplace_back(beam[cand.par_idx].history_id, cand.action);
                next_beam.emplace_back(move(next_state), cand.score, new_history_id, cand.action);
            }
            swap(beam, next_beam);
            param.timestamp(beam.size(), candidates.size(), beam_timer.elapsed()-now_time);
            turns_done = turn + 1;
        }
        if (found_finished) {
            if (verbose) {
                beam_log::on_solution_found(cerr, turns_done, best_finished_score);
                vector<Action> sol = build_history(best_finished_history_id);
                beam_log::end_banner(cerr, "solution found", turns_done, param.max_turn,
                                     beam_timer.elapsed(), param.ave_width(),
                                     best_finished_score, true, (int)sol.size());
                return sol;
            }
            return build_history(best_finished_history_id);
        }
        int best_idx = 0;
        for (int i = 1; i < (int)beam.size(); ++i) {
            if (beam[i].score < beam[best_idx].score) {
                best_idx = i;
            }
        }
        vector<Action> sol = build_history(beam[best_idx].history_id);
        if (verbose) {
            beam_log::on_max_turn(cerr);
            beam_log::end_banner(cerr, "max_turn reached", turns_done, param.max_turn,
                                 beam_timer.elapsed(), param.ave_width(),
                                 beam[best_idx].score, true, (int)sol.size());
        }
        return sol;
    }
};
} // namespace flying_squirrel
