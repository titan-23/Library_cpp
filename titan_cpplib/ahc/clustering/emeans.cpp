/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/emeans.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/kmeans_hamerly.cpp"
using namespace std;
namespace titan23 {
/// @brief emeans の設定 / 既定値は原論文と異なり、K-means++ で初期化し Hamerly 法で仕上げる
/// 参考: https://arxiv.org/abs/2611.09501
struct EMeansOptions {
    int epochs = 3;
    int batch_size = 0;               // 0 なら n, k, 次元, epochs から決める
    double alpha0 = 0;               // 0 なら n, k, バッチの点数, epochs から決める
    double revival_threshold = -1;   // -1 なら d <= 2 で 0、それ以外で 0.1
    bool kmeans_pp = true;
    int warmup_iterations = 0;       // EMA の前に行う Hamerly 法の反復の上限。0 なら行わない
    int polish_iterations = 100;     // 0 なら最後の周回の所属を返す
    uint32_t seed = 0;
};

namespace emeans_detail {
struct SquaredDistance {
    // K-means++ の初期化はこの印があると、同じ値を SIMD で求める
    static constexpr bool euclidean_squared = true;
    template <class Point, class Center>
    double operator()(const Point& point, const Center& center) const {
        double result = 0;
        for (size_t axis = 0; axis < center.size(); ++axis) {
            double difference = double(point[axis]) - double(center[axis]);
            result += difference * difference;
        }
        return result;
    }
};

struct Metric {
    // Hamerly 法はこの印があると、同じ値を SIMD で求める
    static constexpr bool euclidean_metric = true;
    template <class Point, class Center>
    double operator()(const Point& point, const Center& center) const {
        return sqrt(SquaredDistance{}(point, center));
    }
};

struct Mean {
    // 中心の計算はこの印があると、点を番号の順に 1 回だけ読んで同じ値を求める
    static constexpr bool mean_center = true;
    template <class Point>
    vector<double> operator()(const vector<Point>& points, span<const int> members) const {
        vector<double> center(points[0].size());
        for (int i : members) for (size_t axis = 0; axis < center.size(); ++axis) center[axis] += double(points[i][axis]);
        for (double& value : center) value /= members.size();
        return center;
    }
};

template <class Point>
void check_input(const vector<Point>& points, size_t k, const EMeansOptions& options) {
    kmeans_detail::check_lloyd_input(points.size(), k, 1);
    if (points[0].size() == 0) throw invalid_argument("emeans: dimension must be positive");
    for (const auto& point : points) {
        if (point.size() != points[0].size()) throw invalid_argument("emeans: dimensions must match");
        for (size_t axis = 0; axis < point.size(); ++axis) {
            if (!isfinite(double(point[axis]))) throw invalid_argument("emeans: coordinates must be finite doubles");
        }
    }
    if (options.epochs < 1 || options.batch_size < 0 || options.warmup_iterations < 0 || options.polish_iterations < 0)
        throw invalid_argument("emeans: invalid iteration count or batch size");
    if (!isfinite(options.alpha0) || options.alpha0 < 0 || options.alpha0 > 1)
        throw invalid_argument("emeans: alpha0 must be in [0, 1]");
    if (!isfinite(options.revival_threshold) || (options.revival_threshold != -1 &&
        (options.revival_threshold < 0 || options.revival_threshold > 1)))
        throw invalid_argument("emeans: revival_threshold must be -1 or in [0, 1]");
}

inline double round_power_of_two(double value) {
    return exp2(round(log2(max(1.0, value))));
}

inline int batch_size(int n, int k, size_t dimension, const EMeansOptions& options) {
    if (options.batch_size > 0) return min(n, options.batch_size);
    // 低次元では Faiss の現行の設定に合わせ、バッチを約 8 分の 1 にする
    double coefficient = dimension <= 16 ? 0.00439175 : 0.035134;
    double batch = round_power_of_two(coefficient * pow(k, 0.134749) * pow(n, 0.876473) * pow(options.epochs, 0.813344));
    double cap = dimension <= 16 ? double(n / 2) : round_power_of_two(n / 2.0);
    return int(max(1.0, min({batch, cap, double(n)})));
}

inline void revive(vector<vector<double>>& centers, vector<double>& mass, double threshold_ratio, Random& random) {
    if (threshold_ratio == 0) return;
    int k = centers.size();
    double threshold = threshold_ratio * accumulate(mass.begin(), mass.end(), 0.0) / k;
    vector<int> unused;
    for (int j = 0; j < k; ++j) if (mass[j] < threshold) unused.push_back(j);
    if (unused.empty()) return;
    vector<pair<double, int>> donors;
    donors.reserve(k - unused.size());
    for (int j = 0; j < k; ++j) if (mass[j] >= threshold && mass[j] > 0) {
        // 独立な Gumbel 鍵を使った重み付き非復元抽出
        double u = max(kmeans_detail::random_unit(random), numeric_limits<double>::min());
        donors.emplace_back(log(mass[j]) - log(-log(u)), j);
    }
    size_t count = min(unused.size(), donors.size());
    partial_sort(donors.begin(), donors.begin() + count, donors.end(), greater<pair<double, int>>{});
    for (size_t i = 0; i < count; ++i) {
        int source = donors[i].second, target = unused[i];
        mass[source] *= 0.5;
        mass[target] = mass[source];
        for (size_t axis = 0; axis < centers[source].size(); ++axis) {
            double value = centers[source][axis];
            double delta = axis % 2 == 0 ? 1e-4 : -1e-4;
            centers[source][axis] = value * (1 - delta);
            centers[target][axis] = value * (1 + delta);
        }
        if (centers[source] == centers[target]) centers[target][0] = nextafter(centers[target][0], numeric_limits<double>::infinity());
    }
}

template <class Point>
KMeansResult<vector<double>, double> run(const vector<Point>& points, vector<vector<double>> centers, const EMeansOptions& options) {
    int n = points.size(), k = centers.size();
    size_t d = points[0].size();
    int batch = batch_size(n, k, d, options);
    long long updates = (long long)options.epochs * ((n - 1) / batch + 1);
    if (updates + options.warmup_iterations + options.polish_iterations + 1 > numeric_limits<int>::max())
        throw invalid_argument("emeans: total update count must fit in int");
    int iterations = 0;
    if (options.warmup_iterations > 0) {
        auto warmup = kmeans_hamerly_from_centers(points, centers, SquaredDistance{}, Metric{}, Mean{}, options.warmup_iterations);
        if (warmup.converged) return warmup;
        iterations = warmup.iterations;
        centers = move(warmup.centers);
    }
    double alpha0 = options.alpha0;
    if (alpha0 == 0) alpha0 = clamp(0.8244 * pow(k, -0.4617) * pow(batch, 0.9003) * pow(n, -0.4498) * pow(options.epochs, -0.1057), 1e-4, 1.0);
    double revival = options.revival_threshold < 0 ? (d <= 2 ? 0 : 0.1) : options.revival_threshold;
    vector<double> mass(k, exp2(-16));
    vector<vector<double>> sums(k, vector<double>(d));
    vector<int> order(n), labels(n), counts(k);
    Random split_random(options.seed);
    SquaredDistance cost;
    kmeans_detail::Columns columns;
    vector<double> point_buffer;
    int step = 0;
    for (int epoch = 0; epoch < options.epochs; ++epoch) {
        iota(order.begin(), order.end(), 0);
        Random random(options.seed * 1000003u + uint32_t(epoch));
        random.shuffle(order);
        for (int begin = 0; begin < n;) {
            int end = begin + min(batch, n - begin);
            fill(counts.begin(), counts.end(), 0);
            for (auto& row : sums) fill(row.begin(), row.end(), 0);
            columns.assign(centers);
            for (int pos = begin; pos < end; ++pos) {
                int i = order[pos];
                kmeans_detail::load_point(points[i], d, point_buffer);
                int best = kmeans_detail::nearest_squared(point_buffer.data(), columns).first;
                labels[i] = best;
                ++counts[best];
                for (size_t axis = 0; axis < d; ++axis) sums[best][axis] += double(points[i][axis]);
            }
            double alpha = alpha0 * (0.1 + 0.45 * (1 + cos(acos(-1.0) * step / updates)));
            for (int j = 0; j < k; ++j) {
                mass[j] *= 1 - alpha;
                if (counts[j] == 0) continue;
                mass[j] += alpha * counts[j];
                double weight = alpha * counts[j] / mass[j];
                for (size_t axis = 0; axis < d; ++axis) {
                    double mean = sums[j][axis] / counts[j];
                    centers[j][axis] = weight == 1 ? mean : centers[j][axis] + weight * (mean - centers[j][axis]);
                }
            }
            if (++step % 5 == 0) revive(centers, mass, revival, split_random);
            begin = end;
        }
    }
    iterations += step;
    // 最後の周回の所属で中心を求め直す。重複点や再配置で空いたクラスタも埋める
    counts.assign(k, 0);
    for (int label : labels) ++counts[label];
    if (find(counts.begin(), counts.end(), 0) != counts.end()) {
        vector<double> assigned_cost(n);
        for (int i = 0; i < n; ++i) assigned_cost[i] = cost(points[i], centers[labels[i]]);
        kmeans_detail::repair_empty_clusters(labels, counts, assigned_cost);
    }
    vector<int> offsets, members, cursor;
    Mean mean;
    kmeans_detail::update_centers<Point, vector<double>>(points, labels, counts, offsets, members, cursor, centers, mean);
    ++iterations;
    if (options.polish_iterations > 0) {
        auto result = kmeans_hamerly_from_centers(points, centers, cost, Metric{}, mean, options.polish_iterations);
        result.iterations += iterations;
        return result;
    }
    double total_cost = kmeans_detail::calculate_total_cost(points, labels, centers, cost);
    return {move(labels), move(centers), move(counts), total_cost, iterations, false};
}
}

/// @brief 初期中心 initial_centers から e-means を行う / 中心は vector<double> で返す
template <class Point, class Center>
auto emeans_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, EMeansOptions options = {}) {
    emeans_detail::check_input(points, initial_centers.size(), options);
    vector<vector<double>> centers;
    centers.reserve(initial_centers.size());
    for (const auto& center : initial_centers) {
        if (center.size() != points[0].size()) throw invalid_argument("emeans: center dimension must match points");
        centers.emplace_back(center.size());
        for (size_t axis = 0; axis < center.size(); ++axis) {
            double value = double(center[axis]);
            if (!isfinite(value)) throw invalid_argument("emeans: centers must be finite doubles");
            centers.back()[axis] = value;
        }
    }
    return emeans_detail::run(points, move(centers), options);
}

/// @brief points を cluster_count 個のクラスタに分け、二乗距離の総和を小さくする / 1 <= cluster_count <= n
template <class Point>
auto emeans(const vector<Point>& points, int cluster_count, EMeansOptions options = {}) {
    emeans_detail::check_input(points, cluster_count, options);
    vector<vector<double>> centers;
    emeans_detail::Mean mean;
    if (options.kmeans_pp) {
        emeans_detail::SquaredDistance cost;
        centers = kmeans_detail::initialize_kmeans_pp<Point, vector<double>>(points, cluster_count, cost, mean, options.seed);
    } else {
        vector<int> indices(points.size());
        iota(indices.begin(), indices.end(), 0);
        Random random(options.seed);
        centers.reserve(cluster_count);
        for (int i = 0; i < cluster_count; ++i) {
            swap(indices[i], indices[random.randrange(i, points.size())]);
            centers.push_back(mean(points, span<const int>(&indices[i], 1)));
        }
    }
    return emeans_detail::run(points, move(centers), options);
}

/// @brief 種 seed, seed + 1, ... で trials 回 emeans を行い、費用が最小の結果を返す
template <class Point>
auto emeans_best_of(const vector<Point>& points, int cluster_count, int trials, EMeansOptions options = {}) {
    return kmeans_detail::run_best_of("emeans_best_of", trials, options, [&](const EMeansOptions& current) {
        return emeans(points, cluster_count, current);
    });
}
}
