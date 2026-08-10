# RangeSet 設計案

整数の集合を、互いに素な区間の列として `std::map` で保持するデータ構造。いわゆる「区間を set で管理するテク」をクラスに切り出す。

- 配置場所は `titan_cpplib/ds/range_set.cpp`。
- `namespace titan23`、クラス名は `RangeSet<T>`。
- 既存 `ds/` に同種の構造はない。命名は `static_set.cpp` と `std_set.cpp` の規約に合わせる。

以下に挙げた機能はすべて実装対象とする。優先度の欄は実装する順序の目安であり、対象から外す印ではない。

区間を先頭から走査すれば書けて、内部表現を知る必要のないものは載せない。区間列は `tovector()` と `for_each_range()` で取り出せるので、最長区間の取得や昇順 `k` 番目の要素は利用側で書ける。データ構造側に置くのは、`map` の探索を使って走査より速くなるものと、区間の併合条件を扱うものに絞る。

イテレータは公開しない。`map` のイテレータをそのまま渡すと内部表現に依存した書き方を許すことになる。走査は `tovector()` と `for_each_range()`、`for_each_gap()` に寄せる。

## 想定する用途

- 使用済み区間の管理。区間を追加していき、被覆した長さの合計を取る。
- mex の取得。集合に含まれない最小の値を対数時間で求める。
- 空きマスの管理。削除済みの位置を飛ばして次の空きを探す。
- 二点が同じ連結区間に属するかの判定。
- 区間集合どうしの和、積、差の計算。

## 番兵と扱う範囲

コンストラクタで `neg_inf` と `pos_inf` を受け取る。集合に入る値は `neg_inf < x < pos_inf` を満たすものに限る。区間の不変条件は `neg_inf < l < r <= pos_inf` になる。

この二値は「見つからなかった」ことを表す戻り値として使う。`optional` は使わない。

| 状況 | 戻り値 |
|---|---|
| `ge`、`gt`、`mex` が右側に見つからない | `pos_inf` |
| `le`、`lt`、`rmex` が左側に見つからない | `neg_inf` |
| `get_range`、`prev_range`、`get_gap`、`prev_gap` が見つからない | `{neg_inf, neg_inf}` |
| `next_range`、`next_gap` が見つからない | `{pos_inf, pos_inf}` |

区間を返す関数の「見つからない」は空区間で表す。保持する区間も空き区間も必ず `l < r` を満たすので、`first == second` が判定になる。番兵の値そのものと比較してもよい。

`neg_inf` と `pos_inf` は公開メンバとして持ち、利用側から読めるようにする。書き換えは想定しない。

既定値は `neg_inf = numeric_limits<T>::min()`、`pos_inf = numeric_limits<T>::max()` とする。番兵に対して足し引きをしないので、既定値のままでもオーバーフローしない。

## 内部表現

内部は `map<T, T> data` とし、キーを左端 `l`、値を右端 `r` として半開区間 `[l, r)` を持つ。

- 半開区間にする理由は、隣接区間の併合条件が `r == l'` で書けること。閉区間だと `r + 1 == l'` となり、`T` の最大値付近でオーバーフローする。
- 空区間 `l == r` は保持しない。`add(l, l)` は何もしない。
- 保持する区間は互いに素で、かつ隣接しない。つまり常に極大な区間に併合された状態を保つ。
- 要素数の合計は `long long sum_len` としてメンバに持ち、`add` と `remove` の差分で更新する。走査せずに `len()` を `O(1)` で返すため。

番兵を `map` の中にも入れて `begin()` と `end()` の場合分けを消す案は取らない。右番兵のキーを `pos_inf` に置くと、要素 `pos_inf - 1` を持つ区間の右端 `pos_inf` とキーが衝突し、併合処理が番兵を飲み込む。衝突を避けるには `pos_inf + 1` に置く必要があり、コンストラクタの引数に一単位の余裕を要求することになる。番兵は戻り値としてのみ使い、内部では `it != data.begin()` と `it != data.end()` で守る。この場合分けは実装の中に閉じるので、利用側からは見えない。

## 実装する機能

計算量の `n` は保持する区間の個数、`k` はその操作で消える区間または触れる区間の個数とする。集合演算では相手の区間数を `m` とする。

### 構築

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `RangeSet()` | 番兵を `numeric_limits<T>` の最小と最大にして空集合を構築する | `O(1)` | 高 |
| `RangeSet(T neg_inf, T pos_inf)` | 番兵を指定して空集合を構築する | `O(1)` | 高 |
| `RangeSet(const vector<pair<T, T>> &ranges, T neg_inf, T pos_inf)` | 区間列から構築する。ソートして併合する | `O(m log m)` | 中 |
| `RangeSet(const vector<T> &a, T neg_inf, T pos_inf)` | 点の列から構築する | `O(m log m)` | 中 |

区間列と点列を受ける版は、番兵を省いた二引数の形も用意する。既定の番兵で構築する。

### 更新

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `long long add(T l, T r)` | `[l, r)` を追加する。戻り値は新たに増えた要素数 | 償却 `O(log n)` | 高 |
| `bool add(T x)` | `x` を追加する。戻り値は実際に増えたかどうか | 償却 `O(log n)` | 高 |
| `long long remove(T l, T r)` | `[l, r)` を削除する。戻り値は実際に減った要素数 | 償却 `O(log n)` | 高 |
| `void remove(T x)` | `x` を削除する。含まれないとき `assert` で落とす | 償却 `O(log n)` | 高 |
| `bool discard(T x)` | `x` を削除する。含まれないとき `false` を返す | 償却 `O(log n)` | 高 |
| `void clear()` | 空にする。番兵は保持する | `O(n)` | 高 |
| `void fill()` | 扱える範囲全体を追加する。`add(neg_inf + 1, pos_inf)` と同じ | `O(n)` | 中 |
| `bool toggle(T x)` | `x` の含む含まないを反転する。戻り値は反転後に含むかどうか | 償却 `O(log n)` | 中 |
| `long long flip(T l, T r)` | `[l, r)` の含む含まないを反転する。戻り値は反転後の要素数の増分 | `O((k+1) log n)` | 中 |

`add(l, r)` の戻り値を増分にするのは、被覆した長さの合計を追跡する用途が多いため。`remove` も同じ理由で減分を返す。

`flip` は `add` と `remove` の組み合わせでは書けない。`[l, r)` と交差する区間を走査して、覆われている部分と空いている部分を入れ替える。走査した区間を消してから、集めた空き部分をまとめて挿入する。

### 判定

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `bool contains(T x) const` | `x` を含むか | `O(log n)` | 高 |
| `bool contains(T l, T r) const` | `[l, r)` 全体を含むか | `O(log n)` | 高 |
| `bool same(T x, T y) const` | `x` と `y` が同じ区間に属するか | `O(log n)` | 中 |
| `bool intersects(T l, T r) const` | `[l, r)` と交差する区間があるか | `O(log n)` | 中 |
| `bool intersects(const RangeSet &other) const` | 共通要素があるか | `O((n+m) log)` | 低 |
| `bool is_subset_of(const RangeSet &other) const` | 自身が `other` に含まれるか | `O(n log m)` | 低 |
| `bool is_superset_of(const RangeSet &other) const` | `other` を含むか | `O(m log n)` | 低 |
| `bool operator==(const RangeSet &other) const` | 同じ集合か | `O(n)` | 低 |
| `bool operator!=(const RangeSet &other) const` | 異なる集合か | `O(n)` | 低 |

`same(x, y)` は `x <= y` のとき `contains(x, y + 1)` と等価になる。薄いが、Union-Find の代用として使うときに意図が読めるので別途用意する。`y < pos_inf` なので `y + 1` はオーバーフローしない。

`operator==` は区間表現が極大に正規化されているので、`data` の一致だけで判定できる。

### 区間の取得

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `pair<T, T> get_range(T x) const` | `x` を含む区間。含まないとき `{neg_inf, neg_inf}` | `O(log n)` | 高 |
| `pair<T, T> next_range(T x) const` | `x` を含む区間、なければ `x` より右で最初の区間 | `O(log n)` | 中 |
| `pair<T, T> prev_range(T x) const` | `x` を含む区間、なければ `x` より左で最後の区間 | `O(log n)` | 中 |

### 空き区間の取得

集合に含まれない値の側を扱う。空きマスの管理で使う。

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `pair<T, T> get_gap(T x) const` | `x` を含む極大な空き区間。`x` を含むとき `{neg_inf, neg_inf}` | `O(log n)` | 中 |
| `pair<T, T> next_gap(T x) const` | `x` を含む空き区間、なければ `x` より右で最初の空き区間 | `O(log n)` | 中 |
| `pair<T, T> prev_gap(T x) const` | `x` を含む空き区間、なければ `x` より左で最後の空き区間 | `O(log n)` | 中 |
| `int gap_count() const` | `[neg_inf + 1, pos_inf)` の中の空き区間の個数 | `O(1)` | 低 |

空き区間の端は `neg_inf + 1` と `pos_inf` になりうる。どちらも実在の値なので、空区間による「見つからない」判定と衝突しない。

`gap_count()` は `range_count()` と両端が覆われているかどうかから求まる。両端の判定に `O(log n)` かかるが、`data.begin()` と `data.rbegin()` を見るだけなので `O(1)` で済む。

### 点の探索

| 関数 | 説明 | 見つからないとき | 計算量 | 優先度 |
|---|---|---|---|---|
| `T mex(T x) const` | `x` 以上で集合に含まれない最小の値 | `pos_inf` | `O(log n)` | 高 |
| `T ge(T x) const` | `x` 以上で集合に含まれる最小の要素 | `pos_inf` | `O(log n)` | 中 |
| `T gt(T x) const` | `x` より大きく集合に含まれる最小の要素 | `pos_inf` | `O(log n)` | 中 |
| `T le(T x) const` | `x` 以下で集合に含まれる最大の要素 | `neg_inf` | `O(log n)` | 中 |
| `T lt(T x) const` | `x` より小さく集合に含まれる最大の要素 | `neg_inf` | `O(log n)` | 中 |
| `T rmex(T x) const` | `x` 以下で集合に含まれない最大の値 | `neg_inf` | `O(log n)` | 中 |
| `T get_min() const` | 最小の要素 | `pos_inf` | `O(1)` | 中 |
| `T get_max() const` | 最大の要素 | `neg_inf` | `O(1)` | 中 |

`mex` はこの構造を使う主な動機の一つなので優先度を上げる。`ge` から `lt` の四つは、区間の端を返すだけで実装できる。

`mex` の戻り値は場合分けなしで番兵に収まる。`[x, pos_inf)` が完全に覆われているとき、`x` を含む区間の右端はちょうど `pos_inf` なので、そのまま返せば `pos_inf` になる。覆われていなければ右端は `pos_inf` 未満で、それが求める値になる。`rmex` も左側で同じ形になる。

`mex` の引数に既定値は与えない。`neg_inf` を指定できるので、`mex(0)` を暗黙に仮定すると誤りが出る。

### 統計

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `long long len() const` | 含まれる整数の総数 | `O(1)` | 高 |
| `long long size() const` | `len()` と同じ | `O(1)` | 高 |
| `int range_count() const` | 保持する区間の個数 | `O(1)` | 高 |
| `bool empty() const` | 空かどうか | `O(1)` | 高 |
| `long long count_range(T l, T r) const` | `[l, r)` に含まれる要素数 | `O((k+1) log n)` | 中 |

`len()` は「要素数」を返す既存の規約に合わせる。区間の個数は `range_count()` として別名にする。二つを混同すると被覆長の計算を間違えるので、名前をはっきり分ける。

`count_range` は交差する区間だけを見るので線形にならない。一方で `x` 未満の要素数や昇順 `k` 番目の要素は、左端から区間長を足す走査になる。対数にするには区間長の累積和が要るが `map` では維持できないので、これらは持たない。

### 集合演算

戻り値は新しい `RangeSet` とし、複合代入版も用意する。番兵が一致することを `assert` で確認する。

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `RangeSet operator\|(const RangeSet &other) const` | 和集合 | `O(n + m)` | 中 |
| `RangeSet operator&(const RangeSet &other) const` | 積集合 | `O(n + m)` | 中 |
| `RangeSet operator-(const RangeSet &other) const` | 差集合 | `O(n + m)` | 中 |
| `RangeSet operator^(const RangeSet &other) const` | 対称差 | `O(n + m)` | 低 |
| `RangeSet complement() const` | `[neg_inf + 1, pos_inf)` の中での補集合 | `O(n)` | 中 |
| `operator\|=`、`operator&=`、`operator-=`、`operator^=` | 複合代入版 | 同上 | 中 |

両者の区間列は昇順に並んでいるので、二本の走査で結果を作れる。結果の `map` へは末尾ヒント付きで挿入し、`O(n + m)` に収める。`add` を繰り返す実装だと `O((n + m) log)` になるので、こちらを取る。

### 列挙と出力

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `vector<pair<T, T>> tovector() const` | 区間列を昇順で返す | `O(n)` | 中 |
| `template<class F> void for_each_range(T l, T r, F &&f) const` | `[l, r)` と交差する区間を、交差部分に切り詰めて `f(a, b)` に渡す | `O((k+1) log n)` | 中 |
| `template<class F> void for_each_gap(T l, T r, F &&f) const` | `[l, r)` の中の空き区間を `f(a, b)` に渡す | `O((k+1) log n)` | 中 |
| `friend ostream& operator<<` | `others/print.cpp` に合わせた出力 | `O(n)` | 中 |

出力形式は `[[l, r), [l, r), ...]` とする。半開であることが読み取れる形にしておく。番兵を `map` に入れないので、そのまま全要素を出せる。

要素を一点ずつ列挙する関数は用意しない。要素数は座標の幅に比例しうるので、区間単位の走査に寄せる。

## 計算量の根拠

`add` は区間を高々一つ増やし、`k` 個消す。消える区間は一度挿入されたものだけなので、削除の総数は挿入の総数を超えない。よって一操作あたり償却 `O(log n)` になる。`remove` は一つの区間を二つに割ることがあり、増える区間は高々一つなので同じ議論が使える。`flip` も同様に、増える区間は消える区間の個数に対して定数個多いだけで収まる。

## 実装上の注意

- `add` と `remove` は `assert(neg_inf < l && l <= r && r <= pos_inf)` を置く。`l == r` のときは何もせず `0` を返す。
- 要素数を扱う戻り値はすべて `long long` にする。区間の個数が多いと合計が `T` に収まらない場合がある。
- 区間一つの長さ `r - l` は `T` で計算する。`pos_inf - neg_inf` が `T` に収まる範囲で番兵を指定する。既定の番兵は `T` の全域なので、`T` が `long long` 以外のときに全域を覆う使い方はしない。
- `T` は整数型を前提とする。`len()`、`count_range`、`mex` は整数でのみ意味を持つ。実数区間で使う場合はこれらを呼ばない。
- 内部処理は `add` と `remove` に集約する。`flip`、`fill`、集合演算の複合代入は、区間の併合条件を各所で書き直さず、共通の補助関数を通す。
- テストは `test/range_set/` に置く。愚直な `std::set<T>` または `vector<bool>` との比較で、更新と探索をランダム検証する。番兵の境界も範囲に含めて回す。集合演算は二つの `vector<bool>` の演算結果と突き合わせる。

## 実装順

1. 内部表現と `add`、`remove`、`discard`、`contains`
2. `mex`、`get_range`、`same`
3. `len`、`size`、`range_count`、`empty`、`count_range`、`clear`
4. `ge`、`gt`、`le`、`lt`、`rmex`、`get_min`、`get_max`
5. `tovector`、`for_each_range`、`for_each_gap`、`operator<<`
6. 空き区間の `get_gap`、`next_gap`、`prev_gap`、`gap_count`
7. `flip`、`toggle`、`fill`、`next_range`、`prev_range`
8. 集合演算と `complement`
9. `is_subset_of`、`is_superset_of`、`intersects(RangeSet)`、比較演算子
