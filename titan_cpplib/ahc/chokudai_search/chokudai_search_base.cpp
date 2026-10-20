/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/chokudai_search/chokudai_search_base.cpp
#pragma once
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/chokudai_search/chokudai_param.cpp"
#include "titan_cpplib/ahc/beam_search/beam_result.cpp"
#include "titan_cpplib/ahc/beam_search/beam_history.cpp"
using namespace std;

namespace flying_squirrel::chokudai_detail {

/// @brief 世代別の未展開候補を保持し、各巡回で上位 beam_width 個を展開する
template<typename ScoreType, typename HashType, class Action, class State, ScoreType INF,
         bool variable_turn, bool record_history>
class Search {
    using Result = BeamResult<ScoreType, Action, State>;
    using Clock = chrono::steady_clock;

    struct Node {
        optional<Action> action;
        ScoreType score = INF;
        HashType hash{};
        int parent = -1, depth = 0, turn = 0, refs = 0, history_id = -1;
        uint64_t order = 0;
    };
    struct Entry {
        ScoreType score;
        uint64_t order;
        int node;
        bool operator<(const Entry& rhs) const {
            if (score < rhs.score) return true;
            if (rhs.score < score) return false;
            return order < rhs.order;
        }
    };
    struct Seen {
        ScoreType score;
        int pending_node;
    };
    struct Generation {
        set<Entry> pending;
        // 展開済みの最良評価も残す。より良い経路が来た場合だけ再展開する。
        unordered_map<HashType, Seen> seen;
    };

    // enumerate_actions 中の submit による追加でも last_action の参照を保つ。
    deque<Node> nodes;
    vector<int> free_nodes, forward_path;
    vector<unique_ptr<Generation>> generations;
    set<int> active_turns;
    vector<ScoreType> thresholds;
    vector<Action> actions;
    vector<HistoryNode<ScoreType, HashType>> history;
    vector<TurnSnapshot> snapshots;
    Clock::time_point started;
    double time_limit = -1;
    int max_turn = 0, current = 0, best_finished = -1, best_full = -1, best_partial = -1;
    int turns_done = 0;
    uint64_t next_order = 0;
    bool timed_out = false;

    double elapsed() const {
        return chrono::duration<double, milli>(Clock::now() - started).count();
    }

    bool expired() {
        if (!timed_out && time_limit >= 0 && elapsed() >= time_limit) timed_out = true;
        return timed_out;
    }

    Generation& generation(int turn) {
        if (!generations[turn]) generations[turn] = make_unique<Generation>();
        return *generations[turn];
    }

    int allocate(const Action& action, int parent, int turn, ScoreType score, HashType hash, int history_id) {
        int id;
        if (free_nodes.empty()) {
            id = (int)nodes.size();
            nodes.emplace_back();
        } else {
            id = free_nodes.back();
            free_nodes.pop_back();
        }
        Node& node = nodes[id];
        node.action.emplace(action);
        node.score = score;
        node.hash = hash;
        node.parent = parent;
        node.depth = nodes[parent].depth + 1;
        node.turn = turn;
        node.refs = 1;
        node.history_id = history_id;
        node.order = next_order++;
        ++nodes[parent].refs;
        return id;
    }

    // 候補、現在の State、最良解、子からの参照がなくなった枝を反復的に回収する。
    void release(int id) {
        while (id != -1 && --nodes[id].refs == 0) {
            int parent = nodes[id].parent;
            nodes[id].action.reset();
            free_nodes.push_back(id);
            id = parent;
        }
    }

    void set_best(int& best, int id) {
        ++nodes[id].refs;
        release(best);
        best = id;
    }

    // 移動先の所有権を呼び出し側から受け取る。論理 turn ではなく操作数で LCA を求める。
    void move_to(State& state, int target) {
        int from = current, to = target;
        forward_path.clear();
        while (nodes[from].depth > nodes[to].depth) {
            state.rollback(*nodes[from].action);
            from = nodes[from].parent;
        }
        while (nodes[to].depth > nodes[from].depth) {
            forward_path.push_back(to);
            to = nodes[to].parent;
        }
        while (from != to) {
            state.rollback(*nodes[from].action);
            from = nodes[from].parent;
            forward_path.push_back(to);
            to = nodes[to].parent;
        }
        for (auto it = forward_path.rbegin(); it != forward_path.rend(); ++it) {
            state.apply_op(*nodes[*it].action);
        }
        release(current);
        current = target;
    }

    int record(State& state, const Action& action, int turn, ScoreType score, HashType hash, int status) {
        if constexpr (record_history) {
            int id = (int)history.size();
            history.push_back({id, nodes[current].history_id, turn, score, hash,
                               action.to_string(), state.get_state_info(), status});
            return id;
        } else {
            return -1;
        }
    }

    void accept(State& state, Action& action) {
        if (expired()) return;
        auto [score, hash, finished] = [&] {
            if constexpr (variable_turn) return state.try_op(action, thresholds);
            else return state.try_op(action, INF);
        }();
        int turn = nodes[current].turn + 1;
        if constexpr (variable_turn) turn = action.target_turn;
        if (!(score < INF) || turn <= nodes[current].turn || turn > max_turn) {
            record(state, action, turn, score, hash, 2);
            return;
        }
        turns_done = max(turns_done, turn);

        // 終了候補も探索を打ち切らず保存する。未完成候補との hash 比較はしない。
        if (finished || turn == max_turn) {
            int& best = finished ? best_finished : best_full;
            bool improves = best == -1 || score < nodes[best].score;
            int hid = record(state, action, turn, score, hash, improves ? 0 : 1);
            if (improves) {
                int id = allocate(action, current, turn, score, hash, hid);
                set_best(best, id);
                release(id);
            }
            return;
        }

        Generation& gen = generation(turn);
        auto it = gen.seen.find(hash);
        if (it != gen.seen.end() && !(score < it->second.score)) {
            record(state, action, turn, score, hash, 1);
            return;
        }
        if (it != gen.seen.end() && it->second.pending_node != -1) {
            int old = it->second.pending_node;
            gen.pending.erase({nodes[old].score, nodes[old].order, old});
            if constexpr (record_history) history[nodes[old].history_id].status = 1;
            release(old);
        }
        int hid = record(state, action, turn, score, hash, 0);
        int id = allocate(action, current, turn, score, hash, hid);
        gen.seen.insert_or_assign(hash, Seen{score, id});
        gen.pending.insert({score, nodes[id].order, id});
        active_turns.insert(turn);
        if (best_partial == -1 || turn > nodes[best_partial].turn ||
            (turn == nodes[best_partial].turn && score < nodes[best_partial].score)) {
            set_best(best_partial, id);
        }
    }

    struct Submitter {
        Search& search;
        State& state;
        // 保留候補数を制限しないので、途中評価による枝刈りは行わない。
        ScoreType threshold() const { return INF; }
        ScoreType threshold(int) const { return INF; }
        void operator()(Action& action) { search.accept(state, action); }
    };

    void expand(State& state) {
        const Action& last_action = *nodes[current].action;
        Submitter submit{*this, state};
        if constexpr (variable_turn) {
            state.enumerate_actions(last_action, submit);
        } else if constexpr (requires { state.enumerate_actions(0, last_action, submit); }) {
            state.enumerate_actions(nodes[current].turn, last_action, submit);
        } else {
            actions.clear();
            state.enumerate_actions(actions, nodes[current].turn, last_action, INF);
            for (Action& action : actions) {
                if (expired()) break;
                submit(action);
            }
        }
    }

    void init(const BeamParam& param) {
        started = Clock::now();
        time_limit = param.time_limit;
        max_turn = param.max_turn;
        current = turns_done = 0;
        best_finished = best_full = best_partial = -1;
        timed_out = false;
        next_order = 1;
        nodes.clear();
        free_nodes.clear();
        forward_path.clear();
        generations.clear();
        generations.resize((size_t)max_turn + 1);
        active_turns.clear();
        thresholds.assign((size_t)max_turn + 1, INF);
        actions.clear();
        history.clear();
        snapshots.clear();
        nodes.emplace_back();
        nodes[0].action.emplace();
        if constexpr (variable_turn) nodes[0].action->target_turn = -1;
        nodes[0].refs = 2; // current と世代 0 の候補
        generation(0).pending.insert({INF, 0, 0});
        active_turns.insert(0);
    }

    // BeamParam の int 統計は大量の再巡回で溢れうる。探索には使わず飽和させる。
    void timestamp(BeamParam& param, int expanded, double duration) const {
        auto add = [](int value, size_t delta) {
            return (int)min<long long>(numeric_limits<int>::max(), (long long)value + (long long)delta);
        };
        param.pool_size_sum = add(param.pool_size_sum, nodes.size() - free_nodes.size());
        param.beam_width_sum = add(param.beam_width_sum, expanded);
        param.turn_sum = add(param.turn_sum, 1);
        param.time_sum += duration;
        param.width_hist.push_back(expanded);
    }

    template<bool materialize_final_state>
    Result run(BeamParam& param, long long max_sweeps, bool verbose, const string& history_file) {
        if (param.max_turn <= 0 || param.beam_width <= 0 ||
            !isfinite(param.time_limit) || max_sweeps < -1) {
            return {{}, INF, 0, 0.0, BeamStatus::InvalidParameter, nullptr};
        }
        init(param);
        State state;
        state.init();
        long long sweeps = 0;
        BeamStatus status = BeamStatus::NoCandidates;
        while (!active_turns.empty()) {
            if (expired()) {
                status = BeamStatus::TimeLimitReached;
                break;
            }
            if (max_sweeps >= 0 && sweeps >= max_sweeps) {
                status = BeamStatus::SweepLimitReached;
                break;
            }
            ++sweeps;
            int turn = *active_turns.begin();
            while (turn < max_turn) {
                Generation& gen = *generations[turn];
                double before = elapsed();
                int expanded = 0;
                while (expanded < param.beam_width && !gen.pending.empty()) {
                    if (expired()) break;
                    int id = gen.pending.begin()->node;
                    gen.pending.erase(gen.pending.begin());
                    if (id != 0) gen.seen.find(nodes[id].hash)->second.pending_node = -1;
                    move_to(state, id);
                    expand(state);
                    ++expanded;
                }
                if (gen.pending.empty()) active_turns.erase(turn);
                timestamp(param, expanded, elapsed() - before);
                if constexpr (record_history) {
                    vector<int> pending_ids;
                    for (const Entry& entry : gen.pending) pending_ids.push_back(nodes[entry.node].history_id);
                    snapshots.push_back({turn, move(pending_ids)});
                }
                if (timed_out) break;
                auto next = active_turns.upper_bound(turn);
                if (next == active_turns.end()) break;
                turn = *next;
            }
            if (timed_out) {
                status = BeamStatus::TimeLimitReached;
                break;
            }
        }

        int best = best_partial;
        if (best_finished != -1) {
            best = best_finished;
            status = BeamStatus::Finished;
        } else if (best_full != -1) {
            best = best_full;
            status = BeamStatus::MaxTurnReached;
        }
        vector<Action> path;
        ScoreType score = INF;
        if (best != -1) {
            score = nodes[best].score;
            for (int id = best; nodes[id].parent != -1; id = nodes[id].parent) {
                path.push_back(*nodes[id].action);
            }
            reverse(path.begin(), path.end());
        }
        double elapsed_ms = elapsed();
        if constexpr (record_history) {
            if (!history_file.empty()) dump_history_json(history_file, INF, history, snapshots);
        }
        if (verbose) {
            cerr << (variable_turn ? "ChokudaiSearchTurn" : "ChokudaiSearch")
                 << ": sweeps=" << sweeps << ", turns=" << turns_done
                 << ", score=" << score << ", elapsed_ms=" << elapsed_ms << '\n';
        }
        unique_ptr<State> final_state;
        if constexpr (materialize_final_state) {
            if (best != -1) {
                for (int id = current; nodes[id].parent != -1; id = nodes[id].parent) {
                    state.rollback(*nodes[id].action);
                }
                final_state = make_final_state<true>(state, path);
            }
        }
        return {move(path), score, turns_done, elapsed_ms, status, move(final_state)};
    }

public:
    /// @brief 既存ビームサーチと同じ設定・状態・返却型で実行する
    template<bool materialize_final_state=true>
    Result search(BeamParam& param, bool verbose=false, const string& history_file="") {
        return run<materialize_final_state>(param, -1, verbose, history_file);
    }

    /// @brief 巡回回数を制限して実行する
    template<bool materialize_final_state=true>
    Result search(ChokudaiParam& param, bool verbose=false, const string& history_file="") {
        return run<materialize_final_state>(param, param.max_sweeps, verbose, history_file);
    }
};

} // namespace flying_squirrel::chokudai_detail
