# グリッド Util の設計案

二次元グリッドを扱う問題で繰り返し記述する処理を `Grid` クラスにまとめます。配置するファイルは `titan_cpplib/others/grid.cpp` とし、`namespace titan23` に定義します。

## 設計方針

- 盤面は `Grid` クラスが保持します。関数を呼び出すたびに `const vector<string> &g` を渡す形式は採用しません。探索、走査、変換はすべてメンバ関数にします。
- 盤面の実体は `vector<vector<T>>` とします。文字の盤面は `Grid<char>`、数値の盤面は `Grid<int>` などを使用します。`Grid<char>` でも `g(r, c) == '#'` の形式で直接比較することができます。
- 座標は `(r, c)` の順に統一し、`r` は行、`c` は列を表します。差分は `dr/dc` と呼び、既存の `format.cpp` で使用している `dy/dx` とは区別します。
- 範囲外の判定はクラス内で行い、利用側で `isrange` を記述する必要がないようにします。`H`, `W` はメンバ変数として保持します。
- 近傍の列挙はコールバック方式を基本にします。ホットループでの `vector` の確保を避けるようにします。
- 探索関数は通行可能かを判定する述語を引数に取ります。壁を表す文字には依存しません。述語を省略した場合の既定の述語も用意します。
- 盤面の変換は新しい `Grid` を返し、元の盤面は変更しません。

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

文字の盤面と数値の盤面を同じクラスで扱います。使用頻度が最も高い文字の盤面を `Grid<>` と記述できるように、`T` の既定の型を `char` にします。

## 要素アクセス

```cpp
T& operator()(int r, int c);              // G[r][c]
const T& operator()(int r, int c) const;
bool inside(int r, int c) const;          // 0<=r<H && 0<=c<W
```

要素の参照と更新は `operator()` で行います。`operator[]` で行の参照を返す案もありますが、二次元のアクセスは `(r, c)` の形式に統一します。

## 入力と構築

静的ファクトリメソッドで標準入力から盤面を構築します。

```cpp
static Grid<char> read(int h);            // h 行の文字の盤面
static Grid<int>  read_int(int h, int w); // 整数の盤面
```

## 方向定数

4 近傍と 8 近傍の差分を表す定数をクラスの外に定義します。順序は上下左右に固定します。

```cpp
constexpr int dr4[4] = {-1, 1, 0, 0};
constexpr int dc4[4] = { 0, 0, -1, 1};
constexpr int dr8[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
constexpr int dc8[8] = { 0, 0, -1, 1, -1, 1, -1, 1};
```

移動を表す文字との対応も定義します。下方向が `r` の増加に対応するため、`D` は `dr=+1` とします。

```cpp
pair<int,int> dir_of(char c);   // 'U'->{-1,0} 'D'->{1,0} 'L'->{0,-1} 'R'->{0,1}
char char_of(int dr, int dc);   // 逆変換
```

## 近傍列挙

コールバックを受け取るメンバ関数を基本とします。コールバックには盤面内の隣接マスだけを渡します。

```cpp
template<class F> void each4(int r, int c, F f) const;   // 盤面内のマスに対してだけ f(nr, nc) を呼び出す
template<class F> void each8(int r, int c, F f) const;
```

方向番号もコールバックに渡す版を用意します。最短経路の復元で使用します。

```cpp
template<class F> void each4_idx(int r, int c, F f) const;  // f(dir, nr, nc) を呼び出す / dir は 0..3
```

利便性のために、隣接マスの一覧を返す版も用意します。デバッグや、ホットパス以外の処理での使用を想定しています。

```cpp
vector<pair<int,int>> neighbors4(int r, int c) const;
vector<pair<int,int>> neighbors8(int r, int c) const;
```

## 通行判定

探索関数は述語 `bool(int r, int c)` を引数に取ります。文字の盤面で標準的に使用する述語は、メンバ関数で作成します。

```cpp
auto passable_except(T wall) const;   // wall 以外を通行可とする述語を返す
auto passable_only(T road) const;     // road のみ通行可とする述語を返す
```

これらの述語は `*this` をキャプチャします。利用側で独自の条件を記述することもできます。

## 探索

距離の `-1` は未到達を表します。始点は複数を受け取る形式を基本とし、単一の始点は `{{sr, sc}}` で渡します。述語を省略した場合はすべてのマスを通行可能として扱います。

```cpp
// 重みなし最短距離
template<class P>
vector<vector<int>> bfs(const vector<pair<int,int>> &starts, P passable) const;
```

経路復元のために、各マスに到達したときの方向も返す版を用意します。

```cpp
// dist と prev_dir を返す
// prev_dir(r,c) は r,c に入ったときの向きで、始点と未到達のマスは -1
template<class P>
pair<vector<vector<int>>, vector<vector<int>>>
bfs_with_prev(const vector<pair<int,int>> &starts, P passable) const;

// 復元した経路を始点から終点の順に (r,c) の配列で返す
vector<pair<int,int>> restore_path(const vector<vector<int>> &prev_dir, int tr, int tc) const;
```

## 連結成分

通行可能なマスをラベル付けします。

```cpp
// 各マスの成分番号と成分数を返す
// 通行できないマスの成分番号は -1
template<class P>
pair<vector<vector<int>>, int> connected_components(P passable, bool diag = false) const;

// 指定したマスから到達できるマスの集合を返す
template<class P>
vector<pair<int,int>> flood_fill(int sr, int sc, P passable) const;
```

`diag` を真にすると 8 近傍に切り替えます。連結の判定は成分番号が一致するかで行います。

## 盤面変換

回転と反転の関数は新しい `Grid` を返します。

```cpp
Grid<T> rotate90() const;    // 右回りに 90 度
Grid<T> rotate180() const;
Grid<T> rotate270() const;
Grid<T> transpose() const;
Grid<T> flip_h() const;      // 左右反転
Grid<T> flip_v() const;      // 上下反転
```

## 走査と探索補助

全マスの走査と、条件を満たすマスの走査を用意します。始点の探索で使用します。

```cpp
template<class F> pair<int,int> find_cell(F f) const;   // f(r,c) が真になる最初のマスを返す / なければ {-1,-1}
template<class F> int count_cell(F f) const;            // 条件を満たすマス数
template<class F> void each_cell(F f) const;            // 全マスで f(r, c) を呼び出す
vector<pair<int,int>> cells_of(T value) const;          // 値が value のマスをすべて列挙する
```

## マスの性質

座標から定まるマスの属性を求めます。市松模様や境界の判定で使用します。

```cpp
int color(int r, int c) const;     // 市松模様の色 / (r + c) % 2
bool is_border(int r, int c) const; // 外周のマスかを返す
```

マスが属する対角線の番号を求めます。同じ対角線上のマスを同一視する問題で使用します。ビショップの移動や、斜めに一直線に並ぶかの判定が該当します。

```cpp
int diag(int r, int c) const;       // 右下がりの対角線の番号 / r - c + (W - 1) で 0 始まり
int anti_diag(int r, int c) const;  // 右上がりの対角線の番号 / r + c
```

## 直線移動

ある向きへ直線的に進む処理を用意します。ボールが転がる問題や、光線が当たるまで進む問題などで使用します。

```cpp
// (r,c) から (dr,dc) 方向へ、述語 stop が真になる手前まで進んだ到達点を返す
template<class P> pair<int,int> slide(int r, int c, int dr, int dc, P stop) const;

// (r,c) から (dr,dc) 方向にある盤面内のマスを順に f(nr, nc) へ渡す
template<class F> void ray(int r, int c, int dr, int dc, F f) const;
```

## 対称変換

回転と反転を組み合わせた 8 通りの盤面を生成します。パズルの一致判定で使用します。

```cpp
vector<Grid<T>> transforms() const;   // 4 種類の回転と反転の組み合わせから重複を除いた最大 8 通り
```

## 座標ユーティリティ

距離の計算はクラス外の自由関数として定義します。

```cpp
long long manhattan(int r1, int c1, int r2, int c2);  // |r1-r2| + |c1-c2|
long long chebyshev(int r1, int c1, int r2, int c2);  // max(|r1-r2|, |c1-c2|)
```

## 出力

デバッグ用の出力を用意し、`others/print.cpp` と整合させます。

```cpp
friend ostream& operator<<(ostream &os, const Grid<T> &grid);  // 行ごとに出力
```

## 利用例

壁 `#`、道 `.`、始点 `S`、終点 `G` の盤面で最短手数を求めます。

```cpp
auto grid = Grid<>::read(h);
auto [sr, sc] = grid.find_cell([&](int r, int c){ return grid(r, c) == 'S'; });
auto [tr, tc] = grid.find_cell([&](int r, int c){ return grid(r, c) == 'G'; });
auto dist = grid.bfs({{sr, sc}}, grid.passable_except('#'));
cout << dist[tr][tc] << '\n';   // 未到達なら -1
```

## 実装順の目安

- 第 1 段は要素アクセス、`inside`、`read`、方向定数、`each4`、`bfs`、`connected_components`、`find_cell`、`cells_of` です。これらは出題頻度が高い処理です。
- 第 2 段は `bfs_with_prev`、`restore_path`、`flood_fill` です。
- 第 3 段は盤面変換、`transforms`、`slide`/`ray`、対角線と市松模様、距離関数、`dir_of`/`char_of`、`operator<<` です。

## 検討事項

- 述語を返す `passable_except` は `*this` を参照でキャプチャします。返された述語は `Grid` の寿命内で使用することを前提とします。
- `Grid<char>` の内部表現を `vector<string>` にすると入出力の記述は短くなりますが、`vector<vector<T>>` と合わせて二系統の内部表現を扱うことになります。本案は `vector<vector<T>>` に統一し、`read` の中で文字列から変換します。
