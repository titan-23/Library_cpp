/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/test/manhattan_nearest_neighbor_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include "titan_cpplib/geometry/manhattan_nearest_neighbor.cpp"

using namespace std;
using namespace titan23;

using Tree = ManhattanNearest<long long>;
using Point = Tree::Point;

long long absolute(long long value) {
    return value < 0 ? -value : value;
}

long long distance(const Point& p, long long x, long long y) {
    return absolute(p.x - x) + absolute(p.y - y);
}

bool in_direction(const Point& p, long long x, long long y, int direction) {
    if (direction == Tree::NE) return p.x >= x && p.y >= y;
    if (direction == Tree::NW) return p.x <= x && p.y >= y;
    if (direction == Tree::SW) return p.x <= x && p.y <= y;
    return p.x >= x && p.y <= y;
}

array<int, 4> brute_four(const vector<Point>& points, const vector<int>& cnt, long long x, long long y) {
    array<int, 4> answer = {-1, -1, -1, -1};
    array<long long, 4> best{};
    int n = points.size();
    for (int direction = 0; direction < 4; ++direction) {
        for (int i = 0; i < n; ++i) {
            if (cnt[i] == 0 || !in_direction(points[i], x, y, direction)) continue;
            long long d = distance(points[i], x, y);
            if (answer[direction] == -1 || d < best[direction] ||
                (d == best[direction] && i < answer[direction])) {
                answer[direction] = i;
                best[direction] = d;
            }
        }
    }
    return answer;
}

int brute_nearest(const vector<Point>& points, const vector<int>& cnt, long long x, long long y) {
    int answer = -1;
    long long best = 0;
    int n = points.size();
    for (int i = 0; i < n; ++i) {
        if (cnt[i] == 0) continue;
        long long d = distance(points[i], x, y);
        if (answer == -1 || d < best || (d == best && i < answer)) {
            answer = i;
            best = d;
        }
    }
    return answer;
}

void test_basic() {
    vector<Point> points = {
        {3, 4}, {-2, 5}, {-4, -3}, {5, -2}, {2, 2}, {-1, 1},
    };
    Tree tree(points);

    assert(tree.nearest(0, 0) == -1);
    assert(tree.nearest_dist(0, 0) == -1);
    assert((tree.nearest_four(0, 0) == array<int, 4>{-1, -1, -1, -1}));

    tree.add(0);
    tree.add(1);
    tree.add(2);
    tree.add(3);
    assert((tree.nearest_four(0, 0) == array<int, 4>{0, 1, 2, 3}));
    assert(tree.nearest(0, 0) == 0);
    assert(tree.nearest_dist(0, 0) == 7);

    tree.add(4);
    tree.add(5);
    assert((tree.nearest_four(0, 0) == array<int, 4>{4, 5, 2, 3}));
    assert(tree.nearest(0, 0) == 5);
    assert(tree.nearest_dist(0, 0) == 2);

    // 同じIDを再度追加すると個数が増える
    tree.add(5);
    assert(tree.nearest(0, 0) == 5);

    tree.remove(5);
    assert((tree.nearest_four(0, 0) == array<int, 4>{4, 5, 2, 3}));
    assert(tree.nearest(0, 0) == 5);
    tree.remove(5);
    assert((tree.nearest_four(0, 0) == array<int, 4>{4, 1, 2, 3}));
    assert(tree.nearest(0, 0) == 4);
    tree.remove(5);
    assert(tree.nearest(0, 0) == 4);

    tree.add(5);
    assert(tree.nearest(0, 0) == 5);

    tree.remove(0);
    tree.remove(5);
    tree.add_all();
    assert((tree.nearest_four(0, 0) == array<int, 4>{4, 5, 2, 3}));
    assert(tree.nearest(0, 0) == 5);

    tree.remove(5);
    assert(tree.nearest(0, 0) == 4);
    tree.add(5);
    assert(tree.nearest(0, 0) == 5);
}

void test_id_multiset() {
    Tree tree({{2, 0}, {9, 0}});
    tree.add(0);
    tree.add(0);
    tree.remove(0);
    assert(tree.nearest(0, 0) == 0);
    assert(tree.nearest_dist(0, 0) == 2);
    tree.remove(0);
    assert(tree.nearest(0, 0) == -1);

    tree.add_all();
    tree.add(0);
    tree.add_all();
    tree.remove(0);
    assert(tree.nearest(0, 0) == 0);
    tree.remove(0);
    assert(tree.nearest(0, 0) == 1);
}

void test_closed_boundaries_and_ties() {
    vector<Point> points = {
        {0, 0}, {0, 0}, {0, 4}, {0, -4}, {4, 0}, {-4, 0}, {1, 1}, {-1, 1},
    };
    Tree tree(points);
    tree.add_all();

    // クエリと同じ座標の点は4方向すべてに入り、同距離ではIDが小さい
    assert((tree.nearest_four(0, 0) == array<int, 4>{0, 0, 0, 0}));
    assert(tree.nearest(0, 0) == 0);

    // NE と NW の候補が同距離なら全体でもIDが小さい方を返す
    Tree tied({{1, 1}, {-1, 1}});
    tied.add_all();
    assert((tied.nearest_four(0, 0) == array<int, 4>{0, 1, -1, -1}));
    assert(tied.nearest(0, 0) == 0);
}

void test_large_coordinates() {
    constexpr long long L = 2000000000000000000LL;
    vector<Point> points = {
        {-L, -L},
        {L, L},
        {-L, L},
        {L, -L},
    };
    Tree tree(points);
    tree.add_all();

    assert((tree.nearest_four(0, 0) == array<int, 4>{1, 2, 0, 3}));
    assert(tree.nearest(L, L) == 1);
    assert(tree.nearest(-L, -L) == 0);
}

void test_coordinate_template() {
    using IntTree = ManhattanNearest<int>;
    vector<IntTree::Point> points = {{-5, 2}, {3, 4}, {1, -7}};
    IntTree tree(points);
    tree.add_all();
    assert(tree.nearest(0, 0) == 0);
    tree.remove(0);
    assert(tree.nearest(0, 0) == 1);
}

void test_invalid_input() {
    Tree tree({{0, 0}});
    bool failed = false;
    try {
        tree.add(-1);
    } catch (const out_of_range&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        tree.remove(1);
    } catch (const out_of_range&) {
        failed = true;
    }
    assert(failed);
}

void test_random() {
    mt19937_64 rng(23);
    constexpr int n = 240;
    vector<Point> points;
    points.reserve(n);
    for (int i = 0; i < n; ++i) {
        long long x = static_cast<long long>(rng() % 81) - 40;
        long long y = static_cast<long long>(rng() % 81) - 40;
        points.push_back({x, y});
    }
    shuffle(points.begin(), points.end(), rng);

    Tree tree(points);
    vector<int> cnt(n, 0);
    vector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    shuffle(order.begin(), order.end(), rng);

    for (int step = 0; step <= n; ++step) {
        if (step > 0) {
            int index = order[step - 1];
            tree.add(index);
            ++cnt[index];
        }
        if (step < 30 || step % 5 == 0) {
            for (int trial = 0; trial < 20; ++trial) {
                long long x = static_cast<long long>(rng() % 121) - 60;
                long long y = static_cast<long long>(rng() % 121) - 60;
                assert(tree.nearest_four(x, y) == brute_four(points, cnt, x, y));
                assert(tree.nearest(x, y) == brute_nearest(points, cnt, x, y));
            }
        }
    }
}

void test_random_add_remove() {
    mt19937_64 rng(20250308);
    constexpr int n = 180;
    vector<Point> points;
    points.reserve(n);
    for (int i = 0; i < n; ++i) {
        long long x = static_cast<long long>(rng() % 41) - 20;
        long long y = static_cast<long long>(rng() % 41) - 20;
        points.push_back({x, y});
    }

    Tree tree(points);
    vector<int> cnt(n, 0);
    for (int step = 0; step < 3000; ++step) {
        int index = rng() % n;
        if (rng() & 1) {
            tree.add(index);
            ++cnt[index];
        } else {
            tree.remove(index);
            if (cnt[index] > 0) --cnt[index];
        }

        for (int trial = 0; trial < 3; ++trial) {
            long long x = static_cast<long long>(rng() % 81) - 40;
            long long y = static_cast<long long>(rng() % 81) - 40;
            assert(tree.nearest_four(x, y) == brute_four(points, cnt, x, y));
            assert(tree.nearest(x, y) == brute_nearest(points, cnt, x, y));
        }
    }
}

int main() {
    test_basic();
    test_id_multiset();
    test_closed_boundaries_and_ties();
    test_large_coordinates();
    test_coordinate_template();
    test_invalid_input();
    test_random();
    test_random_add_remove();
    cout << "Manhattan nearest neighbor tests: OK\n";
}
