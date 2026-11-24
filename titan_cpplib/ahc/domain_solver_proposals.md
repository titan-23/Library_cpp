# AHC 向け問題領域ライブラリの追加候補

## 目的

`tsp/` や `clustering/` のように、特定の一問に特化せず、AHC で繰り返し出題される種類の問題をまとめたライブラリを追加する場合の候補を整理します。

ここで想定するのは単独のアルゴリズムや補助データ構造ではなく、次の要素を一式で提供する問題領域モジュールです。

- 問題入力を保持する `Problem`
- 解、逆引き、評価値、不変条件をまとめた `State`
- 差分評価できる `Move`
- 複数の初期解構築法
- 局所探索や問題領域固有の改善法
- 正当性テスト、brute force との比較、ベンチマーク
- 利用方法、適用範囲、計算量を説明する文書

## 選定基準

候補は次の観点で評価します。

1. 複数の AHC で再利用できるか
2. 問題の定義と初版の対象範囲を明確に定めることができるか
3. 状態と近傍操作を差分更新できるか
4. 汎用化によって定数倍が大きく悪化しないか
5. 小規模な厳密解や単純な実装と比較して検証できるか
6. 既存の TSP、クラスタリング、グラフアルゴリズムと役割が重複しないか

## 候補一覧

| 優先度 | モジュール | 主な用途 | 実装難度 |
|---:|---|---|---:|
| 1 | `graph_partition/` | 均衡グラフ分割、領域分割、担当の割当 | 中～高 |
| 2 | `assignment/` | 容量付き資源配分、item から bin や agent への割当 | 中 |
| 3 | `quadratic_assignment/` | 相互作用を考慮した離散配置、グラフレイアウト | 中 |
| 4 | `coverage/` | Set Cover、Budgeted Maximum Coverage | 中 |
| 5 | `routing/` | CVRP など容量制約付きの配送 | 高 |
| 6 | `scheduling/` | Classical Job-shop Scheduling | 高 |
| 7 | `rectangle_packing/` | 固定幅の軸平行矩形 packing | 高 |
| 8 | `facility_location/` | 離散的な施設開設問題 | 中～高 |

優先する観点ごとの有力な候補は次のとおりです。

- 独自性を優先する場合: `graph_partition/`
- 適用範囲の広さを優先する場合: `assignment/`
- 短期間で明確な一式を完成させる場合: `quadratic_assignment/` または `coverage/`

## 1. 均衡グラフ分割

### 対象問題

無向重み付きグラフの頂点を `k` 個の part に分割し、各 part の容量制約を満たしながら、異なる part 間にまたがる辺の重みを最小化します。

座標クラスタリングが「点と中心の距離」を扱うのに対し、グラフ分割は「どの関係を切断するか」を扱うため、既存の `clustering/` とは別の問題領域になります。

### 初版の範囲

- 辺の重みが非負の無向グラフ
- 頂点ごとに一種類の非負の重み
- part ごとの重みの下限と上限
- cut edge の重みの総和を最小化
- 必要な場合は頂点と part の組に対する単項コストを追加

有向グラフ、hypergraph、各 part の連結性制約、多次元容量は初版に含めません。
容量の上下限の明らかな矛盾は構築時に検出し、構築法で実行可能解を得られなかった場合も、成功した State とは区別して返します。

### 状態

- `part_of[v]`
- part ごとのメンバーと総重量
- 現在の cut cost
- 境界頂点の集合
- 必要に応じて、頂点から各 part へ接続する辺の重みの合計

### 初期解と近傍

- balanced random
- incremental-cost greedy
- region growing
- 頂点の relocate
- 異なる part 間の swap
- 厳密な人数制約向けの短い cycle move

1 頂点の移動は基本的に `O(deg(v))` で評価および適用できます。

### 探索

- 境界頂点だけを対象とする first/best improvement
- 容量制約付き Fiduccia-Mattheyses 法
- Kernighan-Lin 型の交換
- 後段で coarsen、初期分割、uncoarsen、refine からなる multilevel 法

CSR とフラット配列を基本とします。part 数に比例するキャッシュは、part 数が小さい場合に限定した派生としてベンチマーク後に追加します。

## 2. 一般化割当と容量付き資源配分

### 対象問題

各 item を一つの bin、機械、担当者などに割り当て、容量制約を満たしながら割当コストを最小化します。

密な 1 対 1 割当を解く Hungarian 法や、中心との距離を最小化する balanced K-means とは異なり、任意の割当コスト、item ごとの使用量、bin の容量を扱います。

### 初版の範囲

- item は一つの bin に所属します。未割当を許す場合だけ専用の状態を使用します。
- 一次元の容量
- item と bin の組ごとの割当コストと使用量
- 禁止する割当
- 未割当を許す場合のペナルティ

bin の使用に対する固定コスト、bin 内の順序、経路、item 間の相互作用、多次元容量は初版に含めません。
bin の開設と固定コストは `facility_location/` の責務とします。

### 状態

- `bin_of[item]`
- bin ごとのメンバー
- item を `O(1)` で削除するためのメンバー内の位置
- bin ごとの使用量
- 割当コスト、制約違反量

### 初期解と近傍

- cheapest feasible assignment
- best-fit decreasing
- regret greedy
- コストを優先して割り当てた後の feasibility repair
- relocate
- swap
- 2-for-1 exchange
- 短い ejection chain
- ruin-recreate

容量が一次元で、直接参照できる item-bin 表を使用する場合、relocate と swap は定数時間で評価できます。
単位需要など厳密に解くことができる小さい派生では、最小費用流をテスト用の基準解として利用できます。

## 3. Quadratic Assignment と離散レイアウト

### 対象問題

item を slot に単射で配置し、item 間の相互作用と slot 間の距離によるコストを最小化します。

代表的な目的関数は次の形になります。

```text
sum interaction[i][j] * distance[position[i]][position[j]]
```

グラフ描画、設備配置、座席配置、通信量を考慮した配置などに利用できます。
既存の Hungarian 法は単項の割当コストだけを扱うため、pairwise interaction のある QAP とは重複しません。

### 初版の範囲

- item 数と slot 数が等しい全単射
- 非対称な interaction と distance にも対応
- 任意で item と slot の単項コストを追加
- 容量、未配置、複数の item の同一 slot への配置は扱いません。

### 状態と近傍

- `slot_of[item]`
- `item_at[slot]`
- 現在の総コスト
- 2 点 swap
- 3-cycle

疎な interaction graph では入辺と出辺の双方向の CSR を保持します。`deg(v)` を入次数と出次数の和とすれば、swap を `O(deg(u) + deg(v))` で評価できます。密な行列では `O(n)` で評価します。
密行列は一次元の連続領域に格納し、対称性などの条件は構築時に固定してホットパスの分岐を減らします。

### 探索

- random permutation
- interaction と slot の中心性を使用した greedy
- 線形近似を Hungarian 法で解く初期解
- sampled-swap SA
- first/best improvement
- tabu search

全 swap の delta cache を保持する版は `O(n^2)` のメモリを必要とするため、軽量な State とは分離します。

## 4. Set Cover と Maximum Coverage

### 対象問題

候補集合ごとのコストと被覆対象を入力とし、次のいずれかを解きます。

- すべての要素を被覆する集合のコストを最小化する Set Cover
- 個数または予算の範囲で被覆価値を最大化する Maximum Coverage

### 初版の範囲

- 集合から要素、要素から集合への双方向の CSR
- 要素の非負の重み、集合の正のコスト
- 選択状態、各要素の被覆数、使用予算、現在の目的値
- lazy greedy
- gain/cost greedy
- randomized greedy
- redundant set removal
- add、remove、1-for-1、1-for-2 exchange
- ruin-rebuild

add または remove の提案の評価時間は、対象集合の要素数に比例します。採択後も被覆数だけを更新し、他の全集合の正確な限界利得は更新しません。lazy greedy の heap には古い利得を上界として保持し、取り出した集合だけを再評価します。この heap は集合を追加する初期構築専用とし、remove や ruin の後は破棄または再構築します。密な入力向けの bitset 版と疎な入力向けの CSR 版は、同じホットパスに実行時の分岐を入れず、別の実装にする方が適切です。

実装範囲が明確で、小さい入力の全探索と比較しやすいため、比較的短期間で完成度を上げやすい候補です。

## 5. CVRP と容量制約付き配送

### 対象問題

固定の depot、顧客の需要、車両の容量がある Capacitated Vehicle Routing Problem を扱います。

既存の `MultipleTspState` は複数の巡回路の状態と区間操作を提供しています。ただし、各 route に異なる depot が必要であるため、通常の単一 depot の CVRP をそのまま表現できません。物理的な depot を車両数だけ複製する adapter を用意するか、共有 depot に対応した route core を抽出し、既存の差分操作を再利用する方針が必要になります。

### 初版の範囲

- 全顧客への訪問が必須
- 固定の depot
- 一種類の需要と車両容量
- 固定または上限付きの車両数
- 非負かつ対称な辺のコスト

time window、pickup and delivery、optional prize、複数の depot は初版に含めません。

### 初期解と近傍

- Clarke-Wright savings
- cheapest insertion、regret insertion
- route 内の 2-opt、Or-opt
- route 間の relocate、exchange
- 2-opt star、cross exchange
- capacity repair
- ruin and recreate

経路操作の追加だけでは既存の TSP との境界が曖昧になります。独立したモジュールにする場合は、容量制約を満たす構築法と solver まで含めます。

## 6. Classical Job-shop Scheduling

スケジューリング全般は制約と目的関数の差が大きいため、初版は非 preemptive な classical job-shop の makespan 最小化だけを扱います。

- 各 job は順序が固定された複数の operation からなります。
- 各 operation の machine と処理時間は固定します。
- machine ごとの operation の順序、各 operation の開始時刻と終了時刻、makespan を State に保持します。
- dispatch rule による初期解を用意します。
- critical path と critical block を抽出します。
- critical block 内の隣接 swap、端点移動、短い insert を近傍とします。
- 局所探索と tabu search を提供します。

初版には release time、sequence-dependent setup、納期、preemption、多目的を含めません。
独立した job の並列機械への割当は、順序が目的値に影響しない場合が多く `assignment/` に近いため、別の候補とします。

## 7. 二次元矩形パッキング

### 初版の範囲

- 軸に平行な固定サイズの矩形
- 整数座標
- 重なり禁止
- 90 度回転はオプション
- 容器の幅を固定し、packing height を最小化する strip packing

### 構成候補

- shelf、skyline、bottom-left による初期解
- packing order と decoder、または sequence pair
- 順列の swap、insert、rotation
- compaction
- 衝突判定と正当性の確認

表現と decoder が性能を大きく左右し、近傍ごとの差分 decode も困難です。まず単純で正しい decoder と brute force 用の小規模な検証を作成し、その後に差分化を検討します。

不定形、多角形、三次元、物理的な安定性、可変サイズの矩形は別の問題として扱います。

## 8. 離散 Facility Location

client とは別に与えられる候補地点から施設を選び、各 client を開設した施設に割り当てます。

- 開設コストのある uncapacitated facility location
- greedy add、open、close、facility swap
- client ごとの最良の施設と 2 番目に良い施設のキャッシュ
- add-drop local search

次の問題はそれぞれ別のモジュールで扱います。

- client 自身から固定個数の代表点を選ぶ K-medoids: `clustering/`
- client とは異なる候補地点、可変の開設数、開設の固定コストを扱う問題: `facility_location/`

容量付き施設配置は、単一の API として実装するよりも、`assignment/` の完成後に両者を組み合わせる方が適切です。

## 優先度の低い候補

### QUBO と Ising

疎な二次擬ブール最適化として広い範囲の問題を表現でき、flip delta、SA、tabu search を高速に実装できます。
一方、制約のペナルティ化と係数の調整が利用側の負担になりやすく、問題領域に固有で効果の高い近傍も失われやすくなります。
最初の追加対象にはせず、具体的な利用例が複数得られてから検討します。

### Graph Coloring と Set Packing

DSATUR、Kempe chain、TabuCol や、最大重み独立集合向けの ejection 近傍は明確に実装できます。
ただし現時点では、上位の候補より AHC での適用範囲が狭いと考えられます。

### Multi-Agent Path Finding

prioritized planning、reservation table、conflict repair、CBS などをまとめることができますが、状態と API の規模が大きくなります。
一般の Grid と再利用型の探索 workspace がそろってから着手する方が適切です。

### 制約付き Network Design

terminal を連結しながら辺を選ぶ prize-collecting Steiner network や、予算制約や次数制約のある network は問題領域モジュールにできます。既存の MST や small terminal 向けの Steiner tree を初期解、下界、検証に利用できます。一方、連結条件と目的の派生が多いため、着手時には一つの標準問題に限定します。

## 共通のディレクトリ構成

```text
titan_cpplib/ahc/<domain>/
  <domain>.cpp
  <domain>_moves.cpp
  <domain>_initial_state.cpp
  <domain>_local_search.cpp
  how_to_use_<domain>.md
  test/
  benchmark/
```

基本の API は既存の TSP に近い形が適しています。

- `Problem` は入力とコスト関数を保持します。
- `State` は解、逆引き、集計値、現在のスコアを一体で管理します。
- `make_*_move` または `try_*` は状態を変更せず、差分付きの `Move` を返します。
- 採択後だけ `apply` します。
- `Move` は作成元の State の revision を保持し、状態変更後の古い Move を検出します。
- 初期解と局所探索は State から分離します。
- 問題固有の State を既存の SA に組み込むことができるようにします。

ホットパスでは動的確保、仮想関数、`std::function` を避けるようにします。疎と密、静的と動的などで最適なメモリ配置が異なる場合は、実行時の設定を増やすのではなく、別の実装またはテンプレートで分けます。

## 推奨ロードマップ

### 第一段階

次のどちらかを選びます。

- 独自性と将来の強さを重視する場合: `graph_partition/`
- 適用範囲と他モジュールからの再利用を重視する場合: `assignment/`

### 第二段階

- `quadratic_assignment/`
- `coverage/`

両者とも対象範囲が明確であり、差分計算と、小規模な厳密解による検証がしやすい候補です。

### 第三段階

- 既存の TSP を基礎とする `routing/`
- classical job-shop に限定した `scheduling/`

### 第四段階

- `rectangle_packing/`
- `facility_location/`

これらは既存モジュールとの境界や解の表現の選択が性能に強く影響するため、先に設計を確定します。

## 現時点の結論

最初の候補には `graph_partition/` または `assignment/` が適しています。

- `graph_partition/` はクラスタリングでは扱うことができない関係グラフを直接最適化でき、独自の価値が大きい候補です。
- `assignment/` は割当、容量、修復という多くの AHC に共通する構造を提供でき、他のモジュールの基盤にもなります。
- `quadratic_assignment/` は離散配置問題として境界が明確で、定数倍を改善しやすい候補です。
- `coverage/` は比較的少ない作業量で完成度の高い問題領域ライブラリにできます。

万能な最適化 State や、任意の制約と任意の近傍を登録する solver は作成しません。問題領域ごとの State と Move を確定し、複数のモジュールで実際に共通する部分だけを後から抽出します。
