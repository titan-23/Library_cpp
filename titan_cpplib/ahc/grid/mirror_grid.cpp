/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/grid/mirror_grid.cpp
#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <vector>
#include "titan_cpplib/ahc/grid/grid_common.cpp"
using namespace std;

namespace titan23 {

/// @brief MirrorGrid のセルの種類
enum class MirrorTile : uint8_t {
    Empty,      // 直進
    Slash,      // '/'
    Backslash,  // '\'
    Block,      // 光を吸収する
};

namespace mirror_grid_internal {

// TILE_EXIT[タイル][光が入る辺] = 光が出る辺 / 吸収する場合は -1 / 辺の順序は上、右、下、左
// タイルを追加する場合は MirrorTile, この表, TILE_CHARS に 1 つずつ要素を追加する
inline constexpr int8_t TILE_EXIT[][4] = {
    {2, 3, 0, 1},      // Empty
    {3, 2, 1, 0},      // Slash
    {1, 0, 3, 2},      // Backslash
    {-1, -1, -1, -1},  // Block
};

// 盤面の出力に使用する文字
inline constexpr char TILE_CHARS[] = {'.', '/', '\\', '#'};

inline constexpr int TILE_COUNT = sizeof(TILE_EXIT) / sizeof(TILE_EXIT[0]);
inline constexpr int STRAIGHT = 0;           // 直進のタイルで、経路の追跡では表を参照せずに進む
inline constexpr int BORDER = TILE_COUNT;    // 盤面の外周に配置する番兵
inline constexpr int HERE = TILE_COUNT + 1;  // set の実行中に、変更するセルへ設定する目印
static_assert(HERE <= 255, "tile ids must fit in uint8_t");

static_assert(
    TILE_EXIT[STRAIGHT][0] == 2 && TILE_EXIT[STRAIGHT][1] == 3 && TILE_EXIT[STRAIGHT][2] == 0 && TILE_EXIT[STRAIGHT][3] == 1,
    "tile 0 must go straight"
);

// 辺 a から入った光が辺 b から出るなら、辺 b から入った光は辺 a から出る
// 入った辺からは出ない
constexpr bool tile_table_is_valid() {
    for (int t = 0; t < TILE_COUNT; ++t) {
        for (int a = 0; a < 4; ++a) {
            int b = TILE_EXIT[t][a];
            if (b < -1 || b > 3) return false;
            if (b < 0) continue;
            if (b == a || TILE_EXIT[t][b] != a) return false;
        }
    }
    return true;
}
static_assert(tile_table_is_valid(), "TILE_EXIT must pair sides symmetrically and must not reflect back");
static_assert(sizeof(TILE_CHARS) == TILE_COUNT, "TILE_CHARS must have one character per tile");

// 番兵と目印で追跡が停止するように 2 行を追加した表
inline constexpr array<array<int8_t, 4>, TILE_COUNT + 2> EXIT = [] {
    array<array<int8_t, 4>, TILE_COUNT + 2> res{};
    for (int t = 0; t < TILE_COUNT + 2; ++t) {
        for (int a = 0; a < 4; ++a) res[t][a] = t < TILE_COUNT ? TILE_EXIT[t][a] : -1;
    }
    return res;
}();

inline constexpr int PAIRING = 0;   // 4 辺を 2 組に分けて接続する
inline constexpr int ABSORBER = 1;  // 4 辺のすべてで吸収する
inline constexpr int PARTIAL = 2;   // 一部の辺だけで吸収する

inline constexpr array<int, TILE_COUNT> KIND = [] {
    array<int, TILE_COUNT> res{};
    for (int t = 0; t < TILE_COUNT; ++t) {
        int absorbing = 0;
        for (int a = 0; a < 4; ++a) absorbing += TILE_EXIT[t][a] < 0;
        res[t] = absorbing == 0 ? PAIRING : absorbing == 4 ? ABSORBER : PARTIAL;
    }
    return res;
}();

}  // namespace mirror_grid_internal

/// @brief tile のセルに enter_side から入った光が出る辺を返す / 値は GridDirection と同じ 0 から 3 で、吸収される場合は -1
constexpr int exit_side(MirrorTile tile, GridDirection enter_side) {
    return mirror_grid_internal::EXIT[(int)tile][(int)enter_side];
}

/// @brief 光の終点 / 該当しないメンバは -1
/// 盤面の外に出た場合は port、セルに吸収された場合は row, column, side が 0 以上になる
/// 閉路を周回し続ける場合はすべてのメンバが -1 になる
struct RayEnd {
    int port = -1;    // 光が出たポートの番号
    int row = -1;     // 光を吸収したセルの行
    int column = -1;  // 光を吸収したセルの列
    int side = -1;    // 光が到達した面 / 値は GridDirection と同じ 0 から 3

    bool is_port() const { return port >= 0; }
    bool is_block() const { return row >= 0; }
    bool is_loop() const { return port < 0 && row < 0; }

    static RayEnd port_hit(int port) { return {port, -1, -1, -1}; }
    static RayEnd block_hit(int row, int column, GridDirection side) { return {-1, row, column, (int)side}; }
    static RayEnd loop() { return {-1, -1, -1, -1}; }

    bool operator==(const RayEnd&) const = default;
};

/// @brief ポートに接するセルと、そのセルでポートがある辺
struct PortPosition {
    int row, column;
    GridDirection side;
};

/// @brief ポートの一覧 / 先頭の count 件だけが有効
struct PortList {
    array<int, 4> ports{};
    int count = 0;
};

/// @brief 1 つのポートの変更前の終点と変更後の終点
struct PortChange {
    int port;
    RayEnd old_end, new_end;
};

/// @brief 終点が変化するポートの一覧 / 先頭の count 件だけが有効
struct PortDelta {
    array<PortChange, 4> changes;  // 全体を 0 で初期化すると遅いため初期化しない / count 件目以降の port は不定
    int count = 0;
};

/// @brief 鏡とブロックを配置するグリッド / 外周のポートまたは盤面内から出した光の終点と経路を求める
/// 各セルの各辺から出た光の終点を記録しているため、光の終点と、1 つのセルを変更した場合の差分は経路を追跡せずに求まる
/// ポートには上辺の左端から時計回りに 0, 1, ..., 2(H+W)-1 の番号を付ける
class MirrorGrid {
private:
    int H = 0;
    int W = 0;
    int P = 0;     // ポート数 2(H+W)
    int S = 0;     // 番兵を含む配列の 1 行の長さ W+2
    // 記録に使用する内部の終点の番号
    // 除算を使用せずに行と列を復元できるように、列を SHIFT ビットに格納する
    //   ポート p: p / セル (row,column) の面 side: P + 4*((row << SHIFT) | column) + side / 閉路: LOOP
    // end_id が利用側に返す番号は連続した番号であり、この番号とは異なる
    int SHIFT = 0;
    int LOOP = 0;
    array<int, 4> delta{};     // 上、右、下、左へ 1 セル進むときの位置の変化量
    vector<uint8_t> tiles;     // 番兵を含む (H+2)*(W+2) 個
    vector<int> far_end;       // far_end[4*位置+辺] = その辺から外へ出た光の終点の内部の番号
    vector<int> id_base;       // 盤面内: そのセルの上の面の内部の番号 / 番兵: その位置から出るポートの番号
    vector<int> port_cell;     // ポートに接するセルの位置
    vector<uint8_t> port_side_of;  // ポートに接するセルでポートがある辺

    int pos(int row, int column) const { return (row + 1) * S + (column + 1); }

    bool in_bounds(int row, int column) const {
        return 0 <= row && row < H && 0 <= column && column < W;
    }

    RayEnd decode(int id) const {
        if (id < P) return {id, -1, -1, -1};
        if (id == LOOP) return {-1, -1, -1, -1};
        int k = id - P;
        int cell = k >> 2;
        return {-1, cell >> SHIFT, cell & ((1 << SHIFT) - 1), k & 3};
    }

    // セル x の辺 enter から入った光の終点の番号を返す
    // res[辺] はセル x の内部の接続を切断したと仮定して、その辺から外向きに進んだ結果である
    // res[辺] が 0 以上なら終点の番号、負なら ~(セル x に戻る辺) を表す
    static int resolve(const int8_t *exit_of, const int *res, int own, int loop_id, int enter) {
        int cur = enter;
        while (true) {
            int e = exit_of[cur];
            if (e < 0) return own + cur;
            int r = res[e];
            if (r >= 0) return r;
            cur = ~r;
            if (cur == enter) return loop_id;
        }
    }

    // セル x の内部の接続を切断したと仮定し、辺 side から外向きに追跡する / 盤面を変更しない
    int walk_out(int x, int side) const {
        using namespace mirror_grid_internal;
        const uint8_t *tl = tiles.data();
        int move = delta[side];  // 現在の進行方向へ 1 セル進むときの位置の変化量
        int at = x + move;
        int enter = side ^ 2;
        while (true) {
            if (at == x) return ~enter;
            int t = tl[at];
            if (t == STRAIGHT) {
                // 直進では進行方向も入る辺も変化しない
                // 次の位置が表の参照結果に依存しないため高速である
                at += move;
                continue;
            }
            int e = EXIT[t][enter];
            if (e < 0) return t == BORDER ? id_base[at] : id_base[at] + enter;
            move = delta[e];
            at += move;
            enter = e ^ 2;
        }
    }

    // 終点の番号を RayEnd に変換して dst に設定する
    // 一時変数からコピーすると、書き込みの直後に幅の異なる読み出しが発生して遅いため、メンバごとに直接設定する
    void assign_end(RayEnd &dst, int id) const {
        if (id < P) {
            dst.port = id;
            dst.row = -1;
            dst.column = -1;
            dst.side = -1;
        } else if (id == LOOP) {
            dst.port = -1;
            dst.row = -1;
            dst.column = -1;
            dst.side = -1;
        } else {
            int k = id - P;
            int cell = k >> 2;
            dst.port = -1;
            dst.row = cell >> SHIFT;
            dst.column = cell & ((1 << SHIFT) - 1);
            dst.side = k & 3;
        }
    }

    void push_change(PortDelta &result, int port, int old_id, int new_id) const {
        PortChange &change = result.changes[result.count++];
        change.port = port;
        assign_end(change.old_end, old_id);
        assign_end(change.new_end, new_id);
    }

    // 経路を追跡して差分を求める / 記録だけでは終点が定まらない場合に使用する
    void preview_by_walk(int x, int own, int old_tile, int new_tile, PortDelta &result) const {
        using namespace mirror_grid_internal;
        int res[4];
        bool known[4] = {false, false, false, false};
        for (int s = 0; s < 4; ++s) {
            if (known[s]) continue;
            res[s] = walk_out(x, s);
            if (res[s] < 0) {
                res[~res[s]] = ~s;
                known[~res[s]] = true;
            }
        }
        const int *f = &far_end[4 * x];
        for (int s = 0; s < 4; ++s) {
            int port = f[s];
            if (port >= P) continue;
            bool seen = false;
            for (int j = 0; j < s; ++j) seen |= f[j] == port;
            if (seen) continue;
            // このポートから入射した光が最初に入る辺は、外向きに追跡するとこのポートに直接到達する辺である
            int entry = 0;
            while (res[entry] != port) ++entry;
            int old_id = resolve(EXIT[old_tile].data(), res, own, LOOP, entry);
            int new_id = resolve(EXIT[new_tile].data(), res, own, LOOP, entry);
            if (old_id != new_id) push_change(result, port, old_id, new_id);
        }
    }

    // セル (row, column) に辺 enter から入った状態から追跡する
    // (stop_at, stop_enter) の状態に戻った場合は閉路と判定する
    template<class F>
    RayEnd walk(int row, int column, int enter, int stop_at, int stop_enter, F &&on_cell) const {
        using namespace mirror_grid_internal;
        const uint8_t *tl = tiles.data();
        const int step[4] = {delta[0], delta[1], delta[2], delta[3]};
        int at = pos(row, column);
        // 現在の進行方向へ 1 セル進むときの位置の変化量
        // 進行方向は入った辺の反対側である
        int move = step[enter ^ 2];
        int row_move = grid_directions4[enter ^ 2].row_delta;
        int column_move = grid_directions4[enter ^ 2].column_delta;
        while (true) {
            int t = tl[at];
            if (t == STRAIGHT) {
                // 直進では進行方向も入る辺も変化しない
                on_cell(row, column, (GridDirection)enter);
            } else {
                if (t == BORDER) return RayEnd{id_base[at], -1, -1, -1};
                int e = EXIT[t][enter];
                if (e < 0) return RayEnd{-1, row, column, enter};
                on_cell(row, column, (GridDirection)enter);
                move = step[e];
                row_move = grid_directions4[e].row_delta;
                column_move = grid_directions4[e].column_delta;
                enter = e ^ 2;
            }
            at += move;
            row += row_move;
            column += column_move;
            if (at == stop_at && enter == stop_enter) return RayEnd{-1, -1, -1, -1};
        }
    }

    // 盤面の大きさから定まるメンバを設定する / タイルは番兵だけを設定し、記録は設定しない
    void init_shape(int height, int width) {
        using namespace mirror_grid_internal;
        assert(1 <= height && 1 <= width);
        int shift = 0;
        while ((1LL << shift) < width) ++shift;
        // 配列の添字と内部の終点の番号が int に収まることを、int の演算より前に確認する
        [[maybe_unused]] const long long int_max = numeric_limits<int>::max();
        assert((long long)height + 2 <= (int_max - 1) / 4 / ((long long)width + 2));
        assert(2 * ((long long)height + width) + 4 * ((long long)height << shift) < int_max);
        H = height;
        W = width;
        P = 2 * (H + W);
        S = W + 2;
        SHIFT = shift;
        LOOP = P + 4 * (H << SHIFT);
        delta = {-S, 1, S, -1};
        const int area = (H + 2) * S;
        tiles.assign(area, (uint8_t)BORDER);
        id_base.assign(area, -1);
        far_end.assign(4 * (size_t)area, -1);
        port_cell.resize(P);
        port_side_of.resize(P);
        for (int port = 0; port < P; ++port) {
            PortPosition p = port_position(port);
            int x = pos(p.row, p.column);
            port_cell[port] = x;
            port_side_of[port] = (uint8_t)p.side;
            id_base[x + delta[(int)p.side]] = port;
        }
        for (int row = 0; row < H; ++row) {
            for (int column = 0; column < W; ++column) {
                id_base[pos(row, column)] = P + 4 * ((row << SHIFT) | column);
            }
        }
    }

    // 全セルの記録を現在のタイルから作成する / O(HW)
    void build_records() {
        using namespace mirror_grid_internal;
        const uint8_t *tl = tiles.data();
        int *far = far_end.data();
        // 以下の追跡で設定されない記録は、外周のポートにも吸収する面にも到達しない光の記録である
        for (int row = 0; row < H; ++row) {
            const int begin = 4 * pos(row, 0);
            for (int i = 0; i < 4 * W; ++i) far[begin + i] = LOOP;
        }
        // 位置 at のセルに辺 enter から入った光を追跡し、光が入った辺の記録に value を設定する
        // 入った辺から外へ出た光は、同じ経路を逆向きに進んで value の終点に到達する
        auto fill_strand = [&](int at, int enter, int value) {
            int move = delta[enter ^ 2];
            while (true) {
                int t = tl[at];
                if (t == BORDER) break;
                far[4 * at + enter] = value;
                if (t == STRAIGHT) {
                    at += move;
                    continue;
                }
                int e = EXIT[t][enter];
                if (e < 0) break;
                move = delta[e];
                at += move;
                enter = e ^ 2;
            }
        };
        for (int port = 0; port < P; ++port) fill_strand(port_cell[port], port_side_of[port], port);
        for (int row = 0; row < H; ++row) {
            for (int column = 0; column < W; ++column) {
                const int x = pos(row, column);
                for (int s = 0; s < 4; ++s) {
                    if (EXIT[tl[x]][s] < 0) fill_strand(x + delta[s], s ^ 2, id_base[x] + s);
                }
            }
        }
    }

public:
    MirrorGrid() = default;

    /// @brief height*width の盤面を作成し、全セルを Empty にする / O(HW)
    MirrorGrid(int height, int width) {
        init_shape(height, width);
        for (int row = 0; row < H; ++row) {
            for (int column = 0; column < W; ++column) {
                int x = pos(row, column);
                tiles[x] = (uint8_t)MirrorTile::Empty;
                // 全セルが直進であるため、各辺から出た光はその方向にある外周のポートに到達する
                far_end[4 * x + 0] = port_at(GridDirection::Up, column);
                far_end[4 * x + 1] = port_at(GridDirection::Right, row);
                far_end[4 * x + 2] = port_at(GridDirection::Down, column);
                far_end[4 * x + 3] = port_at(GridDirection::Left, row);
            }
        }
    }

    /// @brief 文字列の配列から盤面を作成する / O(HW)
    explicit MirrorGrid(
        const vector<string> &grid,
        const string &tile_chars = string(mirror_grid_internal::TILE_CHARS, mirror_grid_internal::TILE_COUNT)
    ) {
        from_grid(grid, tile_chars);
    }

    /// @brief 文字列の配列から盤面を再作成する / 盤面の大きさは grid に合わせる / O(HW)
    /// tile_chars[t] は番号 t のタイルを表す文字で、既定では '.', '/', '\', '#' の順
    /// tile_chars に含まれない文字は Empty とする
    void from_grid(
        const vector<string> &grid,
        const string &tile_chars = string(mirror_grid_internal::TILE_CHARS, mirror_grid_internal::TILE_COUNT)
    ) {
        using namespace mirror_grid_internal;
        assert(!grid.empty() && !grid[0].empty());
        assert((int)tile_chars.size() <= TILE_COUNT);
        init_shape((int)grid.size(), (int)grid[0].size());
        array<uint8_t, 256> tile_of;
        tile_of.fill((uint8_t)MirrorTile::Empty);
        for (int t = 0; t < (int)tile_chars.size(); ++t) tile_of[(unsigned char)tile_chars[t]] = (uint8_t)t;
        for (int row = 0; row < H; ++row) {
            assert((int)grid[row].size() == W);
            for (int column = 0; column < W; ++column) {
                tiles[pos(row, column)] = tile_of[(unsigned char)grid[row][column]];
            }
        }
        build_records();
    }

    int height() const { return H; }
    int width() const { return W; }

    /// @brief ポート数 2(H+W) を返す
    int port_count() const { return P; }

    /// @brief end_id が返す値の個数を返す / end_id の値は 0 以上 end_count() 未満
    int end_count() const { return P + 4 * H * W + 1; }

    /// @brief (row,column) のタイルを返す / O(1)
    MirrorTile get(int row, int column) const {
        TITAN_GRID_ASSERT(in_bounds(row, column));
        return (MirrorTile)tiles[pos(row, column)];
    }

    /// @brief 辺 side の index 番目のポートの番号を返す / index は上辺と下辺では列番号、右辺と左辺では行番号 / O(1)
    int port_at(GridDirection side, int index) const {
        switch (side) {
            case GridDirection::Up:
                TITAN_GRID_ASSERT(0 <= index && index < W);
                return index;
            case GridDirection::Right:
                TITAN_GRID_ASSERT(0 <= index && index < H);
                return W + index;
            case GridDirection::Down:
                TITAN_GRID_ASSERT(0 <= index && index < W);
                return W + H + (W - 1 - index);
            default:
                TITAN_GRID_ASSERT(0 <= index && index < H);
                return 2 * W + H + (H - 1 - index);
        }
    }

    /// @brief ポートに接するセルと、そのセルでポートがある辺を返す / O(1)
    PortPosition port_position(int port) const {
        TITAN_GRID_ASSERT(0 <= port && port < P);
        if (port < W) return {0, port, GridDirection::Up};
        port -= W;
        if (port < H) return {port, W - 1, GridDirection::Right};
        port -= H;
        if (port < W) return {H - 1, W - 1 - port, GridDirection::Down};
        port -= W;
        return {H - 1 - port, 0, GridDirection::Left};
    }

    /// @brief 終点を 0 以上 end_count() 未満の整数に変換する / O(1)
    int end_id(const RayEnd &end) const {
        if (end.port >= 0) return end.port;
        if (end.row >= 0) return P + 4 * (end.row * W + end.column) + end.side;
        return P + 4 * H * W;
    }

    /// @brief (row,column) のタイルを tile に変更する / 変更するセルの 4 辺から延びる経路上の記録を更新する
    /// O(1 + 4 辺から外向きに追跡するセル数) / 現在と同じタイルなら O(1)
    void set(int row, int column, MirrorTile tile) {
        using namespace mirror_grid_internal;
        TITAN_GRID_ASSERT(in_bounds(row, column));
        TITAN_GRID_ASSERT((int)tile < TILE_COUNT);
        const int x = pos(row, column);
        const int new_tile = (int)tile;
        uint8_t *tl = tiles.data();
        if (tl[x] == new_tile) return;
        int *far = far_end.data();
        const int *base = id_base.data();
        const int step[4] = {delta[0], delta[1], delta[2], delta[3]};
        const int own = base[x];

        // 手順 1: セル x の内部の接続を切断したと仮定した場合の、4 辺それぞれの到達先を求める
        // res[辺] が 0 以上なら終点の番号、負なら ~(セル x に戻る辺) を表す
        // 多くの場合は現在の記録から求まり、その根拠は preview と同じである
        // 記録から求まらない場合だけ外向きに追跡する
        int res[4];
        {
            const int *f = far + 4 * x;
            const int old_tile = tl[x];
            const int old_kind = KIND[old_tile];
            int loops = 0;
            bool has_own = false;
            for (int s = 0; s < 4; ++s) {
                loops += f[s] == LOOP;
                has_own |= (unsigned)(f[s] - own) < 4u;
            }
            const bool has_same = f[0] == f[1] || f[0] == f[2] || f[0] == f[3]
                               || f[1] == f[2] || f[1] == f[3] || f[2] == f[3];
            if (loops == 0 && !has_own && !has_same) {
                // セル x に戻る辺がないため、記録の値が到達先である
                for (int s = 0; s < 4; ++s) res[s] = f[s];
            } else if (old_kind == ABSORBER) {
                // 記録がセル x 自身の面である辺がセル x に戻る辺であり、戻り先はその面の辺である
                for (int s = 0; s < 4; ++s) res[s] = (unsigned)(f[s] - own) < 4u ? ~(f[s] - own) : f[s];
            } else if (old_kind == PAIRING && loops == 2) {
                // 記録が閉路である 2 辺が互いにセル x に戻る辺である
                // 残りの 2 辺は記録の値が到達先である
                int pair_sum = 0;
                for (int s = 0; s < 4; ++s) if (f[s] == LOOP) pair_sum += s;
                for (int s = 0; s < 4; ++s) res[s] = f[s] == LOOP ? ~(pair_sum - s) : f[s];
            } else {
                // セル x に目印を設定すると、セル x に戻った経路の追跡も表の参照だけで停止する
                tl[x] = (uint8_t)HERE;
                bool known[4] = {false, false, false, false};
                for (int s = 0; s < 4; ++s) {
                    if (known[s]) continue;
                    int move = step[s];  // 現在の進行方向へ 1 セル進むときの位置の変化量
                    int at = x + move;
                    int enter = s ^ 2;
                    int t;
                    while (true) {
                        t = tl[at];
                        if (t == STRAIGHT) {
                            // 直進では進行方向も入る辺も変化しない
                            // 次の位置が表の参照結果に依存しないため高速である
                            at += move;
                            continue;
                        }
                        int e = EXIT[t][enter];
                        if (e < 0) break;
                        move = step[e];
                        at += move;
                        enter = e ^ 2;
                    }
                    if (t == HERE) {
                        // 別の辺 enter からセル x に戻った場合である
                        // 辺 enter から出た結果は同じ経路の逆向きになる
                        res[s] = ~enter;
                        res[enter] = ~s;
                        known[enter] = true;
                    } else if (t == BORDER) {
                        res[s] = base[at];
                    } else {
                        res[s] = base[at] + enter;
                    }
                }
            }
        }

        // 手順 2: 新しいタイルを適用し、各辺からセル x に入った光の終点を求める
        const int8_t *new_exit = EXIT[new_tile].data();
        int in_end[4];
        for (int s = 0; s < 4; ++s) in_end[s] = resolve(new_exit, res, own, LOOP, s);

        // 手順 3, 4: 4 辺の経路を追跡して記録を更新する
        for (int s = 0; s < 4; ++s) {
            const int first = x + step[s];
            if (res[s] >= 0) {
                // 外周のポートかブロックの面で終了する経路である
                // セル x へ向かう側の辺の記録だけが変化する
                if (tl[first] == BORDER) continue;
                const int value = in_end[s];
                if (far[4 * first + (s ^ 2)] == value) continue;  // この経路上の記録は変化しない
                int move = step[s];
                int at = first;
                int enter = s ^ 2;
                while (true) {
                    int t = tl[at];
                    if (t == STRAIGHT) {
                        far[4 * at + enter] = value;
                        at += move;
                        continue;
                    }
                    if (t == BORDER) break;
                    far[4 * at + enter] = value;  // 経路の光を吸収したブロックの面にも設定する
                    int e = EXIT[t][enter];
                    if (e < 0) break;
                    move = step[e];
                    at += move;
                    enter = e ^ 2;
                }
            } else {
                // セル x に戻る経路である
                // 対になる 2 辺のうち番号が小さい辺から 1 回だけ追跡する
                const int other = ~res[s];
                if (other < s) continue;
                const int value = in_end[s];
                const int other_value = in_end[other];
                const int last = x + step[other];
                if (far[4 * first + (s ^ 2)] == value && far[4 * last + (other ^ 2)] == other_value) continue;
                int move = step[s];
                int at = first;
                int enter = s ^ 2;
                while (at != x) {
                    int t = tl[at];
                    if (t == STRAIGHT) {
                        far[4 * at + enter] = value;
                        far[4 * at + (enter ^ 2)] = other_value;
                        at += move;
                        continue;
                    }
                    int e = EXIT[t][enter];  // セル x に戻る経路上に光を吸収するセルは存在しない
                    far[4 * at + enter] = value;
                    far[4 * at + e] = other_value;
                    move = step[e];
                    at += move;
                    enter = e ^ 2;
                }
            }
        }

        // 手順 5: セル x 自身の 4 辺の記録を更新する
        for (int s = 0; s < 4; ++s) far[4 * x + s] = res[s] >= 0 ? res[s] : in_end[~res[s]];
        tl[x] = (uint8_t)new_tile;
    }

    /// @brief ポート port から入射した光の終点を返す / O(1)
    RayEnd trace_port(int port) const {
        using namespace mirror_grid_internal;
        TITAN_GRID_ASSERT(0 <= port && port < P);
        int x = port_cell[port];
        int side = port_side_of[port];
        int e = EXIT[tiles[x]][side];
        return decode(e < 0 ? id_base[x] + side : far_end[4 * x + e]);
    }

    /// @brief ポート port から入射した光の終点を返す / 通過したセルごとに on_cell(row, column, enter_side) を呼び出す
    /// 光を吸収したセルでは呼び出さない / O(1 + 経路長)
    template<class F>
    RayEnd trace_port(int port, F &&on_cell) const {
        TITAN_GRID_ASSERT(0 <= port && port < P);
        PortPosition p = port_position(port);
        return walk(p.row, p.column, (int)p.side, -1, -1, on_cell);
    }

    /// @brief (row,column) の辺 side から外へ出た光の終点を返す / O(1)
    RayEnd trace_from(int row, int column, GridDirection side) const {
        TITAN_GRID_ASSERT(in_bounds(row, column));
        return decode(far_end[4 * pos(row, column) + (int)side]);
    }

    /// @brief (row,column) の辺 side から外へ出た光の終点を返す / 通過したセルごとに on_cell(row, column, enter_side) を呼び出す
    /// 出発時の (row,column) では呼び出さない
    /// 光が (row,column) に戻って通過するときは呼び出す / O(1 + 経路長)
    template<class F>
    RayEnd trace_from(int row, int column, GridDirection side, F &&on_cell) const {
        TITAN_GRID_ASSERT(in_bounds(row, column));
        int s = (int)side;
        int next_row = row + grid_directions4[s].row_delta;
        int next_column = column + grid_directions4[s].column_delta;
        return walk(next_row, next_column, s ^ 2, pos(next_row, next_column), s ^ 2, on_cell);
    }

    /// @brief 全ポートの終点をポート番号順に返す / O(P)
    vector<RayEnd> trace_all_ports() const {
        vector<RayEnd> ends(P);
        for (int port = 0; port < P; ++port) ends[port] = trace_port(port);
        return ends;
    }

    /// @brief 光が (row,column) に入る外周のポートを返す / 最大 4 個 / O(1)
    /// 4 辺の記録を上、右、下、左の順に参照し、初めて出現した順に格納する
    PortList ports_through(int row, int column) const {
        TITAN_GRID_ASSERT(in_bounds(row, column));
        PortList result;
        const int *f = &far_end[4 * pos(row, column)];
        // 分岐予測が外れやすいため、採否に関係なく値を格納し、採用する場合だけ件数を加算する
        int count = 0;
        for (int s = 0; s < 4; ++s) {
            int port = f[s];
            bool keep = port < P;
            for (int j = 0; j < s; ++j) keep &= f[j] != port;
            result.ports[count] = port;
            count += keep;
        }
        for (int i = count; i < 4; ++i) result.ports[i] = 0;
        result.count = count;
        return result;
    }

    /// @brief 盤面を変更せずに、(row,column) のタイルを tile に変更した場合に終点が変化する外周のポートを返す / 最大 4 個
    /// O(1) / 記録から終点が定まらない場合だけ 4 辺から外向きに追跡する
    PortDelta preview(int row, int column, MirrorTile tile) const {
        using namespace mirror_grid_internal;
        TITAN_GRID_ASSERT(in_bounds(row, column));
        TITAN_GRID_ASSERT((int)tile < TILE_COUNT);
        PortDelta result;
        const int x = pos(row, column);
        const int old_tile = tiles[x];
        const int new_tile = (int)tile;
        if (old_tile == new_tile) return result;
        const int *f = &far_end[4 * x];
        const int own = id_base[x];
        const int8_t *old_exit = EXIT[old_tile].data();
        const int8_t *new_exit = EXIT[new_tile].data();
        const int f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3];

        // 1. 4 辺の記録がすべて異なり、いずれも閉路でもセル x 自身の面でもなければ、セル x に戻る辺はない
        //    この場合の記録はその辺から直接到達する終点であるため、4 辺を新しいタイルで再接続する
        const bool has_loop = f0 == LOOP || f1 == LOOP || f2 == LOOP || f3 == LOOP;
        const bool has_own = (unsigned)(f0 - own) < 4u || (unsigned)(f1 - own) < 4u
                          || (unsigned)(f2 - own) < 4u || (unsigned)(f3 - own) < 4u;
        const bool has_same = f0 == f1 || f0 == f2 || f0 == f3 || f1 == f2 || f1 == f3 || f2 == f3;
        if (!has_loop && !has_own && !has_same) {
            for (int s = 0; s < 4; ++s) {
                int port = f[s];
                if (port >= P) continue;
                int old_e = old_exit[s];
                int new_e = new_exit[s];
                int old_id = old_e < 0 ? own + s : f[old_e];
                int new_id = new_e < 0 ? own + s : f[new_e];
                if (old_id != new_id) push_change(result, port, old_id, new_id);
            }
            return result;
        }

        const int old_kind = KIND[old_tile];
        const int new_kind = KIND[new_tile];
        if (old_kind == ABSORBER) {
            // 2. 記録がセル x 自身の面である辺がセル x に戻る辺であり、戻り先はその面の辺である
            int res[4];
            for (int s = 0; s < 4; ++s) res[s] = (unsigned)(f[s] - own) < 4u ? ~(f[s] - own) : f[s];
            for (int s = 0; s < 4; ++s) {
                int port = f[s];
                if (port >= P) continue;
                int new_id = resolve(new_exit, res, own, LOOP, s);
                if (new_id != own + s) push_change(result, port, own + s, new_id);
            }
            return result;
        }
        if (old_kind == PAIRING && new_kind == PAIRING) {
            // 3. セル x に戻る辺がある場合、4 辺をどのように接続してもセル x に入る光の終点は変化しない
            return result;
        }
        if (old_kind == PAIRING && new_kind == ABSORBER && has_loop) {
            // 4. 記録が閉路である 2 辺が互いにセル x に戻る辺である
            //    残りの 2 辺から入る光はセル x の面で吸収される
            for (int s = 0; s < 4; ++s) {
                int port = f[s];
                if (port >= P) continue;
                push_change(result, port, f[old_exit[s]], own + s);
            }
            return result;
        }
        // 残りは同じ光がセル x を 2 回通過する場合と、一部の辺だけで吸収するタイルの場合である
        // これらの場合は記録から終点が定まらないため経路を追跡する
        preview_by_walk(x, own, old_tile, new_tile, result);
        return result;
    }

    /// @brief 盤面を H 行の文字列で出力する / Empty は '.', Slash は '/', Backslash は '\', Block は '#'
    friend ostream &operator<<(ostream &os, const MirrorGrid &grid) {
        for (int row = 0; row < grid.H; ++row) {
            for (int column = 0; column < grid.W; ++column) {
                os << mirror_grid_internal::TILE_CHARS[grid.tiles[grid.pos(row, column)]];
            }
            os << '\n';
        }
        return os;
    }
};

}  // namespace titan23
