# `bitboard.cpp` 改善提案

## 前提

- 対象は `titan_cpplib/ahc/bitboard.cpp`。
- `H, W <= 128` 程度の盤面を想定する。
- SA・ビームサーチの内側から多数回呼ぶ用途を本命とする。
- 返り値用の `Set` を呼び出し側で確保し、ホットパスでは再確保しない現在の方針は維持する。
- 以下の速度評価には未計測の見積もりを含む。採否は最後に記載するベンチマークで決める。

## 実装結果（2026-07-15）

次を実装した。

- `connected` の早期終了 + 行内一括 flood への切り替え
- Kogge–Stone 型 `row_fill` を使う全成分 flood
- `components`・`label`・`largest_component` の行ラン + Union-Find
- `step`・`border` の 1 パス化と `shift` の分岐整理
- `Set`・単一点を受け取る flood/BFS API
- `distance`・`nearest_in_set`・`component`・`component_size`・`shortest_path`
- 集合判定、方向別 shift、`rect`、`kth_cell`、`hash64`、形態演算、任意マスク表示
- 4 近傍・8 近傍のコンパイル時切り替え
- 入出力サイズ・非エイリアス条件のデバッグ検査

作業バッファの `shared_ptr` 共有は採用しない。競プロ用途では盤面コピーを主要なボトルネックとせず、直接メンバの単純さとホットループのアクセス速度を優先する。固定長版、BMI2 専用選択、Zobrist hash は未実装とする。

## 結論

実装候補の優先順位は次のとおり。

1. `connected` を到達時点で打ち切る。
2. `step` と `border` を 1 パス化する。
3. `flood` 系に `Set`・単一点を受け取るオーバーロードを追加する。
4. 全成分を求める flood に行内一括伝播を使う。
5. `components`・`label`・`largest_component` に行ラン + Union-Find を使う。
6. `bfs_nearest` は追加バッファを増やさず、`frontier` と `next` の役割を整理する。
7. 8 近傍をコンパイル時の近傍種別として正式対応する。

追加メソッドでは、次を優先する。

1. `flood(const Set &, Set &)` などの `Set` オーバーロード
2. `distance`
3. `nearest_in_set`
4. `component`・`component_size`
5. `shortest_path`
6. `intersects`・`is_subset` などの集合判定

## 高速化案

### 1. `Bitboard` のコピーコストを下げる（見送り）

現在の `Bitboard` は次の 5 本の `vector<Word>` を持つ。

- `road`
- `frontier`
- `next`
- `seen`
- `comp`

デフォルトのコピーでは、探索結果として不要な作業バッファまでコピーされる。コピー構築なら最大 5 回の動的確保も発生する。`Bitboard` をビームの状態に直接持たせる場合、探索処理よりこちらが支配的になる可能性がある。

候補は、盤面と作業領域を分離する設計である。

```cpp
template<class Word>
class BitboardWorkspace;

template<class Word>
class Bitboard {
    int H, W;
    Word FULL;
    Set road;
};
```

探索メソッドへ `BitboardWorkspace &` を渡すか、1 インスタンスに 1 ワークスペースを外から関連付ける。これにより状態コピーは実質 `road` の `H` ワードだけになる。また、多数の盤面に対して 1 個のワークスペースを順次使い回せる。

単一の盤面をコピーせず使い続ける用途では効果がない。今回は競プロ用途での単純さを優先し、Workspace 分離や `shared_ptr` 共有は行わない。

### 2. `connected` の早期終了

現在は `a` の連結成分全体を `comp` に構築してから `b` を判定している。次の順に処理すればよい。

1. `a`, `b` の範囲を検査する。
2. どちらかが壁なら `false`。
3. `a == b` なら `true`。
4. 各 BFS 層を作った直後、追加集合と `b` のビットが交われば `true`。
5. frontier が空になれば `false`。

近い 2 点の判定では数パスで終了する。全成分の構築が不要なので、独立した targeted flood として実装するのがよい。

### 3. `step`・`border` の 1 パス化

現在の `step` は `shift` 後に `iand(out, road)` を行うため、`out` を 2 回走査する。シフトのループ内で `road[r]` を掛ければ 1 回で済む。

`border` も同様に、展開結果を作った後でもう一度 `~s[r]` を掛けるのではなく、次を展開ループ内で計算できる。

```cpp
out[r] = neighbor_bits & road[r] & ~s[r];
```

どちらも計算量は変わらないが、呼び出し頻度が高ければメモリパスを半減できる。

### 4. `shift` の分岐をループ外へ出す

現在は行ごとに移動元行の境界判定を行い、行ごとに `dc >= 0` を判定する。次の形に分けられる。

- `dr` から有効な出力行区間を先に求める。
- 範囲外になる行だけゼロクリアする。
- `dc >= 0` と `dc < 0` でループを分ける。
- 頻出する 4 方向には `shift_left/right/up/down` の専用処理を用意する。

`H <= 128` では差が小さい可能性もあるため、1 パス化より優先度は低い。

### 5. 行内一括伝播による flood

現行 flood は 1 パスでグラフ距離 1 だけ進む。各パスで全 `H` 行を走査するため、到達集合の偏心距離を `D` とすると、おおよそ `O(H D)` ワード演算になる。

距離が不要な `flood` では、Kogge–Stone 型の伝播により、1 行の同じ道区間へ始点を一括展開できる。

```cpp
Word row_fill(Word s, Word m) const {
    Word g = s & m;
    Word p = m;
    for (int sh = 1; sh < W; sh <<= 1) {
        g |= p & (g << sh);
        p &= p << sh;
    }

    Word h = s & m;
    p = m;
    for (int sh = 1; sh < W; sh <<= 1) {
        h |= p & (h >> sh);
        p &= p >> sh;
    }
    return g | h;
}
```

この式は、`m` の連続した 1 の区間のうち、`s` と交わる区間全体を返す。幅 1 から 10 までの全 `(m, s ⊆ m)`、合計 88,572 ケースについて単純反復版との一致を確認済みである。

全体の flood は次の緩和を変化がなくなるまで行う。

1. 上から下へ、現在行と一つ上の到達集合を合わせて `row_fill`。
2. 下から上へ、現在行と一つ下の到達集合を合わせて `row_fill`。

開けた盤面なら、ほぼ 1 往復で全体へ届く。計算量は往復回数を `T` として `O(T H log W)`。`T` は経路の縦方向の折り返しに依存し、迷路では大きくなるため、最悪計算量が常に改善するわけではない。

また、小さい成分や近距離の `connected` では、各行に約 `2 log W` 回の演算を行う定数倍が不利になり得る。次のように用途別にカーネルを分けるのが安全である。

- 全成分が必要な `flood`・`component`：行内一括伝播版の候補
- 距離が必要な `bfs_dist`・`distance`：現行の層別 frontier 版
- 近距離で終わりやすい `connected`：早期終了付き層別版も残して比較

8 近傍の場合、上下行から渡すビットを `u | (u << 1) | (u >> 1)` としてから `row_fill` すれば同じ構成を利用できる。

### 6. `largest_component` の枝刈り

最初に `remaining = count(road)` を求める。各成分のサイズ `s` が分かるたびに `remaining -= s` とし、`remaining <= best` なら、未探索部分だけで現在の最大成分を超えることはないので終了できる。

最大成分が走査順の早い位置にある盤面では、後半の flood をまとめて省略できる。最大成分が最後に現れる盤面では効果がなく、最初の `count(road)` が追加コストになる。

成分サイズは flood 後に `count(comp)` でも求められるが、探索中に新規追加ビットの popcount を加算すれば、別の全行走査を省略できる。

### 7. `components`・`label` の行ラン + Union-Find

成分ごとに flood する代わりに、各行の道を極大な連続区間へ分解し、隣接行で重なる区間を Union-Find で結合する方法がある。

ビット位置が列番号と同じ向きなら、ランの低位側端点と高位側端点の候補は次で得られる。

```cpp
Word starts = x & ~(x << 1);
Word ends   = x & ~(x >> 1);
```

区間は列順に保持し、上下行の区間列を two-pointer で比較する。全区間の直積を取らないことが重要である。4 近傍では区間が重なるとき、8 近傍では 1 列ずらした範囲まで接するときに union する。

最大ラン数を `R` とすれば、概ね `O(R alpha(R) + HW)` で成分番号を作れる。`label` は全セルへ番号を書くため、どのみち `O(HW)` の出力コストが必要になる。

利点は成分数や盤面径に処理時間が左右されにくいこと。欠点は次のとおり。

- Union-Find とラン情報の初期化コストがある。
- 開けた盤面の単一成分では、行内一括 flood の方が単純で速い可能性がある。
- ホットパスで使うなら、最大 `H * ceil(W / 2)` 程度のラン用配列を事前確保して再利用する必要がある。

`components`・`label`・`largest_component` の共通バックエンドにできるが、行内一括 flood とどちらが速いかは盤面分布ごとに比較して決める。

### 8. `bfs_nearest` の前層判定

別AI案では `prev` の `Set` を 1 本追加して、前層所属を `dist[nidx] == layer - 1` の代わりにビットで判定する案が挙げられている。方向性はよいが、追加バッファは不要である。

現在の `frontier` を前層のまま保持し、`next[r]` を新規セルだけに絞った後、全セルの `src` を決定してから `frontier.swap(next)` すればよい。

```cpp
expand_into(frontier, next);
for (int r = 0; r < H; ++r) {
    next[r] &= ~seen[r];
    seen[r] |= next[r];
}

// next の各セルについて、frontier に属する近傍だけを見る。
// src の最小値を選ぶ処理は残る。

frontier.swap(next);
```

これにより `dist[nidx]` のランダムアクセスは消せる。ただし、最小の始点番号を選ぶための `src[nidx]` 参照は必要であり、完全なビット並列化にはならない。

4 方向分の `shift(frontier, ...)` を毎層作る案は、`4H` の追加走査が発生する。`H <= 128` では直接 4 近傍を見る方が速い可能性が高いので、まずは frontier のビット判定だけを変更する。

### 9. 探索対象行の限定

frontier が存在する最小・最大行を持ち、次の展開ではその範囲の前後 1 行だけ処理する。局所探索や `flood_limited` には効くが、`H <= 128` かつ盤面全体へ広がる探索では、範囲管理の分岐が相殺する可能性がある。

優先度は低めとし、層別 frontier 版の追加最適化として測定する。

### 10. 固定長版

盤面サイズがコンパイル時に決まる用途向けに、次の別名または別クラスを用意する案である。

```cpp
template<int H, class Word, Neighborhood NB = Neighborhood::Four>
class StaticBitboard;
```

`std::array<Word, H>` により動的確保をなくし、コンパイラによるループ展開を期待できる。一方、コード量と型の種類が増える。動的版のボトルネックを測定してからでよい。

## 追加メソッド案

### 優先度 A

#### `Set`・単一点を始点に取る探索

```cpp
void flood(const Set &sources, Set &out) const;
void flood(int r, int c, Set &out) const;
void flood_limited(const Set &sources, int max_steps, Set &out) const;
void bfs_dist(const Set &sources, vector<int> &dist) const;
```

`border` や集合演算の結果を始点にする場合、いったん座標列へ戻す必要がなくなる。単一点版は一時的な `vector<pair<int,int>>` を作らずに済む。

#### 点間距離

```cpp
int distance(pair<int,int> a, pair<int,int> b) const;
```

層別 frontier を進め、`b` に初めて到達した層を返す。到達不能またはどちらかが壁なら `-1`。`connected` と探索本体を共有できる。

#### 集合までの最短距離

```cpp
int nearest_in_set(
    int r,
    int c,
    const Set &targets,
    pair<int,int> &hit
) const;
```

各層で `frontier` と `targets` の共通部分を見る。複数候補が同距離の場合の規則を、例えば行優先・列優先の最小セルと明記する。始点集合から対象集合までの一般形も候補になる。

#### 連結成分取得

```cpp
void component(int r, int c, Set &out) const;
int component_size(int r, int c) const;
```

現在 private にある単一点 flood を公開 API として使える形にする。`component_size` は探索中に popcount を加算し、成分完成後の `count` を避ける。

### 優先度 B

#### 最短経路復元

```cpp
bool shortest_path(
    pair<int,int> a,
    pair<int,int> b,
    vector<pair<int,int>> &path
) const;
```

`b` から距離を計算し、`a` から距離が 1 ずつ減る方向へ進めば復元できる。全盤面の BFS を完了させず、`a` に到達した層で距離計算を止められる。辞書順など、複数の最短路がある場合の方向優先順位を明記する。

#### 集合判定

```cpp
bool intersects(const Set &a, const Set &b) const;
bool disjoint(const Set &a, const Set &b) const;
bool is_subset(const Set &a, const Set &b) const;
```

いずれも条件が確定した行で打ち切れる。探索の終了条件として頻繁に利用できる。

#### 方向別シフト

```cpp
void shift_left(const Set &s, Set &out) const;
void shift_right(const Set &s, Set &out) const;
void shift_up(const Set &s, Set &out) const;
void shift_down(const Set &s, Set &out) const;
```

汎用 `shift` より呼び出し側の意図が明確で、内部の分岐も不要になる。

#### 任意マスクの表示

```cpp
void print(const Set &s, ostream &os) const;
```

現在の `operator<<` は `road` しか表示できないため、frontier や連結成分の調査に使う。

### 優先度 C

#### k 番目のセル

```cpp
bool kth_cell(const Set &s, int k, int &r, int &c) const;
```

`k` は 0-indexed とし、範囲外なら `false`。集合から一様ランダムに 1 セル選ぶ処理に使える。BMI2 の `pdep` は有効だが、コンパイルオプションや実行 CPU に依存するため、通常実装を必ず用意し、`__BMI2__` などで切り替える。

#### ハッシュ

```cpp
uint64_t hash64(const Set &s) const;
uint64_t hash64() const;  // road
```

行番号と行ビットを 64bit mixer へ通す。ビームサーチの重複除去に使えるが、衝突はあり得る。厳密な同一性が必要なら、ハッシュ一致後に `equals` で確認する。

盤面を 1 セルずつ更新して毎回ハッシュを求める用途では、`O(H)` で再計算するより Zobrist hash を `set_road`・`set_wall` と同時更新する設計も候補になる。

#### 形態演算

```cpp
void dilate4(const Set &s, Set &out) const;
void erode4(const Set &s, Set &out) const;
void inner_border4(const Set &s, Set &out) const;
void dilate4(const Set &s, int k, Set &out) const;
```

次を明確に区別する必要がある。

- 盤面内だけで行う通常の形態演算
- `road` に制限した geodesic dilation。現在の `flood_limited` に相当するもの

`erode` では盤面外を 0 と扱うかどうかも仕様に書く。

#### 矩形マスク

```cpp
void rect(int r1, int c1, int r2, int c2, Set &out) const;
```

他 API と合わせて半開区間 `[r1, r2) x [c1, c2)` とする。この場合、列マスクは次である。

```cpp
Word cols = lowmask(c2) ^ lowmask(c1);
```

別AI案にあった `lowmask(c2 + 1)` は、`c2` を含む閉区間の場合の式であり、半開区間 API には使わない。

#### 成分列挙

```cpp
template<class F>
int for_each_component(F &&f) const;
```

成分ごとに `Set` を `vector` へ保存せず、その場で処理できる。成分数や最大成分以外の統計を取る用途に向く。

## 8 近傍対応

コメントを差し替えて 4 近傍と 8 近傍を切り替える方式は、`DR/DC` と `dirs` の不一致を起こしやすい。コンパイル時の近傍種別として型に含める。

`bool DIAG` でも実現できるが、呼び出し側で `true` の意味が読みにくいため enum の方が明確である。

```cpp
enum class Neighborhood {
    Four,
    Eight,
};

template<class Word, Neighborhood NB = Neighborhood::Four>
class Bitboard;
```

`expand_into` と方向数は `if constexpr` で分岐する。デフォルトを 4 近傍にすれば、既存の `Bitboard<Word>` という利用方法は維持できる。

```cpp
using Bitboard64 = Bitboard<uint64_t, Neighborhood::Four>;
using Bitboard64Diag = Bitboard<uint64_t, Neighborhood::Eight>;
```

`connected`・`components`・`label`・`bfs_dist`・`bfs_nearest` が同じ近傍定義を必ず使うようにする。

## API と安全性の整理

### 入出力のエイリアス

`expand`・`shift`・`border` は、入力 `s` と出力 `out` が同じ `Set` の場合に正しく動かない。特に行方向の `shift` は、書き換え済みの行を後から入力として読む可能性がある。

ホットパスで毎回エイリアス対応の分岐や一時コピーを入れるより、次を契約として明記する方が現在の設計に合う。

```cpp
// s と out は異なるバッファでなければならない。
```

デバッグビルドでは `assert(&s != &out)` を入れる。`band` など添字ごとに完結する演算については、in-place を許可できる。

### `expand` の意味

現在の `expand` は元集合自身を含まず、道上の隣接セルだけを返す。名前だけでは判別しにくいため、次のどちらかを行う。

- `neighbors4` を追加し、`expand` は互換性のため残す。
- 元集合も含む `dilate4` を別に追加する。

### `clear`・`fill` の名前

現在は `clear()` が全セルを道、`fill()` が全セルを壁にする。一般的なコンテナの感覚と逆なので、互換性を維持しつつ次の別名を追加すると誤用しにくい。

```cpp
void open_all();
void block_all();
```

### 型・サイズの制約

次を明示または検査する。

- `h >= 0`
- `0 <= w && w <= word_bits()`
- `Word` は符号なし整数で、実質 `uint64_t` または `__uint128_t`
- 外部から渡す `Set` の長さは `H`

## 採用前のベンチマーク

### 盤面

- 全て道
- 道密度 20%, 50%, 80% のランダム盤面
- checkerboard。成分数が多いケース
- 1 本の蛇行通路。flood のパス数が多いケース
- 小さい孤立成分が多数あるケース
- 大成分 1 個と小成分多数。`largest_component` の枝刈り向け

### サイズ

- `16 x 16`
- `32 x 32`
- `64 x 64`
- `128 x 128`
- `Word = uint64_t` と `Word = __uint128_t` の両方

### 操作

- `connected`：近距離、遠距離、非連結
- `flood`：単一点、複数点、`Set` 始点
- `flood_limited`：小さい `max_steps`
- `components`
- `label`
- `largest_component`
- `bfs_dist`
- `bfs_nearest`
- `step`・`border`
- `Bitboard` のコピー。状態に直接持たせる場合

### 比較する実装

- 現行の層別 frontier
- 有効行範囲を追跡する層別 frontier
- 行内一括伝播 + 上下 sweep
- 行ラン + Union-Find。成分系のみ

平均時間だけでなく、次も記録する。

- 1 呼び出し当たりの ns
- flood のパス数
- 処理した行数
- 動的確保回数。ホットパスでは 0 が目標
- 盤面分布ごとの最悪時間

速度測定とは別に、単純なスカラー BFS・Union-Find を正解実装としてランダム差分テストを行う。4 近傍・8 近傍、`W = 64`・`W = 128`、盤面端、空集合、全壁、全道路を含める。

## 推奨する実装順

1. ベンチマークとスカラー正解実装を用意する。
2. `connected` の早期終了、`step`・`border` の 1 パス化を行う。
3. `Set`・単一点オーバーロード、`distance`、`nearest_in_set` を追加する。
4. 行内一括伝播版を追加し、現行 flood と盤面別に比較する。
5. `largest_component` の枝刈りと探索中のサイズ集計を試す。
6. 行ラン + Union-Find を成分系だけに追加して比較する。
7. `bfs_nearest` の frontier 保持方式を変更する。
8. 近傍種別をテンプレート引数にする。
9. 実利用で盤面コピーが多いなら Workspace 分離を行う。
10. 固定長版、BMI2、Zobrist hash などは計測結果を見て追加する。

一度に全案を入れず、各段階で正しさと速度を確認する。特に行内一括伝播と行ラン + Union-Find は同じ成分系操作を高速化する競合案なので、両方を恒久的に保守する前に、対象となる盤面分布で優位な方を決める。
