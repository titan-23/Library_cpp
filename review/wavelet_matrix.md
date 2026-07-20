# Wavelet Matrix 系データ構造の整理と実装状況

## 1. 目的

この文書では、リポジトリに存在する Wavelet Matrix / Wavelet Tree 系の実装を整理し、次の点をまとめる。

- 各実装の現在の役割
- 既存APIの差異
- 総和・重み付きクエリ
- 静的版・動的版へ追加した機能
- 追加前に整理したい仕様・境界条件
- 推奨するクラス構成と実装順序

本稿では、配列中の検索対象となる値を「キー」、集約対象を「重み」と呼ぶ。値自身の総和を求める場合は、各要素について `weight = key` とすればよい。

以下では、

- `n`: 配列長
- `sigma`: キーの値域の上端。キーは原則 `[0, sigma)`
- `L`: `ceil(log2(sigma))` 相当のビット数
- `D`: 対象区間に存在する相異なるキーの種類数

とする。

## 2. 既存実装の一覧

| ファイル | クラス | 主な用途 | 現在の主な機能 |
|---|---|---|---|
| [`wavelet_matrix.cpp`](../titan_cpplib/ds/wavelet_matrix.cpp) | `WaveletMatrix<T>` | 通常の静的な配列 | 通常の検索API、majority、値域内の元配列位置探索 |
| [`wavelet_matrix_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_sum.cpp) | `WaveletMatrixSum<T, W>` | 総和付きの静的な配列 | 通常の検索API、値域総和、昇順・降順先頭 `k` 個の総和、目標和へ達する最小個数 |
| [`wavelet_matrix_fenwick.cpp`](../titan_cpplib/ds/wavelet_matrix_fenwick.cpp) | `WaveletMatrixFenwick<T, W>` | キー固定・重み更新可能な配列 | 静的総和版と共通のAPI、`set_weight`, `add_weight` |
| [`wavelet_matrix_bit.cpp`](../titan_cpplib/ds/wavelet_matrix_bit.cpp) | `WaveletMatrix<T, log>` | ビット幅をコンパイル時に固定した静的版 | 通常版とほぼ同じ検索API |
| [`wavelet_matrix_2d_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_sum.cpp) | `WaveletMatrix2DSum<T, W>` | オフライン2次元点集合の重み総和 | 長方形和、個数、`kth_y`、昇順・降順先頭 `k` 点の総和 |
| [`wavelet_matrix_2d_min.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_min.cpp) | `WaveletMatrix2DMin<T, W>` | オフライン2次元点集合の最小値・最大値 | `range_min/max`, `range_argmin/argmax` |
| [`wavelet_matrix_2d_monoid.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp) | `WaveletMatrix2DMonoid<T, S, op, e>` | オフライン2次元点集合の可換モノイド積 | 長方形内の `range_prod` |
| [`dynamic_wavelet_matrix.cpp`](../titan_cpplib/ds/dynamic_wavelet_matrix.cpp) | `DynamicWaveletMatrix<T>` | 各ビットレベルに動的ビット列を持つ可変長配列 | 挿入、削除、更新、順位・頻度検索、`topk`, majority |
| [`dynamic_wavelet_tree.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree.cpp) | `DynamicWaveletTree<T>` | キーのprefixごとにNodeを持つ可変長配列 | 挿入、削除、更新、順位・頻度検索、majority |
| [`dynamic_wavelet_tree_sum.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp) | `DynamicWaveletTreeSum<T, W>` | 総和付きの可変長配列 | 挿入、削除、更新、検索API、静的総和版と共通の総和API |

### 2.1 通常の静的 `WaveletMatrix`

一般的な1次元配列の Wavelet Matrix である。検索系APIに加え、`has_majority`, `range_select`, `tovector` と値域内の元配列位置探索を備える。

以前の `sum(l, r)` は、区間内の相異なる値を列挙する `O(DL)` の実装だったため削除した。総和には `WaveletMatrixSum` または `WaveletMatrixFenwick` を使う。

### 2.2 固定ビット幅版 `wavelet_matrix_bit.cpp`

ビット数をテンプレート引数にして `std::array` を使う実装である。ループの展開や動的配列の回避を狙った高速版と考えられる。

通常版と同じ名前空間に同じクラス名 `WaveletMatrix` を定義しているため、両方を同時にincludeできない。クラス名は現状のままとする。

現在の `sum()` は `assert(false)` で利用不能だが、今回の整理対象にはせず現状のまま残す。

### 2.3 `WaveletMatrix2DSum`

点 `(x, y)` と重みを先に登録して構築する、オフライン2次元データ構造である。長方形和に加え、`y` による個数・順位・先頭 `k` 点の総和を提供する。

同じ座標への複数登録は別々の点として数える。総和では全登録の重みを加え、`range_count` と `kth_y` では登録回数を使う。

### 2.4 `WaveletMatrix2DMin` と `WaveletMatrix2DMonoid`

`WaveletMatrix2DMin` は各レベルに静的RMQを持ち、長方形内の最小値・最大値とその点を返す。同値の場合は先に登録した点を選ぶ。

`WaveletMatrix2DMonoid` は各レベルにSegment Treeを持つ一般化版である。2次元点集合には自然な積順序がないため、演算が可換であることを前提とする。

### 2.5 `DynamicWaveletMatrix`

各ビットレベルに1本ずつ `AVLTreeBitVector` を持ち、`mid` に0側の要素数を保持する。静的WMに近い平坦な構造である。

`insert`, `pop`, `set`, `topk`, `has_majority` まで実装済みであり、動的配列向けのAPIは比較的揃っている。

### 2.6 `DynamicWaveletTree`

キーのビットprefixごとにNodeを作り、各Nodeがそのprefixへ到達した要素だけの `BTreeBitVector` を持つ。

値の分布が疎な場合、各レベルを一律に持つ `DynamicWaveletMatrix` より有利になる可能性がある。一方でNode数は増える。

## 3. 推奨する共通API

静的な配列WM、固定ビット幅版、動的WM、動的WTでは、可能な範囲で次の検索APIを共通化したい。

```cpp
T access(int k) const;

int rank(int r, T value) const;
int range_count(int l, int r, T value) const;

int range_freq(int l, int r, T upper) const;
int range_freq(int l, int r, T lower, T upper) const;

T kth_smallest(int l, int r, int k) const;
T kth_largest(int l, int r, int k) const;

T prev_value(int l, int r, T upper) const;
T next_value(int l, int r, T lower) const;

int select(int k, T value) const;
int range_select(int l, int r, int k, T value) const;

vector<pair<T, int>> topk(int l, int r, int k) const;
pair<bool, T> has_majority(int l, int r) const;

int len() const;
vector<T> tovector() const;
```

値域の個数取得には、既存の次のオーバーロードをそのまま使う。

```cpp
range_freq(l, r, upper)
range_freq(l, r, lower, upper)
```

`count_lt` と `count_range` の別名は追加しない。

## 4. 総和・重み付きクエリ

### 4.1 推奨API

総和型はキー型 `T` と分離し、`W` とする。値自身を合計するときも、オーバーフローを避けるため `long long` などを指定できるようにしたい。

```cpp
W range_sum(int l, int r) const;

pair<int, W> count_sum_lt(
    int l,
    int r,
    T upper
) const;

W sum_lt(int l, int r, T upper) const;

W sum_range(
    int l,
    int r,
    T lower,
    T upper
) const;

W sum_k_smallest(
    int l,
    int r,
    int k
) const;

W sum_k_largest(
    int l,
    int r,
    int k
) const;

int min_count_smallest_sum_ge(
    int l,
    int r,
    W target
) const;

int min_count_largest_sum_ge(
    int l,
    int r,
    W target
) const;
```

`sum_k_smallest(l, r, k)` の `k` は0-indexedの順位ではなく、合計する要素数とする。

```text
0 <= k <= r-l
```

例えば、

```text
a[l, r) = [5, 1, 4, 1, 9]
k = 3
```

なら、昇順先頭3個 `[1, 1, 4]` の総和 `6` を返す。

### 4.2 最小限の基本操作

内部実装としては、次の3操作を基本にすると重複を減らせる。

```cpp
range_sum(l, r)
count_sum_lt(l, r, upper)
sum_k_smallest(l, r, k)
```

他の操作は次のように導出できる。

```cpp
sum_lt(l, r, upper)
    = count_sum_lt(l, r, upper).second;

sum_range(l, r, lower, upper)
    = sum_lt(l, r, upper)
    - sum_lt(l, r, lower);

sum_k_largest(l, r, k)
    = range_sum(l, r)
    - sum_k_smallest(l, r, r-l-k);
```

これは加算と減算が可能な重みを前提とする。`min`, `max`, `gcd` など一般のモノイドでは差分を取れないため、値域 `[lower, upper)` を直接分解して集約する必要がある。

### 4.3 キーと重みの分離

一般形は、各要素を次の組として扱う。

```cpp
(key, weight)
```

- `key`: Wavelet Matrix上で大小比較する値
- `weight`: 総和などの集約対象

値自身の総和では `weight = key` とする。

重み付きで `sum_k_smallest` を提供する場合、同じキーを持つ要素のうち一部だけを採用する可能性がある。そのため、同値キー間の順序を元配列上の安定順序とするなど、仕様を明記する必要がある。

### 4.4 総和を満たす最小個数

次のクエリも、総和付きWMと相性がよい。

```cpp
int min_count_smallest_sum_ge(
    int l,
    int r,
    W target
) const;

int min_count_largest_sum_ge(
    int l,
    int r,
    W target
) const;
```

これは、区間 `[l, r)` の要素をキーの昇順または降順に並べたとき、先頭 `k` 個の重みの総和が `target` 以上になる最小の `k` を返す。

概念上は、

```text
sum_k_smallest(l, r, k) >= target
```

を満たす最小の `k` の探索である。`sum_k_smallest` を呼びながら `k` を二分探索することもできるが、WM上を直接降りた方が速い。

昇順の場合、各レベルで0側に含まれる要素の個数 `count0` と総和 `sum0` を求める。

- `sum0 >= target` なら0側へ降りる
- `sum0 < target` なら0側をすべて採用し、`answer += count0`, `target -= sum0` として1側へ降りる

降順の場合は1側と0側を逆にする。値自身の総和なら、最後に同じ値 `value` の葉へ到達した時点で、

```text
ceil(target / value)
```

個を追加すればよい。

計算量は次の通りである。

| 実装 | WMを直接降りる | `k` を外側で二分探索 |
|---|---:|---:|
| 静的総和WM | 一般重みは `O(L + log n)`、`weight = key` は `O(L)` | `O(L log n)` |
| 動的総和WM | `O(L log n)` | `O(L log^2 n)` |

この探索には、昇順に要素を追加したときの累積重みが単調に増えることが必要である。したがって、まずは重みが非負の場合に限定する。負の重みを許すと、枝全体の総和だけでは「枝の途中で一度 `target` に達したか」を判定できない。

一般の `(key, weight)` で同じキーに異なる重みがある場合、葉の中で何個採るかを決めるため、その葉における安定順序上の累積和または `max_right` が必要になる。`weight = key` なら同じ葉の重みはすべて等しいため、この追加構造は不要である。

### 4.5 WM上の二分探索の整理

WMを使った二分探索には、次の2通りがある。

1. `sum_k_smallest` や `sum_lt` を何度も呼び、外側で通常の二分探索を行う
2. 各レベルの枝が持つ個数・集約値を見て、条件を満たす枝を選びながらWMを1回だけ降りる

値順に関する単調探索では、原則として2を採用する。探索対象を軸で分けると次のようになる。

| 探索軸 | 求めるもの | 代表例 | WMの直接降下 |
|---|---|---|---|
| 値順の個数 `k` | 条件を満たす最大・最小個数 | 予算内の最大個数、目標和へ達する最小個数 | 可能 |
| 値の境界 `value` | 累積集約が条件を変える最初のキー | 重み付き中央値、重み付き分位点 | 可能 |
| 元配列上の位置 `pos` | 条件を満たす最初・最後の添字 | 値域内の次の要素位置 | 通常は不可能 |

値順の個数と値の境界は、同じ降下処理から同時に得られる。元配列上の位置はWMの各レベルで直接表現されないため、通常は `range_count` などを判定関数として外側で二分探索する。

#### 4.5.1 値順の `max_right`

`WaveletMatrixSum`, `WaveletMatrixFenwick`, `DynamicWaveletTreeSum` に次のAPIを実装した。

```cpp
template<class Pred>
tuple<int, T, W> max_right_smallest(int l, int r, Pred pred) const;

template<class Pred>
tuple<int, T, W> max_right_largest(int l, int r, Pred pred) const;
```

返り値は `(count, boundary_value, aggregate)` の順である。`count` は採用できた個数、`boundary_value` は次に採用する要素のキー、`aggregate` は採用済みの重みの総和を表す。全要素を採用できた場合の `boundary_value` は `-1` とする。

個数だけが必要なら、返り値の `get<0>` を使う。単なる別名となる汎用の `max_count_smallest` と `max_count_largest` は提供しない。

Segment Treeの `max_right` を、元配列の位置順ではなくキーの値順に行う操作に相当する。`pred(0) == true` であり、要素を加える順に一度偽になった後は偽のままであることを前提とする。

同じキーを持つ要素を途中まで採る場合は、葉の中でも `max_right` を行う。

#### 4.5.2 総和から導出する操作

重みが非負なら、次を同じプリミティブから導出できる。

| 操作 | 単調条件 |
|---|---|
| 小さい方から総和が予算以下となる最大個数 | `sum <= budget` |
| 大きい方から総和が予算以下となる最大個数 | `sum <= budget` |
| 小さい方から総和が目標以上となる最小個数 | `sum < target` で `max_right` し、境界要素を含める |
| 大きい方から総和が目標以上となる最小個数 | 上記の降順版 |
| 重み付き中央値 | `target = ceil(total_weight / 2)` とした境界キー |
| 重み付き `q` 分位点 | `target = ceil(total_weight * q)` とした境界キー |
| 累積重みが指定割合へ達する最初のキー | `prefix_weight < target` |

次の公開APIを実装した。

```cpp
int max_count_smallest_sum_le(
    int l,
    int r,
    W budget
) const;

int max_count_largest_sum_le(
    int l,
    int r,
    W budget
) const;

int min_count_smallest_sum_ge(
    int l,
    int r,
    W target
) const;

int min_count_largest_sum_ge(
    int l,
    int r,
    W target
) const;

T weighted_quantile(
    int l,
    int r,
    W target
) const;

T weighted_quantile(
    int l,
    int r,
    long long numerator,
    long long denominator
) const;

T weighted_median(int l, int r) const;
```

3引数の `weighted_quantile` は、昇順の累積重みが初めて `target` 以上になるキーを返す。4引数版は `numerator / denominator` 分位点、`weighted_median` はその `1 / 2` ラッパーである。

#### 4.5.3 総和以外から導出する操作

枝ごとに対応する集約値を取得できれば、総和以外にも適用できる。

| 集約 | 単調条件の例 | 得られる操作 |
|---|---|---|
| 個数 | `count <= k` | `kth_smallest`, `kth_largest` |
| ビットOR | 必要なマスクをまだすべて含んでいない | 指定マスクを覆う最小個数・境界キー |
| ビットAND | 必要なビットをまだすべて保持している | 指定ビットを初めて失う個数・境界キー |
| 最大値 | `maximum < target` | 最大値が閾値以上になる最小個数 |
| 最小値 | `minimum > target` | 最小値が閾値以下になる最小個数 |
| gcd | `gcd != 1` | gcdが初めて1になる最小個数 |

ここで集約対象はキー自身とは限らず、各キーに付随する重み・属性でもよい。キー自身の最大値を昇順に集約する場合など、既存の `range_freq` や `kth_smallest` へ単純化できるものもある。

通常の静的WMで枝の区間集約を取得する方法は、演算によって異なる。

- 総和・個数など差分を取れる演算: 各レベルの累積値
- `min`, `max`, `gcd`, ビットORなど: 各レベルのRMQまたはSegment Tree
- 動的版の一般モノイド: 各レベルの動的列に部分木集約を保持

したがって、総和版に任意の述語だけを追加すればすべて利用できるわけではない。演算に対応した枝集約が必要である。

#### 4.5.4 値の境界を求める探索

次の2つは返り値が異なるだけで、内部では同じWM降下を利用できる。

```text
値順の先頭から、条件を満たす最大個数を求める
値順の累積集約が条件を変える最初のキーを求める
```

例えば重み付き分位点では境界キーだけが必要だが、総和が目標へ達する最小個数では、境界キーと、そのキーを何個採るかの両方が必要になる。

値 `value` を外側で二分探索し、

```cpp
sum_lt(l, r, value)
```

を繰り返す実装も可能である。しかしWMをビットごとに直接降りれば、同じ判定を枝単位で行える。

#### 4.5.5 元配列上の位置を求める探索

値順ではなく元配列上の位置を求める次のAPIを、配列を扱う各WMへ実装した。

```cpp
int next_index_in_value_range(
    int l,
    int r,
    T lower,
    T upper
) const;

int prev_index_in_value_range(
    int l,
    int r,
    T lower,
    T upper
) const;

int kth_index_in_value_range(
    int l,
    int r,
    T lower,
    T upper,
    int k
) const;
```

例えば `next_index_in_value_range` は、

```text
range_freq(l, mid, lower, upper) > 0
```

を満たす最小の `mid` を二分探索すれば求められる。`kth_index_in_value_range` では右辺を `> k` にする。

`next` と `prev` は該当要素がなければ `-1` を返す。`kth` の `k` は0-indexedであり、該当個数未満であることを前提とする。

値が1種類に固定されている場合は、既存の `rank` と `select` を組み合わせた方が速い。

#### 4.5.6 計算量

枝の区間集約1回の計算量を `Q` とすると、値順の直接降下は `O(LQ)` である。

| 方法 | 静的総和WM | 動的総和WM |
|---|---:|---:|
| 値順にWMを直接降りる | 一般重みは `O(L + log n)`、同値キーの重みが一定なら `O(L)` | `O(L log n)` |
| `k` を外側で二分探索する | `O(L log n)` | `O(L log^2 n)` |
| `value` を外側で二分探索する | `O(L^2)` | `O(L^2 log n)` |
| 元配列位置を外側で二分探索する | `O(L log n)` | `O(L log^2 n)` |

値の候補を座標圧縮している場合、`value` の外側二分探索回数は `L` ではなく `O(log D)` と考えてもよい。

#### 4.5.7 適用できない、または別方式がよい操作

次は、枝全体の集約値だけを見て一方へ降りる方法にはそのまま適用できない。

- 負の重みを含む総和: 累積和が単調でない
- 累積XOR: 要素を追加すると条件が真偽の間を往復し得る
- modeや頻度上位 `topk`: 片方の枝を捨てられず、優先度付き探索が必要
- 「頻度が `k` 以上の値を最初に探す」: 枝の合計頻度から、条件を満たす葉の存在を判定できない
- distinct数: 通常の枝の個数だけでは重複を除いた種類数を得られない

また、非可換な演算を値順で扱う場合は、各レベルの安定分割順と値の完全なソート順が一致しないため、枝集約の持ち方を別途設計する必要がある。

## 5. 静的な配列WMへの総和実装

### 5.1 データの持ち方

検索専用の既存クラスは変更せず、[`wavelet_matrix_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_sum.cpp) に `WaveletMatrixSum<T, W>` を追加した。

各ビットレベルには、そのレベルの並びでbitが0の要素だけを加えた累積和を持つ。`count_sum_lt` や `sum_k_smallest` が必要とするのは0側の総和であり、全重みの累積和をレベルごとに重複して持つ必要はない。

加えて、次の2本を保持する。

- 元配列順の累積和: `range_sum` と各レベルの現在区間全体の総和に使う
- 全ビットの安定分割後の累積和: 同じキーを持つ要素を一部だけ採用するときに使う

構築時はキーと重みを同時に安定分割する。作業用のキー列・重み列は各レベルで再確保せず、2本ずつを使い回す。

`weight = key` のコンストラクタも用意した。この場合、同じキーの重みは一定なので、目標和へ達する最小個数は葉で二分探索せず除算で決める。

### 5.2 計算量

| 操作 | 計算量 |
|---|---:|
| `range_sum(l, r)` | `O(1)` |
| `count_sum_lt(l, r, upper)` | `O(L)` |
| `sum_range(l, r, lower, upper)` | `O(L)` |
| `sum_k_smallest(l, r, k)` | `O(L)` |
| 一般重みの `min_count_*_sum_ge` | `O(L + log n)` |
| `weight = key` の `min_count_*_sum_ge` | `O(L)` |
| 構築 | `O(nL)` |
| 追加メモリ | `O(nL)` 個の `W` |

単純な `range_sum(l, r)` だけなら元配列順の累積和1本でよい。値による絞り込みや `k` 個の総和まで提供するときに、各レベルの累積和が必要になる。

`min_count_smallest_sum_ge` と `min_count_largest_sum_ge` は重みが非負であることを前提とする。それ以外の総和APIでは負の重みも扱える。

### 5.3 重み更新版

キーを固定して重みだけ変更する版を、[`wavelet_matrix_fenwick.cpp`](../titan_cpplib/ds/wavelet_matrix_fenwick.cpp) の `WaveletMatrixFenwick<T, W>` として追加した。

次の累積和をFenwick Treeへ置き換えている。

- 元配列順の重み
- 各ビットレベルでbitが0の要素だけを残した重み
- 全ビットの安定分割後の重み

現在の重み列も別に保持するため、`access_weight` と更新前の重みの取得は `O(1)` である。更新時は元位置からBitVectorのrankで各レベルの位置を求める。`O(nL)` 個の位置対応表は持たない。

```cpp
void set_weight(int k, W weight);
void add_weight(int k, W delta);
```

| 操作 | 計算量 |
|---|---:|
| `set_weight`, `add_weight` | `O(L log n)` |
| `range_sum` | `O(log n)` |
| `count_sum_lt`, `sum_range` | `O(L log n)` |
| `sum_k_smallest`, `sum_k_largest` | `O(L log n)` |
| `min_count_*_sum_ge` | `O(L log n)` |
| `rank`, `kth_smallest` など検索のみの操作 | `O(L)` |

キーは変更できない。キーの変更や要素の挿入・削除が必要なら `DynamicWaveletTreeSum` を使う。

`min_count_smallest_sum_ge` と `min_count_largest_sum_ge` を使う場合は、更新後を含めて全要素の重みが非負であることを前提とする。その他の総和APIでは負の重みも扱える。

### 5.4 追加した非総和API

通常の静的 `WaveletMatrix` に次を追加した。

```cpp
has_majority(l, r)
range_select(l, r, k, value)
tovector()
```

`range_select` は、既存APIから概ね次のように導出できる。

```cpp
select(rank(l, value) + k, value)
```

列挙で総和を求めていた既存の `sum()` は削除した。総和が必要なら `WaveletMatrixSum` または `WaveletMatrixFenwick` を使う。

## 6. 固定ビット幅版のAPI整理

固定ビット幅版はクラス名と `sigma` の仕様を変更せず、総和付き固定ビット幅版も追加しない。

通常版とのAPI統一のため、次を追加した。

- `has_majority`
- `range_select`
- `tovector`
- `next_index_in_value_range`
- `prev_index_in_value_range`
- `kth_index_in_value_range`

`topk` の返り値型は `vector<pair<T, int>>` に修正した。既存の利用不能な `sum()` はそのまま残している。

## 7. `WaveletMatrix2DSum`

[`wavelet_matrix_2d_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_sum.cpp) に次を実装した。

```cpp
W range_sum(T x1, T x2) const;
W range_sum(T x1, T x2, T y1, T y2) const;
W sum_lt(T x1, T x2, T upper) const;
pair<int, W> count_sum_lt(T x1, T x2, T upper) const;
int range_count(T x1, T x2, T y1, T y2) const;
T kth_y(T x1, T x2, int k) const;
W sum_k_smallest_y(T x1, T x2, int k) const;
W sum_k_largest_y(T x1, T x2, int k) const;
```

点は `add_point(x, y, weight)` で登録してから `build()` する。同じ座標への登録をuniqueせず、1登録を1点として保持する。

同じ `y` の途中で `sum_k_smallest_y` または `sum_k_largest_y` が打ち切られる場合は、`x` の昇順、同じ `x` では登録順に採用する。

## 8. `WaveletMatrix2DMin` と `WaveletMatrix2DMonoid`

[`wavelet_matrix_2d_min.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_min.cpp) に次を実装した。

```cpp
W range_min(T x1, T x2, T y1, T y2) const;
tuple<W, T, T> range_argmin(T x1, T x2, T y1, T y2) const;
W range_max(T x1, T x2, T y1, T y2) const;
tuple<W, T, T> range_argmax(T x1, T x2, T y1, T y2) const;
```

`range_argmin` と `range_argmax` の返り値は `(weight, x, y)` である。問い合わせ長方形に点が存在することを前提とし、重みが同じなら先に登録した点を返す。

[`wavelet_matrix_2d_monoid.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp) の `WaveletMatrix2DMonoid<T, S, op, e>` は、長方形内の `range_prod` を提供する。`op` は可換モノイドであることを前提とする。

総和版は累積和、Min版は静的RMQ、一般モノイド版はSegment Treeを各レベルに持つ。専用版の方が一般モノイド版より高速かつ省メモリである。

## 9. `DynamicWaveletMatrix` の総和版

`DynamicWaveletMatrixSum<T, W>` は追加しない。動的なキー・重み・配列長と総和クエリを同時に扱う用途には、実装済みの `DynamicWaveletTreeSum<T, W>` を使う。

`DynamicWaveletMatrix` に同じ重み情報を追加すると検索専用版まで重くなり、別クラスにすると重い動的総和構造が重複するためである。

## 10. `DynamicWaveletTree` の総和版と追加API

### 10.1 総和の持ち方

各Nodeのビット列 `v` と同じ添字で、0側へ進む要素の重みを保持する。

値自身の総和なら、概念的には次の列を持つ。

```cpp
zero_sum[i] = (v[i] == 0 ? value[i] : 0);
```

`sum_lt(l, r, upper)` では、`upper` の現在ビットが1なら、そのNodeの `[l, r)` にある0側の総和を答えへ加え、1側へ進む。

`sum_k_smallest(l, r, k)` では、0側の個数が `k` 以下なら0側の総和を丸ごと加え、残りの個数について1側へ進む。

より一般的には、各prefix Nodeに、そのNodeへ到達した要素の動的集約列を持たせる方法もある。

実装では検索専用版を変更せず、次の専用ファイルを追加した。

- [`b_tree_bit_vector_sum.cpp`](../titan_cpplib/ds/b_tree_bit_vector_sum.cpp)
- [`dynamic_wavelet_tree_sum.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp)

`BTreeBitVectorSum<W>` は、WT内の全prefix Nodeで葉・内部Nodeのアリーナを共有する。各部分木に要素数、1の個数、全重みの総和、bit=1の重みの総和を保持する。これにより、区間の `count0`, `count1`, `sum0`, `sum1` を1回のB-tree探索で取得する。

`DynamicWaveletTreeSum<T, W>` の更新APIは次の通りである。

```cpp
void insert(int pos, T key, W weight);
pair<T, W> pop(int pos);
void set(int pos, T key, W weight);
void set_key(int pos, T key);
void set_weight(int pos, W weight);
void add_weight(int pos, W delta);
```

総和APIは次の通りである。

```cpp
W range_sum(int l, int r) const;
pair<int, W> count_sum_lt(int l, int r, T upper) const;
W sum_lt(int l, int r, T upper) const;
W sum_range(int l, int r, T lower, T upper) const;
W sum_k_smallest(int l, int r, int k) const;
W sum_k_largest(int l, int r, int k) const;
int min_count_smallest_sum_ge(int l, int r, W target) const;
int min_count_largest_sum_ge(int l, int r, W target) const;
```

最小個数探索では、各階層で優先する枝の総和が目標以上ならその枝へ降り、足りなければ枝全体を採用して反対側へ降りる。同値キーの葉では安定順序の重み列に対して直接 `max_right` 相当の探索を行う。重みが非負であることを前提とする。

### 10.2 検索APIと領域予約

`DynamicWaveletTree` と `DynamicWaveletTreeSum` の両方に、次を実装した。

```cpp
vector<pair<T, int>> topk(int l, int r, int k) const;
int range_select(int l, int r, int k, T value) const;
void reserve(int expected_size);
```

`reserve` は最終要素数の見積もりからNode領域を予約する。総和版では、全prefix Nodeが共有するB-treeの葉・内部Node領域も予約する。

## 11. 境界条件の仕様

### 11.1 `k` の仕様

```cpp
kth_smallest(l, r, k)
```

では `k` は0-indexed順位であり、

```text
0 <= k < r-l
```

とする。

一方、

```cpp
sum_k_smallest(l, r, k)
```

では `k` は個数であり、

```text
0 <= k <= r-l
```

とする。

特に `k = 0` を正しく処理する。`topk(l, r, 0)` も空配列を返すべきである。

### 11.2 空配列と `sigma = 1`

次を明示的にテストする。

- 空配列からの構築
- `sigma = 1`
- 全要素が0
- `l = r`
- `log = 0`

## 12. 推奨するクラス構成

軽量な検索専用版と、重みを持つ集約版を分離する。

```cpp
WaveletMatrix<T>
WaveletMatrix<T, LOG>

WaveletMatrixSum<T, W>
WaveletMatrixFenwick<T, W>

WaveletMatrix2DSum<T, W>
WaveletMatrix2DMin<T, W>
WaveletMatrix2DMonoid<T, S, op, e>

DynamicWaveletMatrix<T>

DynamicWaveletTree<T>
DynamicWaveletTreeSum<T, W>
```

別クラスにせず集約ポリシーをテンプレート引数にする方法もあるが、総和付き版は `O(nL)` 個の重み情報を追加する。総和を使わないケースのメモリとコンパイル時間を増やさないため、まずは別クラスにする方が単純である。

## 13. 実装状況

- `WaveletMatrixSum` と `WaveletMatrixFenwick` の総和・値順探索APIを実装済み
- 通常版と固定ビット幅版の指定された検索APIを実装済み
- `DynamicWaveletTreeSum` の総和・重み更新・値順探索APIを実装済み
- `DynamicWaveletTree` 系の `topk`, `range_select`, `reserve`, `set_key` を実装済み
- `WaveletMatrix2DSum`, `WaveletMatrix2DMin`, `WaveletMatrix2DMonoid` を実装済み
- `DynamicWaveletMatrixSum` は追加せず、同等用途を `DynamicWaveletTreeSum` に集約

今後の候補は、全実装における `sigma` とビット数の定義統一、座標圧縮ラッパー、総和以外の値順集約である。

## 14. テスト項目

### 14.1 共通

- 空配列
- 長さ1
- `sigma = 1`
- `sigma` が2冪
- `sigma` が2冪でない
- 全要素が同じ
- 全要素が異なる
- 重複が多い
- `l = 0`, `r = n`
- `l = r`
- `upper = 0`
- `upper = sigma`
- `k = 0`
- `k = r-l`

### 14.2 総和

- `W` と `T` が異なる
- 総和が `int` を超える
- 重みが0
- 重みが負数
- キーが同じで重みが異なる
- `sum_range = sum_lt(upper) - sum_lt(lower)` の一致
- `sum_k_largest` と全体和・`sum_k_smallest` の一致
- 非負重みに対する `min_count_smallest_sum_ge`
- 非負重みに対する `min_count_largest_sum_ge`

### 14.3 動的版

ナイーブな `vector` と比較するランダムテストを行う。

- 任意位置への `insert`
- 任意位置からの `pop`
- `set_key`
- `set_weight`
- `add_weight`
- 更新後の `access`
- 更新後の `rank`
- 更新後の `kth_smallest`
- 更新後の `topk`
- 更新後の `range_select`
- 更新後のすべての総和クエリ
- 空から構築して再び空になる操作列

### 14.4 2次元版

- x・yが負の点
- 同一座標への複数登録
- 同じyを持つ点の安定順序
- 長方形の個数・総和・`kth_y`・先頭k点の総和
- 最小値・最大値と同値時の登録順
- 可換モノイド版とナイーブ実装の一致

### 14.5 実装間比較

同じ配列に対し、次の結果が一致することを確認する。

- `WaveletMatrix`
- `wavelet_matrix_bit.cpp` の固定ビット幅版
- `DynamicWaveletMatrix`
- `DynamicWaveletTree`
- ナイーブ実装

総和付き版についても、静的版と動的版を同じクエリ列で比較する。

## 15. まとめ

検索専用版と集約版を分けたまま、静的総和、重み更新可能な静的総和、動的総和、2次元総和・最小値・可換モノイドを整備した。

値順の述語探索は専用Result型を持たず、`tuple<int, T, W>` で `(count, boundary_value, aggregate)` を返す。重み付き分位点、元配列位置探索、通常版・固定ビット幅版・動的Wavelet Treeの指定APIも追加済みである。

2次元版は1登録を1点として扱う多重集合仕様で統一した。動的総和は `DynamicWaveletTreeSum` に集約し、`DynamicWaveletMatrixSum` は追加しない。
