/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/test/edge_wall_bitboard_test.cpp
#include <bits/stdc++.h>
#ifdef EDGE_WALL_BITBOARD_TEST_DEBUG
#define TITAN_DEBUG
#endif
#include "titan_cpplib/ahc/grid/edge_wall_bitboard.cpp"
using namespace std;
using namespace titan23;

constexpr array<GridDirection, 4> directions = {
    GridDirection::Up,
    GridDirection::Right,
    GridDirection::Down,
    GridDirection::Left,
};

struct ScalarGrid {
    int height;
    int width;
    vector<string> right_walls;
    vector<string> down_walls;

    int destination(int index, GridDirection direction) const {
        int row = index / width;
        int column = index - row * width;
        switch (direction) {
            case GridDirection::Up:
                return row > 0 && down_walls[row - 1][column] == '0' ? index - width : index;
            case GridDirection::Right:
                return column + 1 < width && right_walls[row][column] == '0' ? index + 1 : index;
            case GridDirection::Down:
                return row + 1 < height && down_walls[row][column] == '0' ? index + width : index;
            case GridDirection::Left:
                return column > 0 && right_walls[row][column - 1] == '0' ? index - 1 : index;
        }
        assert(false);
        return index;
    }

    vector<int> bfs(const vector<char>& sources) const {
        int cell_count = height * width;
        vector<int> distance(cell_count, -1);
        vector<int> queue;
        queue.reserve(cell_count);
        for (int index = 0; index < cell_count; ++index) {
            if (!sources[index]) continue;
            distance[index] = 0;
            queue.push_back(index);
        }
        for (int head = 0; head < (int)queue.size(); ++head) {
            int current = queue[head];
            for (GridDirection direction : directions) {
                int following = destination(current, direction);
                if (following == current || distance[following] != -1) continue;
                distance[following] = distance[current] + 1;
                queue.push_back(following);
            }
        }
        return distance;
    }
};

template<class Board>
void assert_set_equals(const Board& board, const typename Board::Set& actual, const vector<char>& expected) {
    assert((int)expected.size() == board.cell_count());
    int count = 0;
    for (int row = 0; row < board.height(); ++row) {
        for (int column = 0; column < board.width(); ++column) {
            bool value = board.test(actual, row, column);
            assert(value == (bool)expected[row * board.width() + column]);
            count += value;
        }
    }
    assert(board.count(actual) == count);
}

template<class Board>
typename Board::Set make_set(const Board& board, const vector<char>& values) {
    vector<pair<int, int>> cells;
    for (int index = 0; index < (int)values.size(); ++index) {
        if (values[index]) cells.emplace_back(index / board.width(), index % board.width());
    }
    auto result = board.new_set();
    board.make_set(cells, result);
    return result;
}

template<class Board>
void check_set_operations(const Board& board, mt19937& random) {
    int cell_count = board.cell_count();
    vector<char> first(cell_count), second(cell_count);
    for (int index = 0; index < cell_count; ++index) {
        first[index] = random() % 3 == 0;
        second[index] = random() % 3 == 0;
    }
    auto a = make_set(board, first);
    auto b = make_set(board, second);
    auto out = board.new_set();
    vector<char> expected(cell_count);

    for (int index = 0; index < cell_count; ++index) expected[index] = first[index] && second[index];
    board.band(a, b, out);
    assert_set_equals(board, out, expected);

    for (int index = 0; index < cell_count; ++index) expected[index] = first[index] || second[index];
    board.bor(a, b, out);
    assert_set_equals(board, out, expected);

    for (int index = 0; index < cell_count; ++index) expected[index] = first[index] != second[index];
    board.bxor(a, b, out);
    assert_set_equals(board, out, expected);

    for (int index = 0; index < cell_count; ++index) expected[index] = first[index] && !second[index];
    board.bdiff(a, b, out);
    assert_set_equals(board, out, expected);

    for (int index = 0; index < cell_count; ++index) expected[index] = !first[index];
    board.complement_into(a, out);
    assert_set_equals(board, out, expected);
}

template<class Board>
void check_case(
    int height,
    int width,
    const vector<string>& right_walls,
    const vector<string>& down_walls,
    mt19937& random
) {
    Board board(height, width);
    board.from_walls(right_walls, down_walls);
    ScalarGrid scalar{height, width, right_walls, down_walls};
    int cell_count = height * width;

    for (int index = 0; index < cell_count; ++index) {
        int row = index / width;
        int column = index - row * width;
        for (GridDirection direction : directions) {
            int expected = scalar.destination(index, direction);
            assert(board.destination_index(index, direction) == expected);
            assert(board.can_move(row, column, direction) == (expected != index));
        }
    }

    check_set_operations(board, random);

    for (int trial = 0; trial < 8; ++trial) {
        vector<char> sources(cell_count);
        for (char& value : sources) value = random() % 5 == 0;
        if (none_of(sources.begin(), sources.end(), [](char value) { return value != 0; })) {
            sources[random() % cell_count] = true;
        }
        auto source_set = make_set(board, sources);
        auto actual = board.new_set();

        for (GridDirection direction : directions) {
            vector<char> successful(cell_count), advanced(cell_count), uncertain = sources;
            for (int index = 0; index < cell_count; ++index) {
                if (!sources[index]) continue;
                int following = scalar.destination(index, direction);
                if (following != index) successful[following] = true;
                advanced[following] = true;
                uncertain[following] = true;
            }
            board.move_success(source_set, direction, actual);
            assert_set_equals(board, actual, successful);
            board.advance(source_set, direction, actual);
            assert_set_equals(board, actual, advanced);
            board.advance_uncertain(source_set, direction, actual);
            assert_set_equals(board, actual, uncertain);
        }

        vector<char> expanded(cell_count);
        for (int index = 0; index < cell_count; ++index) {
            if (!sources[index]) continue;
            for (GridDirection direction : directions) {
                int following = scalar.destination(index, direction);
                if (following != index) expanded[following] = true;
            }
        }
        board.expand(source_set, actual);
        assert_set_equals(board, actual, expanded);

        vector<int> expected_distance = scalar.bfs(sources);
        vector<char> reached(cell_count);
        for (int index = 0; index < cell_count; ++index) reached[index] = expected_distance[index] != -1;
        board.flood(source_set, actual);
        assert_set_equals(board, actual, reached);

        vector<int> actual_distance;
        board.bfs_dist(source_set, actual_distance);
        assert(actual_distance == expected_distance);
    }

    for (int trial = 0; trial < 20; ++trial) {
        int start = (int)(random() % (uint32_t)cell_count);
        int goal = (int)(random() % (uint32_t)cell_count);
        vector<char> source(cell_count);
        source[start] = true;
        vector<int> distance = scalar.bfs(source);
        vector<pair<int, int>> path;
        bool found = board.shortest_path(
            {start / width, start % width},
            {goal / width, goal % width},
            path
        );
        assert(found == (distance[goal] != -1));
        if (!found) continue;
        assert((int)path.size() == distance[goal] + 1);
        assert((path.front() == pair<int, int>(start / width, start % width)));
        assert((path.back() == pair<int, int>(goal / width, goal % width)));
        for (int index = 1; index < (int)path.size(); ++index) {
            int before = path[index - 1].first * width + path[index - 1].second;
            int after = path[index].first * width + path[index].second;
            bool adjacent = false;
            for (GridDirection direction : directions) {
                adjacent |= scalar.destination(before, direction) == after && before != after;
            }
            assert(adjacent);
        }
    }
}

template<class Board>
void check_random(int max_width, mt19937& random) {
    for (int trial = 0; trial < 120; ++trial) {
        int height = 1 + (int)(random() % 12);
        int width = 1 + (int)(random() % (uint32_t)max_width);
        vector<string> right_walls(height, string(max(0, width - 1), '0'));
        vector<string> down_walls(max(0, height - 1), string(width, '0'));
        for (string& row : right_walls) {
            for (char& wall : row) wall = random() % 100 < 35 ? '1' : '0';
        }
        for (string& row : down_walls) {
            for (char& wall : row) wall = random() % 100 < 35 ? '1' : '0';
        }
        check_case<Board>(height, width, right_walls, down_walls, random);
    }
}

template<class Board>
void check_boundary_width(int width, mt19937& random) {
    int height = 3;
    vector<string> right_walls(height, string(width - 1, '1'));
    vector<string> down_walls(height - 1, string(width, '1'));
    right_walls[1][width - 2] = '0';
    down_walls[1][width - 1] = '0';
    check_case<Board>(height, width, right_walls, down_walls, random);
}

int main() {
    mt19937 random(23);

    EdgeWallBitboard64 empty(0, 0);
    empty.from_walls({}, {});
    auto empty_set = empty.new_set();
    auto empty_result = empty.new_set();
    empty.expand(empty_set, empty_result);
    empty.flood(empty_set, empty_result);
    vector<int> empty_distance;
    empty.bfs_dist(empty_set, empty_distance);
    assert(empty_distance.empty());

    check_case<EdgeWallBitboard64>(1, 1, {""}, {}, random);
    check_case<EdgeWallBitboard64>(1, 64, {string(63, '0')}, {}, random);
    check_case<EdgeWallBitboard64>(64, 1, vector<string>(64, ""), vector<string>(63, "0"), random);
    check_random<EdgeWallBitboard64>(64, random);
    check_random<EdgeWallBitboard128>(128, random);
    check_boundary_width<EdgeWallBitboard64>(64, random);
    check_boundary_width<EdgeWallBitboard128>(128, random);

    cout << "ok\n";
}
