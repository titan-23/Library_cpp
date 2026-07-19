# Wavelet Matrix 系データ構造の整理と拡張案

## 1. 目的

この文書では、リポジトリに存在する Wavelet Matrix / Wavelet Tree 系の実装を整理し、次の点をまとめる。

- 各実装の現在の役割
- 既存APIの差異
- 総和・重み付きクエリの追加案
- 静的版・動的版それぞれに追加したい機能
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
| [`wavelet_matrix.cpp`](../titan_cpplib/ds/wavelet_matrix.cpp) | `WaveletMatrix<T>` | 通常の静的な配列 | `access`, `rank`, `select`, `kth_smallest`, `kth_largest`, `topk`, `range_freq`, `prev_value`, `next_value`, `sum` |
| [`wavelet_matrix_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_sum.cpp) | `WaveletMatrixSum<T, W>` | 総和付きの静的な配列 | 通常の検索API、値域総和、昇順・降順先頭 `k` 個の総和、目標和へ達する最小個数 |
| [`wavelet_matrix_fenwick.cpp`](../titan_cpplib/ds/wavelet_matrix_fenwick.cpp) | `WaveletMatrixFenwick<T, W>` | キー固定・重み更新可能な配列 | 静的総和版と共通のAPI、`set_weight`, `add_weight` |
| [`wavelet_matrix_bit.cpp`](../titan_cpplib/ds/wavelet_matrix_bit.cpp) | `WaveletMatrix<T, log>` | ビット幅をコンパイル時に固定した静的版 | 通常版とほぼ同じ検索API |
| [`wavelet_matrix_cumulative_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_cumulative_sum.cpp) | `WaveletMatrixCumulativeSum<T, W>` | オフライン2次元点集合の重み総和 | 点の登録、構築、長方形領域の総和 |
| [`wavelet_matrix_min.cpp`](../titan_cpplib/ds/wavelet_matrix_min.cpp) | `WaveletMatrixMin<T, W>` | オフライン2次元点集合の最小値 | 点の登録、構築、長方形領域の最小値 |
| [`dynamic_wavelet_matrix.cpp`](../titan_cpplib/ds/dynamic_wavelet_matrix.cpp) | `DynamicWaveletMatrix<T>` | 各ビットレベルに動的ビット列を持つ可変長配列 | 挿入、削除、更新、順位・頻度検索、`topk`, majority |
| [`dynamic_wavelet_tree.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree.cpp) | `DynamicWaveletTree<T>` | キーのprefixごとにNodeを持つ可変長配列 | 挿入、削除、更新、順位・頻度検索、majority |
| [`dynamic_wavelet_tree_sum.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp) | `DynamicWaveletTreeSum<T, W>` | 総和付きの可変長配列 | 挿入、削除、更新、検索API、静的総和版と共通の総和API |

### 2.1 通常の静的 `WaveletMatrix`

一般的な1次元配列の Wavelet Matrix である。検索系APIは概ね揃っている。

現在の `sum(l, r)` は、`topk(l, r, r-l)` で区間内の相異なる値を列挙し、`value * count` を加算している。このため、計算量は `O(D L)` となる。単純な区間総和としては重く、総和付き Wavelet Matrix の代用にはならない。

### 2.2 固定ビット幅版 `wavelet_matrix_bit.cpp`

ビット数をテンプレート引数にして `std::array` を使う実装である。ループの展開や動的配列の回避を狙った高速版と考えられる。

ただし、通常版と同じ名前空間に同じクラス名 `WaveletMatrix` を定義しているため、両方を同時にincludeできない。役割を明確にするため、例えば `WaveletMatrixFixed<T, LOG>` への改名が望ましい。

また、現在の `sum()` は `assert(false)` で利用不能になっている。総和を提供しないならAPI自体を削除し、提供するなら総和付き固定ビット幅版として実装を分けた方が安全である。

### 2.3 `WaveletMatrixCumulativeSum`

これは通常の配列WMというより、点 `(x, y)` に重み `w` を持たせたオフライン2次元データ構造である。

```cpp
sum(x1, x2, y1, y2)
```

により、領域

```text
[x1, x2) × [y1, y2)
```

にある点の重み総和を求められる。

配列 `a` から構築するコンストラクタでは、点 `(i, a[i])` に重み `a[i]` を設定している。そのため、実質的に

```text
インデックスが [l, r)
かつ値が [lower, upper)
```

である要素の総和を既に計算できる。

一方で、通常の1次元WMとしての `rank`, `kth_smallest`, `sum_k_smallest` などは公開されていない。

### 2.4 `WaveletMatrixMin`

`WaveletMatrixCumulativeSum` と同様に、オフライン2次元点集合を対象とする。長方形領域内の重みの最小値を返す。

通常の配列に対する「値で絞り込んだ集約」という点では総和版と同じ構造だが、累積和ではなく静的RMQを各レベルに持っている。

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

int count_lt(int l, int r, T upper) const;
int count_range(int l, int r, T lower, T upper) const;

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

既存の `range_freq` は、

```cpp
range_freq(l, r, upper)
range_freq(l, r, lower, upper)
```

の2通りにオーバーロードされている。互換性のため残しつつ、意味が明確な `count_lt` と `count_range` を別名として提供する案もある。

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

中心となる内部プリミティブは次の形である。

```cpp
template<class Pred>
int max_count_smallest(
    int l,
    int r,
    Pred pred
) const;

template<class Pred>
int max_count_largest(
    int l,
    int r,
    Pred pred
) const;
```

`max_count_smallest` は、区間 `[l, r)` をキーの昇順に並べ、その先頭から集約した値 `aggregate` について、`pred(aggregate)` が真である最大の要素数を返す。`max_count_largest` は降順版である。

Segment Treeの `max_right` を、元配列の位置順ではなくキーの値順に行う操作に相当する。`pred(identity) == true` であり、一度偽になった後は偽のままであることを前提とする。

必要なら、個数だけでなく次も同じ探索から返せる。

```cpp
struct ValueOrderSearchResult {
    int count;
    T boundary_value;
    W aggregate;
};
```

- `count`: 条件を満たす範囲で採用できた個数
- `boundary_value`: 次に採用すると条件が変わる要素のキー
- `aggregate`: 採用済み要素の集約値

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

公開APIの候補は次の通りである。

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
```

`weighted_quantile` の `target` は、昇順の累積重みが初めて `target` 以上になるキーを求めるものとする。中央値や割合指定の分位点はこのラッパーとして提供できる。

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

ここで集約対象はキー自身とは限らず、各キーに付随する重み・属性でもよい。キー自身の最大値を昇順に集約する場合など、既存の `count_lt` や `kth_smallest` へ単純化できるものもある。

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

値順ではなく元配列上の位置を求める場合は、外側の二分探索が有用である。

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
count_range(l, mid, lower, upper) > 0
```

を満たす最小の `mid` を二分探索すれば求められる。`kth_index_in_value_range` では右辺を `> k` にする。

ただし、値が1種類に固定されている場合は既存の `rank` と `select` を組み合わせた方が速い。値域全体を対象とする位置探索だけを二分探索の候補とする。

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

### 5.4 追加したい非総和API

通常の静的 `WaveletMatrix` には、動的版とのAPI統一のため次を追加する候補がある。

```cpp
has_majority(l, r)
range_select(l, r, k, value)
tovector()
```

`range_select` は、既存APIから概ね次のように導出できる。

```cpp
select(rank(l, value) + k, value)
```

## 6. 固定ビット幅版への追加案

固定ビット幅版は、通常版とAPIを揃えつつ、クラス名を分離する。

```cpp
template<typename T, int LOG>
class WaveletMatrixFixed;
```

総和を必要とする場合は、軽量版を太らせず、別クラスにする案が自然である。

```cpp
template<typename T, typename W, int LOG>
class WaveletMatrixFixedSum;
```

固定ビット幅版については、現在次の点も整理が必要である。

- 通常版と同名で同時includeできない
- `sum()` が `assert(false)` になっている
- `topk` の返り値型が `T` ではなく `int` に固定されている箇所がある
- `sigma` が上端、最大値、ビットマスクのどれを表すかが曖昧

## 7. `WaveletMatrixCumulativeSum` への追加案

現在の長方形和を基礎に、次のAPIを追加すると、2次元点集合として機能が揃う。

```cpp
W range_sum(T x1, T x2) const;

W sum_lt(
    T x1,
    T x2,
    T y
) const;

pair<int, W> count_sum_lt(
    T x1,
    T x2,
    T y
) const;

int range_count(
    T x1,
    T x2,
    T y1,
    T y2
) const;

T kth_y(
    T x1,
    T x2,
    int k
) const;

W sum_k_smallest_y(
    T x1,
    T x2,
    int k
) const;

W sum_k_largest_y(
    T x1,
    T x2,
    int k
) const;
```

クラス名は、通常の配列向け総和WMと区別するため、例えば次のようにする。

```cpp
WaveletMatrix2DSum<X, Y, W>
```

### 7.1 重複点の仕様

現在の構築処理では座標 `(x, y)` をuniqueし、同じ座標の重みを同じ位置へ加算している。

したがって、現状の総和については重複点の重みが合算される。一方、`count` や `kth_y` を追加する場合は、

- 同一座標の複数登録を複数点として数える
- 同一座標を1点として数える

のどちらかを明確にする必要がある。

## 8. `WaveletMatrixMin` への追加案

総和とは別の2次元集約構造として、次の追加が考えられる。

```cpp
W range_min(x1, x2, y1, y2) const;
pair<W, Point> range_argmin(x1, x2, y1, y2) const;

W range_max(x1, x2, y1, y2) const;
pair<W, Point> range_argmax(x1, x2, y1, y2) const;
```

一般化する場合は次のようなモノイド版も考えられる。

```cpp
WaveletMatrix2DMonoid<X, Y, S, op, e>
```

ただし、総和は累積和、最小値は静的RMQというように、演算ごとにより適した内部構造がある。性能を重視する場合、`Sum` と `Min` の専用クラスを残す方がよい。

## 9. `DynamicWaveletMatrix` への追加案

### 9.1 総和の持ち方

各ビットレベルに、ビット列と同じ並びの動的集約列を追加する。各レベルでキーを0側・1側へ移動させるのと同時に、重みも同じ位置へ移動させる。

候補となるAPIは次の通り。

```cpp
W range_sum(int l, int r) const;
pair<int, W> count_sum_lt(int l, int r, T upper) const;
W sum_lt(int l, int r, T upper) const;
W sum_range(int l, int r, T lower, T upper) const;
W sum_k_smallest(int l, int r, int k) const;
W sum_k_largest(int l, int r, int k) const;
```

### 9.2 重み付き更新

キーと重みを分離する場合は、次の更新APIが必要になる。

```cpp
void insert(int pos, T key, W weight);
pair<T, W> pop(int pos);

void set_key(int pos, T key);
void set_weight(int pos, W weight);
void add_weight(int pos, W delta);
```

`set_weight` と `add_weight` はキーの経路を変更せず、各レベルの集約列だけを更新する。

### 9.3 計算量

| 操作 | 計算量 |
|---|---:|
| `range_sum(l, r)` | `O(log n)` |
| `count_sum_lt` | `O(L log n)` |
| `sum_range` | `O(L log n)` |
| `sum_k_smallest` | `O(L log n)` |
| `insert`, `pop`, `set_key` | `O(L log n)` |
| 追加メモリ | `O(nL)` 個の重み情報 |

### 9.4 既存実装で先に統一したい点

空配列用コンストラクタは `bit_length(sigma-1)`、配列付きコンストラクタは `bit_length(sigma)` を使っている。値域の意味とビット数を統一してから総和を追加したい。

## 10. `DynamicWaveletTree` への追加案

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
void set_weight(int pos, W weight);
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

### 10.2 追加したいAPI

総和系に加えて、`DynamicWaveletMatrix` とのAPI差を埋めるため次を追加する候補がある。

```cpp
topk(l, r, k)
range_select(l, r, k, value)
reserve(expected_size)
```

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
WaveletMatrixFixed<T, LOG>

WaveletMatrixSum<T, W>
WaveletMatrixFixedSum<T, W, LOG>

WaveletMatrix2DSum<X, Y, W>
WaveletMatrix2DMin<X, Y, W>

DynamicWaveletMatrix<T>
DynamicWaveletMatrixSum<T, W>

DynamicWaveletTree<T>
DynamicWaveletTreeSum<T, W>
```

別クラスにせず集約ポリシーをテンプレート引数にする方法もあるが、総和付き版は `O(nL)` 個の重み情報を追加する。総和を使わないケースのメモリとコンパイル時間を増やさないため、まずは別クラスにする方が単純である。

## 13. 推奨する実装優先順位

### 優先度0: 既存実装の整理

1. 固定ビット幅版を `WaveletMatrixFixed` へ改名する
2. `sigma` とビット数の定義を全実装で統一する

### 優先度1: 静的総和（`WaveletMatrixSum` として実装済み）

1. 既存 `WaveletMatrixCumulativeSum` の内部構造を参考にする
2. 配列向け `WaveletMatrixSum<T, W>` を用意する
3. `range_sum`, `count_sum_lt`, `sum_range` を追加する
4. `sum_k_smallest`, `sum_k_largest` を追加する
5. 通常版の低速な `sum` を置き換えるか非推奨にする

### 優先度2: 動的総和（`DynamicWaveletTreeSum` として実装済み）

1. `DynamicWaveletMatrixSum<T, W>` または `DynamicWaveletTreeSum<T, W>` を追加する
2. 重み付き `insert`, `pop`, `set_weight` を追加する
3. 総和クエリを静的版と同じ名前・意味にする
4. メモリ量と速度を比較する

### 優先度3: APIの統一

1. 静的版へ `has_majority` を追加する
2. `DynamicWaveletTree` へ `topk` を追加する
3. 全配列版へ `range_select` を追加する
4. `count_lt`, `count_range` など明示的な別名を検討する
5. 座標圧縮ラッパーを追加する

### 優先度4: 2次元集約の拡張

1. `WaveletMatrixCumulativeSum` を `WaveletMatrix2DSum` として整理する
2. `WaveletMatrixMin` を `WaveletMatrix2DMin` として整理する
3. `count`, `kth_y`, `sum_k_smallest_y` を追加する
4. 必要ならモノイド版を検討する

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
- 更新後の `access`
- 更新後の `rank`
- 更新後の `kth_smallest`
- 更新後のすべての総和クエリ
- 空から構築して再び空になる操作列

### 14.4 実装間比較

同じ配列に対し、次の結果が一致することを確認する。

- `WaveletMatrix`
- `WaveletMatrixFixed`
- `DynamicWaveletMatrix`
- `DynamicWaveletTree`
- ナイーブ実装

総和付き版についても、静的版と動的版を同じクエリ列で比較する。

## 15. まとめ

既存実装には、通常の静的WM、固定ビット幅版、2次元総和版、2次元最小値版、動的WM、動的WTが存在する。

静的な値域付き総和の基礎機能は `WaveletMatrixCumulativeSum` に既に存在する。ただし通常の配列WMとはAPIと役割が分離されており、`sum_k_smallest` などは未実装である。

配列向けの静的総和版 `WaveletMatrixSum`、キー固定・重み更新版 `WaveletMatrixFenwick`、動的総和版 `DynamicWaveletTreeSum` は、検索専用版から分離して実装した。各総和版でクエリAPIと境界条件を揃えている。

今後の候補は、固定ビット幅版の名前と仕様の整理、2次元集約版のAPI拡張、および検索専用版同士のAPI統一である。
