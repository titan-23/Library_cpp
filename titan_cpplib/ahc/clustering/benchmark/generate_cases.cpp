/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/benchmark/generate_cases.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/benchmark/clustering_benchmark.cpp"
#include "titan_cpplib/alg/random.cpp"
using namespace std;
using namespace titan23;
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;

enum class Family {
    separated,
    overlapping,
    elongated,
    imbalanced,
    outliers,
    duplicates
};

enum class SizeCondition {
    free,
    exact,
    range
};

struct CaseSpec {
    string name;
    int point_count;
    int dimension;
    int cluster_count;
    Family family;
    SizeCondition size_condition;
    uint32_t seed;
};

class NormalRandom {
public:
    explicit NormalRandom(uint32_t seed) : random_(seed) {}

    double next() {
        if (has_saved_) {
            has_saved_ = false;
            return saved_;
        }
        double u1 = ((random_.rand_u64() >> 11) + 0.5) / 9007199254740992.0;
        double u2 = ((random_.rand_u64() >> 11) + 0.5) / 9007199254740992.0;
        double radius = sqrt(-2 * log(u1));
        double angle = 2 * pi * u2;
        saved_ = radius * sin(angle);
        has_saved_ = true;
        return radius * cos(angle);
    }

    void shuffle(vector<int>& values) {
        random_.shuffle(values);
    }

private:
    Random random_;
    bool has_saved_ = false;
    double saved_ = 0;
};

vector<int> make_sizes(const CaseSpec& spec) {
    vector<int> sizes(spec.cluster_count, 1);
    int remaining = spec.point_count - spec.cluster_count;
    if (spec.family != Family::imbalanced) {
        for (int i = 0; i < remaining; ++i) ++sizes[i % spec.cluster_count];
        return sizes;
    }
    long long weight_sum = (long long)spec.cluster_count * (spec.cluster_count + 1) / 2;
    vector<pair<long long, int>> remainders;
    remainders.reserve(spec.cluster_count);
    int assigned = 0;
    for (int cluster = 0; cluster < spec.cluster_count; ++cluster) {
        long long numerator = (long long)remaining * (spec.cluster_count - cluster);
        int add = numerator / weight_sum;
        sizes[cluster] += add;
        assigned += add;
        remainders.emplace_back(-(numerator % weight_sum), cluster);
    }
    sort(remainders.begin(), remainders.end());
    for (int i = 0; i < remaining - assigned; ++i) ++sizes[remainders[i].second];
    return sizes;
}

vector<vector<double>> make_centers(const CaseSpec& spec) {
    double neighbor_distance = spec.family == Family::separated ? 9 : 2.2;
    if (spec.family == Family::outliers) neighbor_distance = 4;
    if (spec.family == Family::duplicates) neighbor_distance = 3;
    double radius = max(neighbor_distance, neighbor_distance * spec.cluster_count / (2 * pi));
    vector<vector<double>> centers(spec.cluster_count, vector<double>(spec.dimension));
    for (int cluster = 0; cluster < spec.cluster_count; ++cluster) {
        double angle = 2 * pi * cluster / spec.cluster_count;
        centers[cluster][0] = radius * cos(angle);
        if (spec.dimension >= 2) centers[cluster][1] = radius * sin(angle);
        for (int axis = 2; axis < spec.dimension; ++axis) {
            centers[cluster][axis] = neighbor_distance * sin((cluster + 1) * (axis + 1) * 1.3247179572447458);
        }
    }
    return centers;
}

ClusteringBenchmarkCase make_case(const CaseSpec& spec) {
    if (spec.point_count < spec.cluster_count || spec.dimension <= 0 || spec.cluster_count <= 0) throw invalid_argument("invalid synthetic clustering case");
    NormalRandom random(spec.seed);
    vector<int> sizes = make_sizes(spec);
    vector<int> labels;
    labels.reserve(spec.point_count);
    for (int cluster = 0; cluster < spec.cluster_count; ++cluster) for (int count = 0; count < sizes[cluster]; ++count) labels.push_back(cluster);
    random.shuffle(labels);
    vector<vector<double>> centers = make_centers(spec);
    vector<vector<double>> points(spec.point_count, vector<double>(spec.dimension));
    for (int point = 0; point < spec.point_count; ++point) {
        int cluster = labels[point];
        double angle = 2 * pi * cluster / spec.cluster_count;
        if (spec.family == Family::elongated && spec.dimension >= 2) {
            double major = random.next() * 4;
            double minor = random.next() * 0.25;
            points[point][0] = centers[cluster][0] + major * cos(angle) - minor * sin(angle);
            points[point][1] = centers[cluster][1] + major * sin(angle) + minor * cos(angle);
        }
        for (int axis = 0; axis < spec.dimension; ++axis) {
            if (spec.family == Family::elongated && spec.dimension >= 2 && axis < 2) continue;
            double noise = random.next();
            double scale = spec.family == Family::overlapping || spec.family == Family::imbalanced ? 1.4 : 1;
            points[point][axis] = centers[cluster][axis] + noise * scale;
        }
        if (spec.family == Family::outliers && point % 23 == 0) {
            for (int axis = 0; axis < spec.dimension; ++axis) points[point][axis] += random.next() * 12;
        }
        if (spec.family == Family::duplicates && point > 0 && point % 4 == 0) {
            int source = point - 1;
            labels[point] = labels[source];
            points[point] = points[source];
        }
    }
    sizes.assign(spec.cluster_count, 0);
    for (int label : labels) ++sizes[label];
    ClusteringBenchmarkCase problem;
    problem.name = spec.name;
    problem.source = "titan synthetic v1, seed=" + to_string(spec.seed);
    problem.points = move(points);
    problem.cluster_count = spec.cluster_count;
    problem.reference_labels = move(labels);
    if (spec.size_condition == SizeCondition::free) {
        problem.ranges.assign(spec.cluster_count, {1, spec.point_count});
    } else if (spec.size_condition == SizeCondition::exact) {
        for (int size : sizes) problem.ranges.push_back({size, size});
    } else {
        for (int size : sizes) {
            int margin = max(1, size / 10);
            problem.ranges.push_back({max(1, size - margin), min(spec.point_count, size + margin)});
        }
    }
    check_clustering_benchmark_case(problem);
    return problem;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            cerr << "usage: generate_cases OUTPUT_DIRECTORY\n";
            return 1;
        }
        filesystem::path output_directory = argv[1];
        filesystem::create_directories(output_directory);
        vector<CaseSpec> specs = {
            {"separated_1000_2_8_free", 1000, 2, 8, Family::separated, SizeCondition::free, 1001},
            {"overlapping_5000_2_16_free", 5000, 2, 16, Family::overlapping, SizeCondition::free, 1002},
            {"elongated_5000_2_12_free", 5000, 2, 12, Family::elongated, SizeCondition::free, 1003},
            {"imbalanced_5000_8_16_free", 5000, 8, 16, Family::imbalanced, SizeCondition::free, 1004},
            {"outliers_10000_8_32_free", 10000, 8, 32, Family::outliers, SizeCondition::free, 1005},
            {"duplicates_5000_4_16_free", 5000, 4, 16, Family::duplicates, SizeCondition::free, 1006},
            {"overlapping_5000_64_16_free", 5000, 64, 16, Family::overlapping, SizeCondition::free, 1007},
            {"overlapping_2000_4_10_exact", 2000, 4, 10, Family::overlapping, SizeCondition::exact, 1008},
            {"imbalanced_3000_8_12_exact", 3000, 8, 12, Family::imbalanced, SizeCondition::exact, 1009},
            {"overlapping_5000_8_20_range", 5000, 8, 20, Family::overlapping, SizeCondition::range, 1010},
            {"overlapping_10000_4_100_exact", 10000, 4, 100, Family::overlapping, SizeCondition::exact, 1011},
            {"overlapping_50000_8_32_free", 50000, 8, 32, Family::overlapping, SizeCondition::free, 1012},
            {"separated_3000_4_12_range", 3000, 4, 12, Family::separated, SizeCondition::range, 1013},
            {"overlapping_3000_4_12_range", 3000, 4, 12, Family::overlapping, SizeCondition::range, 1014},
            {"elongated_3000_2_12_range", 3000, 2, 12, Family::elongated, SizeCondition::range, 1015},
            {"imbalanced_4000_8_20_range", 4000, 8, 20, Family::imbalanced, SizeCondition::range, 1016},
            {"outliers_5000_8_16_range", 5000, 8, 16, Family::outliers, SizeCondition::range, 1017},
            {"overlapping_3000_32_12_range", 3000, 32, 12, Family::overlapping, SizeCondition::range, 1018},
            {"elongated_3000_2_12_exact", 3000, 2, 12, Family::elongated, SizeCondition::exact, 1019},
            {"outliers_5000_8_16_exact", 5000, 8, 16, Family::outliers, SizeCondition::exact, 1020},
            {"overlapping_3000_16_30_exact", 3000, 16, 30, Family::overlapping, SizeCondition::exact, 1021},
            {"overlapping_5000_4_50_exact", 5000, 4, 50, Family::overlapping, SizeCondition::exact, 1022},
            {"overlapping_10000_4_200_exact", 10000, 4, 200, Family::overlapping, SizeCondition::exact, 1023},
            {"imbalanced_6000_8_60_exact", 6000, 8, 60, Family::imbalanced, SizeCondition::exact, 1024},
            {"overlapping_6000_16_120_exact", 6000, 16, 120, Family::overlapping, SizeCondition::exact, 1025},
            {"imbalanced_12000_8_150_exact", 12000, 8, 150, Family::imbalanced, SizeCondition::exact, 1026},
            {"elongated_8000_2_160_exact", 8000, 2, 160, Family::elongated, SizeCondition::exact, 1027},
            {"outliers_12000_8_240_exact", 12000, 8, 240, Family::outliers, SizeCondition::exact, 1028}
        };
        for (const auto& spec : specs) {
            ClusteringBenchmarkCase problem = make_case(spec);
            filesystem::path path = output_directory / (spec.name + ".tcb");
            write_clustering_benchmark_case(path.string(), problem);
            cout << path.string() << '\n';
        }
    } catch (const exception& exception) {
        cerr << exception.what() << '\n';
        return 1;
    }
}
