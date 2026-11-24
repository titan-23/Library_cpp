# RangeSet 設計案

このデータ構造は、整数の集合を互いに素な区間の列として `std::map` で保持します。いわゆる「区間を set で管理するテク」をクラスとして実装します。

- 配置場所は `titan_cpplib/ds/range_set.cpp` です。
- `namespace titan23` で定義し、クラス名は `RangeSet<T>` とします。
- 既存の `ds/` に同種の構造はありません。命名は `static_set.cpp` と `std_set.cpp` の規約に合わせます。

以下に挙げた機能は、優先度が低いものも含めてすべて実装します。優先度の欄は実装する順序の目安です。

内部表現を知らなくても区間を先頭から走査すれば実装できる機能は含めません。区間列は `tovector()` と `for_each_range()` で取得できるため、最長区間の取得や昇順で `k` 番目の要素の取得は利用側で実装できます。データ構造に含める機能は、`map` の探索を利用すると走査より高速になるものと、区間の併合条件を扱うものに限ります。

イテレータは公開しません。`map` のイテレータをそのまま渡すと、利用側が内部表現に依存したコードを記述できるようになるためです。走査には `tovector()`、`for_each_range()`、`for_each_gap()` を使用します。

## 想定する用途

- 使用済み区間を管理し、区間を順に追加しながら被覆した長さの合計を求めます。
- 集合に含まれない最小の値 (mex) を対数時間で求めます。
- 空きマスを管理し、削除済みの位置をスキップして次の空きマスを探します。
- 二点が同じ連結区間に属するかどうかを判定します。
- 区間の集合どうしの和集合、積集合、差集合を計算します。

## 番兵と扱う範囲

コンストラクタで `neg_inf` と `pos_inf` を受け取ります。集合の要素は `neg_inf < x < pos_inf` を満たす値に限ります。区間の不変条件は `neg_inf < l < r <= pos_inf` です。

これらの番兵は、値や区間が見つからなかったことを表す戻り値として使用します。`optional` は使用しません。

| 状況 | 戻り値 |
|---|---|
| `ge`、`gt`、`mex` で右側に値が見つからない | `pos_inf` |
| `le`、`lt`、`rmex` で左側に値が見つからない | `neg_inf` |
| `get_range`、`prev_range`、`get_gap`、`prev_gap` で区間が見つからない | `{neg_inf, neg_inf}` |
| `next_range`、`next_gap` で区間が見つからない | `{pos_inf, pos_inf}` |

区間を返す関数は、区間が見つからない場合に空区間を返します。保持する区間も空き区間も必ず `l < r` を満たすため、`first == second` で見つからなかったことを判定できます。番兵の値と直接比較して判定することもできます。

`neg_inf` と `pos_inf` は公開メンバとし、利用側から参照できるようにします。利用側が値を変更することは想定しません。

既定値は `neg_inf = numeric_limits<T>::min()`、`pos_inf = numeric_limits<T>::max()` とします。番兵に対して加算や減算をしないため、既定値のままでもオーバーフローしません。

## 内部表現

内部表現は `map<T, T> data` とし、キーを左端 `l`、値を右端 `r` として半開区間 `[l, r)` を保持します。

- 半開区間にするのは、隣接区間の併合条件を `r == l'` と記述できるためです。閉区間の場合は `r + 1 == l'` となり、`T` の最大値付近でオーバーフローします。
- 空区間 `l == r` は保持しません。`add(l, l)` は何もしません。
- 保持する区間は互いに素で、かつ隣接しません。つまり、区間は常に極大な区間に併合された状態に維持されます。
- 要素数の合計は `long long sum_len` としてメンバに保持し、`add` と `remove` で増減した要素数を反映して更新します。これにより、走査せずに `len()` を `O(1)` で返すことができます。

番兵を `map` の中にも挿入して `begin()` と `end()` の場合分けをなくす案は採用しません。右側の番兵のキーを `pos_inf` にすると、要素 `pos_inf - 1` を含む区間の右端 `pos_inf` とキーが衝突し、併合処理で番兵がその区間に併合されます。衝突を避けるにはキーを `pos_inf + 1` にする必要があり、その場合はコンストラクタの引数に一単位の余裕を求めることになります。番兵は戻り値としてのみ使用し、内部では `it != data.begin()` と `it != data.end()` で範囲外の参照を防ぎます。この場合分けは実装の内部だけで行うため、利用側には現れません。

## 実装する機能

計算量の `n` は保持する区間の個数、`k` はその操作で削除または走査する区間の個数とします。集合演算では、もう一方の集合の区間数を `m` とします。

### 構築

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `RangeSet()` | 番兵を `numeric_limits<T>` の最小と最大にして空集合を構築する | `O(1)` | 高 |
| `RangeSet(T neg_inf, T pos_inf)` | 番兵を指定して空集合を構築する | `O(1)` | 高 |
| `RangeSet(const vector<pair<T, T>> &ranges, T neg_inf, T pos_inf)` | 区間列をソートして併合し、集合を構築する | `O(m log m)` | 中 |
| `RangeSet(const vector<T> &a, T neg_inf, T pos_inf)` | 点の列から構築する | `O(m log m)` | 中 |

区間列と点列を受け取るコンストラクタには、番兵を省略した二引数の形も用意します。この形では既定の番兵で構築します。

### 更新

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `long long add(T l, T r)` | `[l, r)` を追加し、新たに増えた要素数を返す | 償却 `O(log n)` | 高 |
| `bool add(T x)` | `x` を追加し、要素が実際に増えたかどうかを返す | 償却 `O(log n)` | 高 |
| `long long remove(T l, T r)` | `[l, r)` を削除し、実際に減った要素数を返す | 償却 `O(log n)` | 高 |
| `void remove(T x)` | `x` を削除し、含まれない場合は `assert` で異常終了する | 償却 `O(log n)` | 高 |
| `bool discard(T x)` | `x` を削除し、含まれない場合は `false` を返す | 償却 `O(log n)` | 高 |
| `void clear()` | 番兵を保持したまま空にする | `O(n)` | 高 |
| `void fill()` | `add(neg_inf + 1, pos_inf)` と同じ処理で、扱う範囲全体を追加する | `O(n)` | 中 |
| `bool toggle(T x)` | `x` を含むかどうかを反転し、反転後に含むかどうかを返す | 償却 `O(log n)` | 中 |
| `long long flip(T l, T r)` | `[l, r)` の各値を含むかどうかを反転し、反転による要素数の増分を返す | `O((k+1) log n)` | 中 |

`add(l, r)` の戻り値を増分にするのは、被覆した長さの合計を追跡する用途が多いためです。`remove` も同じ理由で減少分を返します。

`flip` は `add` と `remove` の組み合わせでは実装できません。`[l, r)` と交差する区間を走査し、覆われている部分と空いている部分を入れ替えます。走査した区間を削除してから、収集した空き部分をまとめて挿入します。

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

`same(x, y)` は `x <= y` の場合に `contains(x, y + 1)` と等価です。処理は単純ですが、Union-Find の代わりに使用する場合に意図が伝わりやすいため、別途用意します。`y < pos_inf` であるため、`y + 1` はオーバーフローしません。

区間表現は極大な区間に正規化されているため、`operator==` は `data` が一致するかどうかだけで判定できます。

### 区間の取得

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `pair<T, T> get_range(T x) const` | `x` を含む区間で、なければ `{neg_inf, neg_inf}` | `O(log n)` | 高 |
| `pair<T, T> next_range(T x) const` | `x` を含む区間、なければ `x` より右で最初の区間 | `O(log n)` | 中 |
| `pair<T, T> prev_range(T x) const` | `x` を含む区間、なければ `x` より左で最後の区間 | `O(log n)` | 中 |

### 空き区間の取得

集合に含まれない値からなる区間を扱います。空きマスの管理で使用します。

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `pair<T, T> get_gap(T x) const` | `x` を含む極大な空き区間で、集合が `x` を含む場合は `{neg_inf, neg_inf}` | `O(log n)` | 中 |
| `pair<T, T> next_gap(T x) const` | `x` を含む空き区間、なければ `x` より右で最初の空き区間 | `O(log n)` | 中 |
| `pair<T, T> prev_gap(T x) const` | `x` を含む空き区間、なければ `x` より左で最後の空き区間 | `O(log n)` | 中 |
| `int gap_count() const` | `[neg_inf + 1, pos_inf)` の中の空き区間の個数 | `O(1)` | 低 |

空き区間の端は `neg_inf + 1` や `pos_inf` になる場合があります。どちらも区間の端として実際に取り得る値であるため、見つからないことを空区間で判定する方法とは衝突しません。

`gap_count()` は、範囲の両端が覆われているかどうかと `range_count()` から求めます。両端の判定に `O(log n)` かかりますが、`data.begin()` と `data.rbegin()` を参照するだけなので `O(1)` で判定できます。

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

`mex` はこのデータ構造を使用する主な動機の一つであるため、優先度を高くします。`ge` から `lt` までの四つの関数は、区間の端を返すだけで実装できます。

`mex` は場合分けをせずに、見つからない場合の戻り値を番兵にすることができます。`[x, pos_inf)` が完全に覆われている場合、`x` を含む区間の右端はちょうど `pos_inf` であるため、右端をそのまま返すと `pos_inf` になります。覆われていない場合は右端が `pos_inf` 未満であり、その右端が求める値です。`rmex` も左側について同様です。

`mex` の引数には既定値を設定しません。`neg_inf` は利用側が指定できるため、`mex(0)` を暗黙に仮定すると誤りの原因になります。

### 統計

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `long long len() const` | 含まれる整数の総数 | `O(1)` | 高 |
| `long long size() const` | `len()` と同じ | `O(1)` | 高 |
| `int range_count() const` | 保持する区間の個数 | `O(1)` | 高 |
| `bool empty() const` | 空かどうか | `O(1)` | 高 |
| `long long count_range(T l, T r) const` | `[l, r)` に含まれる要素数 | `O((k+1) log n)` | 中 |

`len()` は、要素数を返すという既存の規約に合わせます。区間の個数は別の関数 `range_count()` で返します。両者を混同すると被覆長の計算で誤りが生じるため、名前を明確に区別します。

`count_range` は交差する区間だけを走査するため、線形時間にはなりません。一方、`x` 未満の要素数や昇順で `k` 番目の要素を求めるには、左端から区間長を加算する走査が必要です。対数時間にするには区間長の累積和が必要ですが、`map` では累積和を維持できないため、これらの機能は用意しません。

### 集合演算

戻り値は新しい `RangeSet` とし、複合代入の演算子も用意します。両者の番兵が一致することを `assert` で確認します。

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `RangeSet operator\|(const RangeSet &other) const` | 和集合 | `O(n + m)` | 中 |
| `RangeSet operator&(const RangeSet &other) const` | 積集合 | `O(n + m)` | 中 |
| `RangeSet operator-(const RangeSet &other) const` | 差集合 | `O(n + m)` | 中 |
| `RangeSet operator^(const RangeSet &other) const` | 対称差 | `O(n + m)` | 低 |
| `RangeSet complement() const` | `[neg_inf + 1, pos_inf)` の中での補集合 | `O(n)` | 中 |
| `operator\|=`、`operator&=`、`operator-=`、`operator^=` | 複合代入版 | 同上 | 中 |

両者の区間列は昇順にソートされているため、二つの列を先頭から同時に走査して結果を作成できます。結果の `map` には末尾をヒントに指定して挿入し、計算量を `O(n + m)` に抑えます。`add` を繰り返す実装では `O((n + m) log)` になるため、この方法を採用します。

### 列挙と出力

| 関数 | 説明 | 計算量 | 優先度 |
|---|---|---|---|
| `vector<pair<T, T>> tovector() const` | 区間列を昇順で返す | `O(n)` | 中 |
| `template<class F> void for_each_range(T l, T r, F &&f) const` | `[l, r)` と交差する区間を、交差部分に切り詰めて `f(a, b)` に渡す | `O((k+1) log n)` | 中 |
| `template<class F> void for_each_gap(T l, T r, F &&f) const` | `[l, r)` の中の空き区間を `f(a, b)` に渡す | `O((k+1) log n)` | 中 |
| `friend ostream& operator<<` | `others/print.cpp` に合わせた出力 | `O(n)` | 中 |

出力形式は `[[l, r), [l, r), ...]` とし、半開区間であることが分かる形にします。番兵を `map` に含めないため、全要素をそのまま出力できます。

要素を一点ずつ列挙する関数は用意しません。要素数は座標の幅に比例する場合があるため、走査は区間単位で行います。

## 計算量の根拠

`add` は区間を高々一つ増やし、`k` 個削除します。削除される区間は過去に挿入された区間だけであるため、削除の総数は挿入の総数を超えません。よって、一操作あたり償却 `O(log n)` です。`remove` は一つの区間を二つに分割する場合がありますが、増える区間は高々一つであるため、同じ議論が成り立ちます。`flip` も同様に、増える区間の個数は、削除される区間の個数に定数を加えた範囲に収まります。

## 実装上の注意

- `add` と `remove` には `assert(neg_inf < l && l <= r && r <= pos_inf)` を記述します。`l == r` の場合は何もせずに `0` を返します。
- 要素数を表す戻り値はすべて `long long` とします。区間の個数が多いと、合計が `T` に収まらない場合があるためです。
- 各区間の長さ `r - l` は `T` で計算します。番兵は `pos_inf - neg_inf` が `T` に収まる範囲で指定します。既定の番兵は `T` の全域であるため、`T` が `long long` 以外の場合は全域を覆う区間を使用しないようにします。
- `T` は整数型を前提とします。`len()`、`count_range`、`mex` は整数の場合にのみ意味があります。実数の区間で使用する場合は、これらの関数を呼び出さないようにします。
- 内部処理は `add` と `remove` に集約します。`flip`、`fill`、集合演算の複合代入では、区間の併合条件を各所で個別に記述せず、共通の補助関数を使用します。
- テストは `test/range_set/` に配置します。更新と探索は、ランダムな入力に対する結果を、愚直な `std::set<T>` または `vector<bool>` による実装の結果と比較して検証します。番兵の境界もテストの範囲に含めます。集合演算は、二つの `vector<bool>` の演算結果と比較します。

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
