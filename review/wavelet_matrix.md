# Wavelet Matrix 系データ構造の整理と実装状況

## 実装の確認結果 (2026-09-03)

現行の `titan_cpplib/ds` 123 件の一部として、ここに列挙された Wavelet Matrix, Wavelet Tree の実装と関連テストを、旧本文を参照する前に再読しました。実行はしていません。設計の整理は概ね有用ですが、実装には次の未解決事項があります。

- `dynamic_wavelet_matrix.cpp:180-201`: `l<r, k=0` の `topk` は空配列を返さず、k を負にして全種類を返し得ます。深さの型を `char` にしているため、`char` が符号なしの環境では空でない区間の探索が葉へ到達する直前に深さが 255 となり、`_v[255]` を範囲外参照します。`_log=0` の場合は空区間でも同じです。`char` が符号付きの環境では `_log=0` の空区間にも件数 0 の項目を返します。`:204-221` の `select` は位置をキーの型 T で扱っており、`T=uint8_t` などで列が長い場合は位置が切り捨てられます。`DynamicWaveletTree` と `DynamicWaveletTreeSum` に k=0 の同じ不具合はありません。
- `wavelet_matrix.cpp:16-20,43,262-263` はデフォルトコンストラクタで `sigma`, `log`, `n` を初期化せず、`wavelet_matrix_bit.cpp:17-20,49,52,273-274` は `sigma` と `n` を初期化しません。直後の `len()` や `get_sigma()` が不定値を参照するため、有効な空の構造にするか、デフォルト構築を禁止するようにします。
- `wavelet_matrix_bit.cpp:194-209`: `range_freq(..., upper)` の `upper` が負の値、またはビット幅で表現できる範囲を超える値でも下位ビットだけを参照し、正しくない件数を返します。キーを非負とする仕様なら `upper<=0` で 0、`upper>=2^log` で区間長を返す必要があります。`:185-191` の公開メソッド `sum` は先頭に無条件の `assert(false)` があり、`assert` が有効な場合は正当な呼び出しでも停止します。
- `wavelet_matrix_bit.cpp:14,19,50` はテンプレート引数 `log` を制限せず、コンストラクタで `1ull << log` を計算します。`WaveletMatrix<uint64_t,64>` では型のビット数と同じ 64 ビットだけ左シフトするため、未定義動作になります。`0<=log && log<64` と `log<=numeric_limits<T>::digits` をコンパイル時にチェックするか、64 ビット全幅の場合を別に処理する必要があります。
- `wavelet_matrix_sum.cpp:283-290`, `wavelet_matrix_fenwick.cpp:276-284`, `dynamic_wavelet_tree_sum.cpp:344-352` の補助関数は、`long long` の分子と分母を先に型 `W` へ変換します。`W=uint8_t, denominator=256` の場合は分母が 0 になります。より広い中間型で比を計算するようにします。
- `wavelet_matrix.cpp:45-50`, `dynamic_wavelet_matrix.cpp:54-56`, `wavelet_matrix_sum.cpp:298-315`, `wavelet_matrix_fenwick.cpp:291-309`, `dynamic_wavelet_tree_sum.cpp:356-376` は、`sigma` を指定するコンストラクタでチェックより先に `sigma-1` を計算し、減算結果が型の範囲外になる値ではオーバーフローが発生します。このうち先頭の 2 つの実装には `sigma>0` の `assert` 自体もありません。チェックの後にビット幅を求めるようにします。
- `test/dwm/wavelet_matrix_sum_random.cpp:52-65` と `test/dwm/wavelet_matrix_fenwick_random.cpp:51-64` の `topk` のテストは、返却件数と、頻度が非増加順であることを確認しておらず、空の `vector` が返されても成功します。まず `size()==min(k,D)` と順序を確認するようにします。同じ頻度の値同士の順序は公開仕様にないため、列全体の一致を条件にする場合は先にその規則を定めます。

定数倍の改善では次の項目を優先します。

1. `dynamic_wavelet_matrix.cpp:120-129` の公開メソッド `access` から、既存の `avl_tree_bit_vector.cpp:627-648` にある `_access_ans_rank1()` を呼び出し、各レベルで AVL 木を降りる回数を 2 回から 1 回へ減らします。
2. `topk` の探索 `:193-198` では `_rank0_pair(l,r)` で `l0/r0` を取得し、`l-l0+mid`, `r-r0+mid` から `l1/r1` を求めて、`rank1` の 2 回の呼び出しを省きます。
3. `wavelet_matrix.cpp:22-39` と動的版 `:32-50` の 0 側と 1 側の一時的な `vector` を、再利用する 2 組の作業領域に変更します。
4. `dynamic_wavelet_tree_sum.cpp:971-1001` でキーを復元するときは、`b_tree_bit_vector_sum.cpp:978-985` による不要な重み列の作成を避け、ビット列だけを取得します。

挙動を変更しないように、安定分割の順序は維持します。

以下の旧本文は公開関数の構想として残しますが、この追記にある不具合と現在の判定を優先します。

## 1. 目的

この文書では、リポジトリに存在する Wavelet Matrix / Wavelet Tree 系の実装を整理し、次の点をまとめます。

- 各実装の現在の役割
- 既存 API の差異
- 総和・重み付きクエリ
- 静的版と動的版へ追加した機能
- 追加前に整理する仕様と境界条件
- 推奨するクラス構成と実装順序

この文書では、配列中の検索対象となる値を「キー」、集約の対象を「重み」と呼びます。値自身の総和を求める場合は、各要素について `weight = key` とします。

以下では次の記号を使用します。

- `n`: 配列長
- `sigma`: キーの値域の上端で、キーは原則として `[0, sigma)` に含まれる
- `L`: `ceil(log2(sigma))` 相当のビット数
- `D`: 対象区間に存在する相異なるキーの種類数

## 2. 既存実装の一覧

| ファイル | クラス | 主な用途 | 現在の主な機能 |
|---|---|---|---|
| [`wavelet_matrix.cpp`](../titan_cpplib/ds/wavelet_matrix.cpp) | `WaveletMatrix<T>` | 通常の静的な配列 | 通常の検索 API、majority、値域内の要素の元配列上の位置の探索 |
| [`wavelet_matrix_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_sum.cpp) | `WaveletMatrixSum<T, W>` | 総和付きの静的な配列 | 通常の検索 API、値域の総和、昇順または降順の先頭 `k` 個の総和、総和が目標に達する最小個数 |
| [`wavelet_matrix_fenwick.cpp`](../titan_cpplib/ds/wavelet_matrix_fenwick.cpp) | `WaveletMatrixFenwick<T, W>` | キーが固定で重みを更新できる配列 | 静的総和版と共通の API、`set_weight`, `add_weight` |
| [`wavelet_matrix_bit.cpp`](../titan_cpplib/ds/wavelet_matrix_bit.cpp) | `WaveletMatrix<T, log>` | ビット幅をコンパイル時に固定した静的版 | 通常版とほぼ同じ検索 API |
| [`wavelet_matrix_2d_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_sum.cpp) | `WaveletMatrix2DSum<T, W>` | オフラインの 2 次元点集合の重みの総和 | 長方形和、個数、`kth_y`、昇順または降順の先頭 `k` 点の総和 |
| [`wavelet_matrix_2d_min.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_min.cpp) | `WaveletMatrix2DMin<T, W>` | オフラインの 2 次元点集合の最小値と最大値 | `range_min/max`, `range_argmin/argmax` |
| [`wavelet_matrix_2d_monoid.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp) | `WaveletMatrix2DMonoid<T, S, op, e>` | オフラインの 2 次元点集合の可換モノイド積 | 長方形内の `range_prod` |
| [`dynamic_wavelet_matrix.cpp`](../titan_cpplib/ds/dynamic_wavelet_matrix.cpp) | `DynamicWaveletMatrix<T>` | 各ビットレベルに動的ビット列を保持する可変長配列 | 挿入、削除、更新、順位と頻度の検索、`topk`, majority |
| [`dynamic_wavelet_tree.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree.cpp) | `DynamicWaveletTree<T>` | キーの prefix ごとに Node を保持する可変長配列 | 挿入、削除、更新、順位と頻度の検索、majority |
| [`dynamic_wavelet_tree_sum.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp) | `DynamicWaveletTreeSum<T, W>` | 総和付きの可変長配列 | 挿入、削除、更新、検索 API、静的総和版と共通の総和 API |

### 2.1 通常の静的 `WaveletMatrix`

一般的な 1 次元配列の Wavelet Matrix です。検索系の API に加えて、`has_majority`, `range_select`, `tovector` と、値域内の要素の元配列上の位置の探索を備えています。

以前の `sum(l, r)` は、区間内の相異なる値を列挙する `O(DL)` の実装だったため削除しました。総和の計算には `WaveletMatrixSum` または `WaveletMatrixFenwick` を使用します。

### 2.2 固定ビット幅版 `wavelet_matrix_bit.cpp`

ビット数をテンプレート引数にして `std::array` を使用する実装です。ループの展開や動的配列の回避を目的とした高速版と考えられます。

通常版と同じ名前空間に同じクラス名 `WaveletMatrix` を定義しているため、両方を同時に include することはできません。クラス名は現状のままとします。

現在の `sum()` は `assert(false)` により利用できませんが、今回の整理の対象にはせず現状のまま残します。

### 2.3 `WaveletMatrix2DSum`

点 `(x, y)` と重みを先に登録してから構築する、オフラインの 2 次元データ構造です。長方形和に加えて、`y` による個数、順位、先頭 `k` 点の総和を提供します。

同じ座標へ複数回登録した場合は別々の点として扱います。総和では全登録の重みを加え、`range_count` と `kth_y` では登録回数を使用します。

### 2.4 `WaveletMatrix2DMin` と `WaveletMatrix2DMonoid`

`WaveletMatrix2DMin` は各レベルに静的 RMQ を保持し、長方形内の最小値、最大値とその点を返します。値が同じ場合は先に登録した点を選択します。

`WaveletMatrix2DMonoid` は各レベルに Segment Tree を保持する一般化版です。2 次元点集合には積を計算する自然な順序がないため、演算が可換であることを前提とします。

### 2.5 `DynamicWaveletMatrix`

各ビットレベルに 1 本ずつ `AVLTreeBitVector` があり、`mid` に 0 側の要素数を保持します。静的 WM に近い平坦な構造です。

`insert`, `pop`, `set`, `topk`, `has_majority` まで実装済みであり、動的配列向けの API は比較的そろっています。

### 2.6 `DynamicWaveletTree`

キーのビット prefix ごとに Node を作成し、各 Node はその prefix へ到達した要素だけの `BTreeBitVector` を保持します。

値の分布が疎な場合は、各レベルを一律に保持する `DynamicWaveletMatrix` より有利になる可能性があります。一方で Node 数は増加します。

## 3. 推奨する共通 API

静的な配列 WM、固定ビット幅版、動的 WM、動的 WT では、可能な範囲で次の検索 API を共通化するようにします。

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

値域内の個数の取得には、既存の次のオーバーロードをそのまま使用します。

```cpp
range_freq(l, r, upper)
range_freq(l, r, lower, upper)
```

`count_lt` と `count_range` の別名は追加しません。

## 4. 総和・重み付きクエリ

### 4.1 推奨 API

総和の型はキーの型 `T` と分離し、`W` とします。値自身を合計する場合も、オーバーフローを避けるために `long long` などを指定できるようにします。

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

`sum_k_smallest(l, r, k)` の `k` は 0-indexed の順位ではなく、合計する要素数とします。

```text
0 <= k <= r-l
```

例えば次の場合は、昇順の先頭 3 個 `[1, 1, 4]` の総和 `6` を返します。

```text
a[l, r) = [5, 1, 4, 1, 9]
k = 3
```

### 4.2 最小限の基本操作

内部実装では次の 3 つの操作を基本にすると重複を減らすことができます。

```cpp
range_sum(l, r)
count_sum_lt(l, r, upper)
sum_k_smallest(l, r, k)
```

他の操作は次のように導出できます。

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

この導出は加算と減算が可能な重みを前提とします。`min`, `max`, `gcd` などの一般のモノイドでは差分を取ることができないため、値域 `[lower, upper)` を直接分解して集約する必要があります。

### 4.3 キーと重みの分離

一般形では各要素を次の組として扱います。

```cpp
(key, weight)
```

- `key`: Wavelet Matrix 上で大小を比較する値
- `weight`: 総和などの集約の対象

値自身の総和では `weight = key` とします。

重み付きで `sum_k_smallest` を提供する場合は、同じキーの要素のうち一部だけを採用する可能性があります。そのため、キーが同じ要素間の順序を元配列上の安定順序とするなど、仕様を明記する必要があります。

### 4.4 総和が目標以上になる最小個数

次のクエリも総和付き WM に適しています。

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

このクエリは、区間 `[l, r)` の要素をキーの昇順または降順に並べたとき、先頭 `k` 個の重みの総和が `target` 以上になる最小の `k` を返します。

概念上は、次の条件を満たす最小の `k` の探索です。

```text
sum_k_smallest(l, r, k) >= target
```

`sum_k_smallest` を呼び出しながら `k` を二分探索することもできますが、WM 上を直接降りる方が高速です。

昇順の場合は各レベルで 0 側に含まれる要素の個数 `count0` と総和 `sum0` を求めます。

- `sum0 >= target` なら 0 側へ降ります。
- `sum0 < target` なら 0 側をすべて採用し、`answer += count0`, `target -= sum0` として 1 側へ降ります。

降順の場合は 1 側と 0 側を逆にします。値自身の総和なら、最後に同じ値 `value` の葉へ到達した時点で次の個数を追加します。

```text
ceil(target / value)
```

計算量は次のとおりです。

| 実装 | WM を直接降りる | `k` を外側で二分探索 |
|---|---:|---:|
| 静的総和 WM | 一般の重みでは `O(L + log n)`、`weight = key` では `O(L)` | `O(L log n)` |
| 動的総和 WM | `O(L log n)` | `O(L log^2 n)` |

この探索には、昇順に要素を追加したときの重みの累積和が単調に増加することが必要です。したがって、まずは重みが非負の場合に限定します。負の重みを許容すると、分岐先全体の総和だけでは「分岐先の途中で一度 `target` に達したか」を判定できません。

一般の `(key, weight)` で同じキーに異なる重みがある場合は、葉の中で何個採用するかを決定するために、その葉における安定順序上の累積和または `max_right` が必要になります。`weight = key` なら同じ葉の重みはすべて等しいため、この追加の構造は不要です。

### 4.5 WM 上の二分探索の整理

WM を使用した二分探索には次の 2 通りがあります。

1. `sum_k_smallest` や `sum_lt` を繰り返し呼び出し、外側で通常の二分探索を行います。
2. 各レベルの分岐先の個数と集約値を参照し、条件を満たす分岐先を選択しながら WM を 1 回だけ降ります。

値順に関する単調性を利用する探索では、原則として 2 の方法を採用します。探索の対象を軸で分類すると次のようになります。

| 探索の軸 | 求める値 | 代表例 | WM の直接降下 |
|---|---|---|---|
| 値順の個数 `k` | 条件を満たす最大個数または最小個数 | 予算内の最大個数、総和が目標に達する最小個数 | 可能 |
| 値の境界 `value` | 累積した集約値により条件の真偽が変化する最初のキー | 重み付き中央値、重み付き分位点 | 可能 |
| 元配列上の位置 `pos` | 条件を満たす最初または最後の添字 | 値域内の次の要素の位置 | 通常は不可能 |

値順の個数と値の境界は、同じ降下処理から同時に得られます。元配列上の位置は WM の各レベルで直接表現されないため、通常は `range_count` などを判定関数として外側で二分探索します。

#### 4.5.1 値順の `max_right`

`WaveletMatrixSum`, `WaveletMatrixFenwick`, `DynamicWaveletTreeSum` に次の API を実装しました。

```cpp
template<class Pred>
tuple<int, T, W> max_right_smallest(int l, int r, Pred pred) const;

template<class Pred>
tuple<int, T, W> max_right_largest(int l, int r, Pred pred) const;
```

返り値は `(count, boundary_value, aggregate)` の順です。`count` は採用できた個数、`boundary_value` は次に採用する要素のキー、`aggregate` は採用済みの重みの総和を表します。全要素を採用できた場合の `boundary_value` は `-1` とします。

個数だけが必要な場合は返り値の `get<0>` を使用します。単なる別名となる汎用の `max_count_smallest` と `max_count_largest` は提供しません。

この API は、Segment Tree の `max_right` を元配列の位置順ではなくキーの値順に行う操作に相当します。`pred(0) == true` であり、要素を順に加える過程で一度偽になった後は偽のままであることを前提とします。

同じキーの要素を途中まで採用する場合は、葉の中でも `max_right` を行います。

#### 4.5.2 総和から導出する操作

重みが非負なら次の操作を同じプリミティブから導出できます。

| 操作 | 単調条件 |
|---|---|
| 小さい方から総和が予算以下となる最大個数 | `sum <= budget` |
| 大きい方から総和が予算以下となる最大個数 | `sum <= budget` |
| 小さい方から総和が目標以上となる最小個数 | `sum < target` で `max_right` を行い、境界の要素を含める |
| 大きい方から総和が目標以上となる最小個数 | 上記の降順版 |
| 重み付き中央値 | `target = ceil(total_weight / 2)` とした境界キー |
| 重み付き `q` 分位点 | `target = ceil(total_weight * q)` とした境界キー |
| 重みの累積和が指定の割合に達する最初のキー | `prefix_weight < target` |

次の公開 API を実装しました。

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

3 引数の `weighted_quantile` は、昇順での重みの累積和が初めて `target` 以上になるキーを返します。4 引数版は `numerator / denominator` 分位点を返し、`weighted_median` はこれを `1 / 2` で呼び出すラッパーです。

#### 4.5.3 総和以外から導出する操作

分岐先ごとに対応する集約値を取得できれば、総和以外にも適用できます。

| 集約 | 単調条件の例 | 得られる操作 |
|---|---|---|
| 個数 | `count <= k` | `kth_smallest`, `kth_largest` |
| ビット OR | 必要なマスクをまだすべて含んでいない | 指定マスクをすべて含む最小個数と境界キー |
| ビット AND | 必要なビットをまだすべて保持している | 指定ビットのいずれかが初めて失われる個数と境界キー |
| 最大値 | `maximum < target` | 最大値が閾値以上になる最小個数 |
| 最小値 | `minimum > target` | 最小値が閾値以下になる最小個数 |
| gcd | `gcd != 1` | gcd が初めて 1 になる最小個数 |

ここでの集約の対象はキー自身とは限らず、各キーに付随する重みや属性でも構いません。キー自身の最大値を昇順に集約する場合など、既存の `range_freq` や `kth_smallest` に単純化できる操作もあります。

通常の静的 WM で分岐先の区間の集約値を取得する方法は、演算によって異なります。

- 総和、個数など差分を取ることができる演算: 各レベルの累積値
- `min`, `max`, `gcd`, ビット OR など: 各レベルの RMQ または Segment Tree
- 動的版の一般モノイド: 各レベルの動的列に保持する部分木の集約値

したがって、総和版に任意の述語を追加するだけではすべての操作を利用できず、演算に対応した分岐先ごとの集約値が必要です。

#### 4.5.4 値の境界を求める探索

次の 2 つは返り値が異なるだけで、内部では WM を降りる同じ処理を利用できます。

```text
値順の先頭から、条件を満たす最大個数を求める
値順に累積した集約値により条件の真偽が変化する最初のキーを求める
```

例えば重み付き分位点では境界キーだけが必要ですが、総和が目標に達する最小個数では、境界キーと、そのキーを何個採用するかの両方が必要になります。

値 `value` を外側で二分探索し、次の呼び出しを繰り返す実装も可能です。

```cpp
sum_lt(l, r, value)
```

しかし WM をビットごとに直接降りれば、同じ判定を分岐先ごとに行うことができます。

#### 4.5.5 元配列上の位置を求める探索

値順ではなく元配列上の位置を求める次の API を、配列を扱う各 WM に実装しました。

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

例えば `next_index_in_value_range` は、次の条件を満たす最小の `mid` を二分探索すれば求められます。

```text
range_freq(l, mid, lower, upper) > 0
```

`kth_index_in_value_range` では右辺を `> k` にします。

`next` と `prev` は該当する要素がなければ `-1` を返します。`kth` の `k` は 0-indexed であり、該当する要素の個数未満であることを前提とします。

値が 1 種類に固定されている場合は、既存の `rank` と `select` を組み合わせる方が高速です。

#### 4.5.6 計算量

分岐先の区間の集約値を 1 回取得する計算量を `Q` とすると、値順の直接降下は `O(LQ)` です。

| 方法 | 静的総和 WM | 動的総和 WM |
|---|---:|---:|
| 値順に WM を直接降りる | 一般の重みでは `O(L + log n)`、同じキーの重みが一定なら `O(L)` | `O(L log n)` |
| `k` を外側で二分探索する | `O(L log n)` | `O(L log^2 n)` |
| `value` を外側で二分探索する | `O(L^2)` | `O(L^2 log n)` |
| 元配列上の位置を外側で二分探索する | `O(L log n)` | `O(L log^2 n)` |

値の候補を座標圧縮している場合は、`value` の外側での二分探索の回数を `L` ではなく `O(log D)` と考えることができます。

#### 4.5.7 適用できない操作と別の方式が適する操作

次の操作には、分岐先全体の集約値だけを参照して一方へ降りる方法をそのまま適用できません。

- 負の重みを含む総和では、累積和が単調ではありません。
- 累積 XOR では、要素を追加すると条件の真偽が繰り返し変化し得ます。
- mode や頻度上位の `topk` では、片方の分岐先を除外できず、優先度付きの探索が必要です。
- 「頻度が `k` 以上の値を最初に探す」操作では、分岐先の頻度の合計から、条件を満たす葉の存在を判定できません。
- distinct 数を求める操作では、通常の分岐先の個数だけでは重複を除いた種類数を取得できません。

また、非可換な演算を値順で扱う場合は、各レベルの安定分割の順序と値の完全なソート順が一致しないため、分岐先ごとの集約値の保持方法を別途設計する必要があります。

## 5. 静的な配列 WM への総和の実装

### 5.1 データの保持方法

検索専用の既存クラスは変更せず、[`wavelet_matrix_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_sum.cpp) に `WaveletMatrixSum<T, W>` を追加しました。

各ビットレベルでは、そのレベルの並びで bit が 0 の要素だけを加えた累積和を保持します。`count_sum_lt` や `sum_k_smallest` が必要とするのは 0 側の総和であり、全重みの累積和をレベルごとに重複して保持する必要はありません。

加えて、次の 2 本の累積和を保持します。

- 元配列順の累積和は、`range_sum` と、各レベルの現在の区間全体の総和の計算に使用します。
- 全ビットの安定分割後の累積和は、同じキーの要素を一部だけ採用するときに使用します。

構築時はキーと重みを同時に安定分割します。作業用のキー列と重み列は各レベルで再確保せず、2 本ずつを再利用します。

`weight = key` とするコンストラクタも用意しました。この場合は同じキーの重みが一定であるため、総和が目標に達する最小個数は葉で二分探索せず除算で求めます。

### 5.2 計算量

| 操作 | 計算量 |
|---|---:|
| `range_sum(l, r)` | `O(1)` |
| `count_sum_lt(l, r, upper)` | `O(L)` |
| `sum_range(l, r, lower, upper)` | `O(L)` |
| `sum_k_smallest(l, r, k)` | `O(L)` |
| 一般の重みの `min_count_*_sum_ge` | `O(L + log n)` |
| `weight = key` の `min_count_*_sum_ge` | `O(L)` |
| 構築 | `O(nL)` |
| 追加メモリ | `O(nL)` 個の `W` |

単純な `range_sum(l, r)` だけなら元配列順の累積和 1 本で十分です。値による絞り込みや `k` 個の総和まで提供する場合に、各レベルの累積和が必要になります。

`min_count_smallest_sum_ge` と `min_count_largest_sum_ge` は重みが非負であることを前提とします。それ以外の総和 API では負の重みも扱うことができます。

### 5.3 重み更新版

キーを固定して重みだけを変更する版を、[`wavelet_matrix_fenwick.cpp`](../titan_cpplib/ds/wavelet_matrix_fenwick.cpp) の `WaveletMatrixFenwick<T, W>` として追加しました。

次の累積和を Fenwick Tree に置き換えています。

- 元配列順の重み
- 各ビットレベルで bit が 0 の要素だけを残した重み
- 全ビットの安定分割後の重み

現在の重み列も別に保持するため、`access_weight` と更新前の重みの取得は `O(1)` です。`O(nL)` 個の位置の対応表は保持せず、更新時は元の位置から BitVector の rank で各レベルの位置を求めます。

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

キーは変更できません。キーの変更や要素の挿入、削除が必要な場合は `DynamicWaveletTreeSum` を使用します。

`min_count_smallest_sum_ge` と `min_count_largest_sum_ge` を使用する場合は、更新後を含めて全要素の重みが非負であることを前提とします。その他の総和 API では負の重みも扱うことができます。

### 5.4 総和以外の追加 API

通常の静的 `WaveletMatrix` に次の API を追加しました。

```cpp
has_majority(l, r)
range_select(l, r, k, value)
tovector()
```

`range_select` は、既存の API から概ね次のように導出できます。

```cpp
select(rank(l, value) + k, value)
```

列挙で総和を求めていた既存の `sum()` は削除しました。総和が必要な場合は `WaveletMatrixSum` または `WaveletMatrixFenwick` を使用します。

## 6. 固定ビット幅版の API 整理

固定ビット幅版ではクラス名と `sigma` の仕様を変更せず、総和付きの固定ビット幅版も追加しません。

通常版と API を統一するため、次の API を追加しました。

- `has_majority`
- `range_select`
- `tovector`
- `next_index_in_value_range`
- `prev_index_in_value_range`
- `kth_index_in_value_range`

`topk` の返り値の型は `vector<pair<T, int>>` に修正しました。既存の利用できない `sum()` はそのまま残しています。

## 7. `WaveletMatrix2DSum`

[`wavelet_matrix_2d_sum.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_sum.cpp) に次の API を実装しました。

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

点は `add_point(x, y, weight)` で登録してから `build()` を呼び出します。同じ座標への登録は重複を除去せず、1 回の登録を 1 点として保持します。

同じ `y` の途中で `sum_k_smallest_y` または `sum_k_largest_y` が打ち切られる場合は、`x` の昇順に採用し、`x` が同じ点は登録順に採用します。

## 8. `WaveletMatrix2DMin` と `WaveletMatrix2DMonoid`

[`wavelet_matrix_2d_min.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_min.cpp) に次の API を実装しました。

```cpp
W range_min(T x1, T x2, T y1, T y2) const;
tuple<W, T, T> range_argmin(T x1, T x2, T y1, T y2) const;
W range_max(T x1, T x2, T y1, T y2) const;
tuple<W, T, T> range_argmax(T x1, T x2, T y1, T y2) const;
```

`range_argmin` と `range_argmax` の返り値は `(weight, x, y)` です。クエリの長方形に点が存在することを前提とし、重みが同じ場合は先に登録した点を返します。

[`wavelet_matrix_2d_monoid.cpp`](../titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp) の `WaveletMatrix2DMonoid<T, S, op, e>` は、長方形内の `range_prod` を提供します。`op` は可換モノイドであることを前提とします。

総和版は累積和、Min 版は静的 RMQ、一般モノイド版は Segment Tree を各レベルに保持します。専用版の方が一般モノイド版より高速かつ省メモリです。

## 9. `DynamicWaveletMatrix` の総和版

`DynamicWaveletMatrixSum<T, W>` は追加しません。動的なキー、重み、配列長と総和クエリを同時に扱う用途には、実装済みの `DynamicWaveletTreeSum<T, W>` を使用します。

`DynamicWaveletMatrix` に同じ重みの情報を追加すると検索専用版のコストまで増加し、別クラスにするとコストの大きい動的総和構造が重複するためです。

## 10. `DynamicWaveletTree` の総和版と追加 API

### 10.1 総和の保持方法

各 Node のビット列 `v` と同じ添字で、0 側へ進む要素の重みを保持します。

値自身の総和なら、概念的には次の列を保持します。

```cpp
zero_sum[i] = (v[i] == 0 ? value[i] : 0);
```

`sum_lt(l, r, upper)` では、`upper` の現在のビットが 1 なら、その Node の `[l, r)` にある 0 側の総和を答えに加えて 1 側へ進みます。

`sum_k_smallest(l, r, k)` では、0 側の個数が `k` 以下なら 0 側の総和をすべて加え、残りの個数について 1 側へ進みます。

より一般的には、各 prefix Node に、その Node へ到達した要素の動的な集約列を保持させる方法もあります。

実装では検索専用版を変更せず、次の専用ファイルを追加しました。

- [`b_tree_bit_vector_sum.cpp`](../titan_cpplib/ds/b_tree_bit_vector_sum.cpp)
- [`dynamic_wavelet_tree_sum.cpp`](../titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp)

`BTreeBitVectorSum<W>` は、WT 内の全 prefix Node で葉と内部 Node のアリーナを共有します。各部分木には要素数、1 の個数、全重みの総和、bit=1 の重みの総和を保持します。これにより、区間の `count0`, `count1`, `sum0`, `sum1` を 1 回の B-tree 探索で取得します。

`DynamicWaveletTreeSum<T, W>` の更新 API は次のとおりです。

```cpp
void insert(int pos, T key, W weight);
pair<T, W> pop(int pos);
void set(int pos, T key, W weight);
void set_key(int pos, T key);
void set_weight(int pos, W weight);
void add_weight(int pos, W delta);
```

総和 API は次のとおりです。

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

最小個数の探索では、各レベルで優先する分岐先の総和が目標以上ならその分岐先へ降り、不足する場合は分岐先全体を採用して反対側へ降ります。同じキーの葉では、安定順序の重み列に対して `max_right` に相当する探索を直接行います。重みが非負であることを前提とします。

### 10.2 検索 API と領域予約

`DynamicWaveletTree` と `DynamicWaveletTreeSum` の両方に次の API を実装しました。

```cpp
vector<pair<T, int>> topk(int l, int r, int k) const;
int range_select(int l, int r, int k, T value) const;
void reserve(int expected_size);
```

`reserve` は最終的な要素数の見積もりから Node の領域を予約します。総和版では、全 prefix Node が共有する B-tree の葉と内部 Node の領域も予約します。

## 11. 境界条件の仕様

### 11.1 `k` の仕様

次の呼び出しでは `k` は 0-indexed の順位です。

```cpp
kth_smallest(l, r, k)
```

範囲は次のとおりとします。

```text
0 <= k < r-l
```

一方、次の呼び出しでは `k` は個数です。

```cpp
sum_k_smallest(l, r, k)
```

範囲は次のとおりとします。

```text
0 <= k <= r-l
```

特に `k = 0` を正しく処理するようにします。`topk(l, r, 0)` も空配列を返すべきです。

### 11.2 空配列と `sigma = 1`

次の場合を明示的にテストします。

- 空配列からの構築
- `sigma = 1`
- 全要素が 0
- `l = r`
- `log = 0`

## 12. 推奨するクラス構成

軽量な検索専用版と、重みを保持する集約版を分離します。

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

別クラスにせず集約ポリシーをテンプレート引数にする方法もありますが、総和付き版は `O(nL)` 個の重みの情報を追加します。総和を使用しない場合のメモリとコンパイル時間を増やさないためには、まずは別クラスにする方が単純です。

## 13. 実装状況

- `WaveletMatrixSum` と `WaveletMatrixFenwick` の総和 API と値順探索 API を実装済み
- 通常版と固定ビット幅版の指定された検索 API を実装済み
- `DynamicWaveletTreeSum` の総和、重み更新、値順探索の API を実装済み
- `DynamicWaveletTree` 系の `topk`, `range_select`, `reserve`, `set_key` を実装済み
- `WaveletMatrix2DSum`, `WaveletMatrix2DMin`, `WaveletMatrix2DMonoid` を実装済み
- `DynamicWaveletMatrixSum` は追加せず、同等の用途を `DynamicWaveletTreeSum` に集約

今後の候補は、全実装における `sigma` とビット数の定義の統一、座標圧縮のラッパー、総和以外の値順の集約です。

## 14. テスト項目

### 14.1 共通

- 空配列
- 長さ 1
- `sigma = 1`
- `sigma` が 2 の冪
- `sigma` が 2 の冪でない
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
- 重みが 0
- 重みが負数
- キーが同じで重みが異なる
- `sum_range = sum_lt(upper) - sum_lt(lower)` の一致
- `sum_k_largest` と、全体の総和と `sum_k_smallest` から求めた値の一致
- 非負重みに対する `min_count_smallest_sum_ge`
- 非負重みに対する `min_count_largest_sum_ge`

### 14.3 動的版

ナイーブな `vector` と比較するランダムテストを行います。

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

### 14.4 2 次元版

- x, y が負の点
- 同一座標への複数登録
- y が同じ点の安定順序
- 長方形の個数、総和、`kth_y`、先頭 k 点の総和
- 最小値、最大値と、値が同じ場合の登録順
- 可換モノイド版とナイーブ実装の一致

### 14.5 実装間比較

同じ配列に対して次の実装の結果が一致することを確認します。

- `WaveletMatrix`
- `wavelet_matrix_bit.cpp` の固定ビット幅版
- `DynamicWaveletMatrix`
- `DynamicWaveletTree`
- ナイーブ実装

総和付き版についても、静的版と動的版を同じクエリ列で比較します。

## 15. まとめ

検索専用版と集約版を分離したまま、静的総和、重みを更新できる静的総和、動的総和と、2 次元の総和、最小値、可換モノイドを整備しました。

値順の述語探索は専用の Result 型を定義せず、`tuple<int, T, W>` で `(count, boundary_value, aggregate)` を返します。重み付き分位点、元配列上の位置の探索と、通常版、固定ビット幅版、動的 Wavelet Tree の指定された API も追加済みです。

2 次元版は、1 回の登録を 1 点として扱う多重集合の仕様に統一しました。動的総和は `DynamicWaveletTreeSum` に集約し、`DynamicWaveletMatrixSum` は追加しません。
