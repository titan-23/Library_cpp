# グリッドUtil 設計案

二次元グリッドを扱う問題で繰り返し書く処理を `Grid` クラスにまとめる。配置場所は `titan_cpplib/others/grid.cpp`、`namespace titan23` とする。

## 設計方針

- 盤面は `Grid` クラスが保持する。関数に `const vector<string> &g` を渡し回す形は取らない。探索・走査・変換はすべてメンバ関数にする。
- 盤面の実体は `vector<vector<T>>` とする。文字盤は `Grid<char>`、値盤は `Grid<int>` などを使う。`Grid<char>` でも `g(r, c) == '#'` の比較がそのまま書ける。
- 座標は `(r, c)` の順で統一する。行 `r`、列 `c`。差分は `dr/dc` と呼び、既存 `format.cpp` の `dy/dx` とは別系統にする。
- 範囲外判定はクラス内で行う。利用側で `isrange` を書かせない。`H`、`W` はメンバに持つ。
- 近傍列挙はコールバック方式を基本にする。ホットループでの `vector` 確保を避ける。
- 探索は「通行可能か」を判定する述語で受ける。壁文字に依存しない。述語を省略した場合の既定は用意する。
- 盤面変換は新しい `Grid` を返す。元の盤面は変更しない。

## クラス構造

```cpp
template<class T = char>
class Grid {
public:
    int H, W;
private:
    vector<vector<T>> G;
public:
    Grid() : H(0), W(0) {}
    Grid(int h, int w, T init = T()) : H(h), W(w), G(h, vector<T>(w, init)) {}
};
```

文字盤と値盤を同じクラスで扱う。`T` の既定を `char` にして、最頻の文字盤を `Grid<>` で書けるようにする。

## 要素アクセス

```cpp
T& operator()(int r, int c);              // G[r][c]
const T& operator()(int r, int c) const;
bool inside(int r, int c) const;          // 0<=r<H && 0<=c<W
```

`operator()` で読み書きする。`operator[]` は行参照を返す案もあるが、二次元アクセスを `(r, c)` に統一する。

## 入力と構築

静的ファクトリで標準入力から作る。

```cpp
static Grid<char> read(int h);            // h 行の文字盤
static Grid<int>  read_int(int h, int w); // 整数盤
```

## 方向定数

クラス外に置く。4近傍と8近傍の差分を持つ。順序は上下左右に固定する。

```cpp
constexpr int dr4[4] = {-1, 1, 0, 0};
constexpr int dc4[4] = { 0, 0, -1, 1};
constexpr int dr8[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
constexpr int dc8[8] = { 0, 0, -1, 1, -1, 1, -1, 1};
```

移動文字との対応も持つ。下が `r` 増加なので `D` は `dr=+1` とする。

```cpp
pair<int,int> dir_of(char c);   // 'U'->{-1,0} 'D'->{1,0} 'L'->{0,-1} 'R'->{0,1}
char char_of(int dr, int dc);   // 逆変換
```

## 近傍列挙

メンバのコールバック版を基本とする。盤面内の隣接マスだけを渡す。

```cpp
template<class F> void each4(int r, int c, F f) const;   // 盤面内のみ f(nr, nc)
template<class F> void each8(int r, int c, F f) const;
```

方向番号も渡す版を用意する。最短経路の復元で使う。

```cpp
template<class F> void each4_idx(int r, int c, F f) const;  // f(dir, nr, nc) dir は 0..3
```

利便用に列挙版を置く。デバッグや非ホットパス向け。

```cpp
vector<pair<int,int>> neighbors4(int r, int c) const;
vector<pair<int,int>> neighbors8(int r, int c) const;
```

## 通行判定

探索は述語 `bool(int r, int c)` で受ける。文字盤の標準形をメンバで作る。

```cpp
auto passable_except(T wall) const;   // wall 以外を通行可とする述語を返す
auto passable_only(T road) const;     // road のみ通行可とする述語を返す
```

述語は `*this` を捕捉する。利用側で独自条件も書ける。

## 探索

距離は `-1` を未到達とする。多始点を基本にする。単一始点は `{{sr, sc}}` で渡す。述語を省略すると全マス通行可で扱う。

```cpp
// 重みなし最短距離
template<class P>
vector<vector<int>> bfs(const vector<pair<int,int>> &starts, P passable) const;
```

経路復元のために、各マスへ来た方向を返す版も置く。

```cpp
// dist と prev_dir を返す。prev_dir(r,c) は r,c に入った向き。始点と未到達は -1
template<class P>
pair<vector<vector<int>>, vector<vector<int>>>
bfs_with_prev(const vector<pair<int,int>> &starts, P passable) const;

// 復元結果を始点->終点の (r,c) 列で返す
vector<pair<int,int>> restore_path(const vector<vector<int>> &prev_dir, int tr, int tc) const;
```

## 連結成分

通行可マスをラベル付けする。

```cpp
// 各マスの成分番号と成分数を返す。通行不可は -1
template<class P>
pair<vector<vector<int>>, int> connected_components(P passable, bool diag = false) const;

// 指定マスから到達できる集合を返す
template<class P>
vector<pair<int,int>> flood_fill(int sr, int sc, P passable) const;
```

`diag` で8近傍に切り替える。連結判定は成分番号の一致で行う。

## 盤面変換

回転と反転は新しい `Grid` を返す。

```cpp
Grid<T> rotate90() const;    // 右90度
Grid<T> rotate180() const;
Grid<T> rotate270() const;
Grid<T> transpose() const;
Grid<T> flip_h() const;      // 左右反転
Grid<T> flip_v() const;      // 上下反転
```

## 走査と探索補助

全マスや条件付きの走査を置く。始点探索で使う。

```cpp
template<class F> pair<int,int> find_cell(F f) const;   // f(r,c) 真の最初のマス。無ければ {-1,-1}
template<class F> int count_cell(F f) const;            // 条件を満たすマス数
template<class F> void each_cell(F f) const;            // 全マスに f(r, c)
vector<pair<int,int>> cells_of(T value) const;          // value のマスを全列挙
```

## マスの性質

座標から定まる属性を取る。市松模様や境界の判定で使う。

```cpp
int color(int r, int c) const;     // 市松。(r + c) % 2
bool is_border(int r, int c) const; // 外周マスか
```

斜め方向のグループ番号を取る。同じ対角線上のマスを同一視する問題で使う。ビショップの移動や斜め一直線の判定が該当する。

```cpp
int diag(int r, int c) const;       // 右下がり対角線。r - c + (W - 1) で 0 起点
int anti_diag(int r, int c) const;  // 右上がり対角線。r + c
```

## 直線移動

ある向きへ進む処理を置く。ボールが転がる、光線が当たるまで進む類の問題で使う。

```cpp
// (r,c) から (dr,dc) 方向へ、述語 stop が真になる手前まで進んだ到達点を返す
template<class P> pair<int,int> slide(int r, int c, int dr, int dc, P stop) const;

// (r,c) から (dr,dc) 方向の盤面内マスを順に f(nr, nc) へ渡す
template<class F> void ray(int r, int c, int dr, int dc, F f) const;
```

## 対称変換

回転と反転を組み合わせた8通りの盤面を生成する。パズルの一致判定で使う。

```cpp
vector<Grid<T>> transforms() const;   // 回転4種 × 反転で重複を除いた最大8通り
```

## 座標ユーティリティ

距離計算をクラス外の自由関数で置く。

```cpp
long long manhattan(int r1, int c1, int r2, int c2);  // |r1-r2| + |c1-c2|
long long chebyshev(int r1, int c1, int r2, int c2);  // max(|r1-r2|, |c1-c2|)
```

## 出力

デバッグ表示を置く。`others/print.cpp` と整合させる。

```cpp
friend ostream& operator<<(ostream &os, const Grid<T> &grid);  // 行ごとに出力
```

## 利用例

壁 `#`、道 `.`、始点 `S`、終点 `G` の盤面で最短手数を求める。

```cpp
auto grid = Grid<>::read(h);
auto [sr, sc] = grid.find_cell([&](int r, int c){ return grid(r, c) == 'S'; });
auto [tr, tc] = grid.find_cell([&](int r, int c){ return grid(r, c) == 'G'; });
auto dist = grid.bfs({{sr, sc}}, grid.passable_except('#'));
cout << dist[tr][tc] << '\n';   // 未到達なら -1
```

## 実装順の目安

- 第1段。アクセス、`inside`、`read`、方向定数、`each4`、`bfs`、`connected_components`、`find_cell`、`cells_of`。出題頻度が高い。
- 第2段。`bfs_with_prev`、`restore_path`、`flood_fill`。
- 第3段。盤面変換、`transforms`、`slide`/`ray`、対角線・市松、距離関数、`dir_of`/`char_of`、`operator<<`。

## 検討事項

- 述語を返す `passable_except` は `*this` を参照で捕捉する。`Grid` の寿命内で使う前提とする。
- `Grid<char>` の内部表現を `vector<string>` にすると入出力は短いが、`vector<vector<T>>` との二系統になる。本案は `vector<vector<T>>` に統一する。`read` 内で文字列から変換する。
