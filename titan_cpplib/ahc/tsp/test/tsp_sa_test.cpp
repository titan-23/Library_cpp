#include <bits/stdc++.h>
#include "titan_cpplib/ahc/sa/sa.cpp"
#include "titan_cpplib/ahc/tsp/tsp_initial_state.cpp"
using namespace std;
using namespace titan23;

using Cost = long long;

struct MatrixCost {
    const vector<vector<Cost>>* distance;

    Cost operator()(int u, int v) const {
        return (*distance)[u][v];
    }
};

using Problem = TspProblem<MatrixCost>;

class TspSaState {
public:
    using ScoreType = Cost;

    struct Param {
        double start_temp;
        double end_temp;

        Param() : start_temp(30), end_temp(0.1) {}
    };

    struct Changed {
        int TYPE_CNT = 1;
        int type = 0;
    } changed;

    struct Result {
        ScoreType score = 0;
        ScoreType true_score = 0;
        vector<int> order;

        void print(ostream& output = cout) const {
            for (int node : order) output << node << ' ';
            output << '\n';
        }
    };

    inline static Param param;
    bool is_valid = true;
    ScoreType score;

    TspSaState(const Problem& problem, const TspCandidates& candidates, TspState<Cost> tsp, uint32_t seed)
        : score(0), problem_(&problem), candidates_(&candidates), tsp_(move(tsp)), random_(seed) {
        score = tsp_.total_cost();
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const { return score; }

    void modify(int64_t, ScoreType, double) {
        changed.type = 0;
        pending_.reset();
        auto [left, right] = random_.rand_pair(1, tsp_.size());
        pending_ = tsp_.make_two_opt(*problem_, left, right);
        if (!pending_) {
            is_valid = false;
            return;
        }
        score = tsp_.total_cost() + pending_->delta();
    }

    void rollback() {
        pending_.reset();
    }

    void advance() {
        tsp_.apply(*problem_, *pending_);
        pending_.reset();
    }

    Result get_result() const {
        return {score, score, tsp_.order()};
    }

private:
    const Problem* problem_;
    const TspCandidates* candidates_;
    TspState<Cost> tsp_;
    Random random_;
    optional<TspTwoOptMove<Cost>> pending_;
};

int main() {
    int n = 10;
    vector<vector<Cost>> distance(n, vector<Cost>(n));
    for (int u = 0; u < n; ++u) for (int v = u + 1; v < n; ++v) {
        distance[u][v] = distance[v][u] = 1 + (u * 37 + v * 53 + u * v * 11) % 100;
    }
    Problem problem(n, MatrixCost{&distance});
    TspCandidates candidates = make_tsp_candidates(problem, 5);
    TspInitialStateOptions initial_options;
    initial_options.search = TspInitialSearch::two_opt;
    initial_options.local_search.max_evaluated_moves = 1000;
    auto tsp = problem.make_nearest_neighbor_state(0);
    improve_tsp_initial_state(problem, candidates, tsp, initial_options);
    TspSaState state(problem, candidates, move(tsp), 23);
    auto result = sa::sa_run<TspSaState>(1.0, state, false);
    assert(result.order.size() == (size_t)n);
    assert(result.order[0] == 0);
    cout << "ok\n";
}
