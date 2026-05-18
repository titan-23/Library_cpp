#pragma once
#include <bits/stdc++.h>

using namespace std;

//! ビームサーチの可視化用探索履歴
//! beam_search.cpp / beam_fast.cpp で共有
namespace flying_squirrel {

template<typename ScoreType, typename HashType>
struct HistoryNode {
    int node_id;
    int parent_id;
    int turn;
    ScoreType score;
    HashType hash;
    string action_str;
    string state_info;
    int status; // 0: 採用, 1: 破棄, 2: 無効
};

struct TurnSnapshot {
    int turn;
    vector<int> active_node_ids;
};

template<typename ScoreType, typename HashType>
void dump_history_json(
    const string& filename,
    ScoreType INF,
    const vector<HistoryNode<ScoreType, HashType>>& history,
    const vector<TurnSnapshot>& snapshots
) {
    ofstream ofs(filename);
    ofs << "{\n  \"INF\": " << INF << ",\n  \"nodes\": [\n";
    for (size_t i = 0; i < history.size(); ++i) {
        const auto& node = history[i];
        ofs << "    {\n"
            << "      \"node_id\": " << node.node_id << ",\n"
            << "      \"parent_id\": " << node.parent_id << ",\n"
            << "      \"turn\": " << node.turn << ",\n"
            << "      \"score\": " << node.score << ",\n"
            << "      \"hash\": " << node.hash << ",\n"
            << "      \"action\": \"" << node.action_str << "\",\n"
            << "      \"state_info\": " << (node.state_info.empty() ? "{}" : node.state_info) << ",\n"
            << "      \"status\": " << node.status << "\n"
            << "    }";
        if (i + 1 < history.size()) ofs << ",";
        ofs << "\n";
    }
    ofs << "  ],\n  \"snapshots\": [\n";
    for (size_t i = 0; i < snapshots.size(); ++i) {
        ofs << "    {\n"
            << "      \"turn\": " << snapshots[i].turn << ",\n"
            << "      \"active_node_ids\": [";
        for (size_t j = 0; j < snapshots[i].active_node_ids.size(); ++j) {
            ofs << snapshots[i].active_node_ids[j];
            if (j + 1 < snapshots[i].active_node_ids.size()) ofs << ", ";
        }
        ofs << "]\n    }";
        if (i + 1 < snapshots.size()) ofs << ",";
        ofs << "\n";
    }
    ofs << "  ]\n}\n";
}
} // namespace flying_squirrel
