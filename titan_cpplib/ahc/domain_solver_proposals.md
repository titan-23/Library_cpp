# AHC向け問題領域ライブラリの追加候補

## 目的

`tsp/`や`clustering/`のように、特定の一問へ特化せず、AHCで繰り返し現れる問題族をまとめた
ライブラリを追加する場合の候補を整理する。

ここで想定するのは、単独のアルゴリズムや補助データ構造ではなく、次を一式で提供する
問題領域モジュールである。

- 問題入力を保持する`Problem`
- 解、逆引き、評価値、不変条件をまとめた`State`
- 差分評価できる`Move`
- 複数の初期解構築法
- 局所探索や問題領域固有の改善法
- 正当性テスト、brute forceとの比較、ベンチマーク
- 利用方法、適用範囲、計算量を説明する文書

## 選定基準

候補は次の観点で評価する。

1. 複数のAHCで再利用できるか
2. 問題の定義と初版の対象範囲を明確に切れるか
3. 状態と近傍操作を差分更新できるか
4. 汎用化によって定数倍が大きく悪化しないか
5. 小規模な厳密解や単純実装と比較して検証できるか
6. 既存のTSP、クラスタリング、グラフアルゴリズムと役割が重複しないか

## 候補一覧

| 優先度 | モジュール | 主な用途 | 実装難度 |
|---:|---|---|---:|
| 1 | `graph_partition/` | 均衡グラフ分割、領域分割、担当分け | 中～高 |
| 2 | `assignment/` | 容量付き資源配分、itemからbinやagentへの割当 | 中 |
| 3 | `quadratic_assignment/` | 相互作用を考慮した離散配置、グラフレイアウト | 中 |
| 4 | `coverage/` | Set Cover、Budgeted Maximum Coverage | 中 |
| 5 | `routing/` | CVRPなど容量制約付き配送 | 高 |
| 6 | `scheduling/` | Classical Job-shop Scheduling | 高 |
| 7 | `rectangle_packing/` | 固定幅の軸平行矩形packing | 高 |
| 8 | `facility_location/` | 離散的な施設開設問題 | 中～高 |

独自性を優先するなら`graph_partition/`、適用範囲の広さを優先するなら`assignment/`、
短期間で明確な一式を完成させるなら`quadratic_assignment/`または`coverage/`が有力である。

## 1. 均衡グラフ分割

### 対象問題

無向重み付きグラフの頂点を`k`個のpartへ分割し、各partの容量制約を満たしながら、
異なるpart間にまたがる辺の重みを最小化する。

座標クラスタリングが「点と中心の距離」を扱うのに対し、グラフ分割は
「どの関係を切断するか」を扱うため、既存の`clustering/`とは別の問題領域になる。

### 初版の範囲

- 非負の辺重みを持つ無向グラフ
- 頂点ごとに一種類の非負の重み
- partごとの重みの下限と上限
- cut edgeの重みの総和を最小化
- 必要なら頂点とpartの組に対する単項費用を追加

有向グラフ、hypergraph、各partの連結性制約、多次元容量は初版に含めない。
容量上下限の明らかな矛盾は構築時に検出し、構築法で実行可能解を得られなかった場合も
成功したStateとは区別して返す。

### 状態

- `part_of[v]`
- partごとのメンバー、総重量
- 現在のcut cost
- 境界頂点集合
- 必要に応じて、頂点から各partへ接続する辺重みの合計

### 初期解と近傍

- balanced random
- incremental-cost greedy
- region growing
- 頂点のrelocate
- 異なるpart間のswap
- 厳密な人数制約向けの短いcycle move

1頂点の移動は、基本的に`O(deg(v))`で評価、適用できる。

### 探索

- 境界頂点だけを対象とするfirst/best improvement
- 容量制約付きFiduccia-Mattheyses法
- Kernighan-Lin型の交換
- 後段でcoarsen、初期分割、uncoarsen、refineからなるmultilevel法

CSRとフラット配列を基本とし、part数に比例するキャッシュは、小さいpart数専用の派生として
ベンチマーク後に追加する。

## 2. 一般化割当・容量付き資源配分

### 対象問題

各itemを一つのbin、機械、担当者などへ割り当て、容量制約を満たしながら割当費用を最小化する。

密な1対1割当を解くHungarian法や、中心との距離を最小化するbalanced K-meansとは異なり、
任意の割当費用、itemごとの使用量、bin容量を扱う。

### 初版の範囲

- itemは一つのbinへ所属する。未割当を許す場合だけ専用の状態を使う
- 一次元の容量
- itemとbinの組ごとの割当費用と使用量
- 禁止割当
- 未割当を許す場合のペナルティ

binの使用固定費、bin内の順序、経路、item間の相互作用、多次元容量は初版に含めない。
binの開設と固定費は`facility_location/`の責務とする。

### 状態

- `bin_of[item]`
- binごとのメンバー
- itemを`O(1)`で削除するためのメンバー内位置
- binごとの使用量
- 割当費用、制約違反量

### 初期解と近傍

- cheapest feasible assignment
- best-fit decreasing
- regret greedy
- 費用を優先して割り当てた後のfeasibility repair
- relocate
- swap
- 2-for-1 exchange
- 短いejection chain
- ruin-recreate

一次元容量と直接参照できるitem-bin表を使う場合、relocateとswapは定数時間で評価できる。
単位需要など厳密に解ける小さい派生では、最小費用流をテスト用の基準解として利用できる。

## 3. Quadratic Assignment・離散レイアウト

### 対象問題

itemをslotへ単射配置し、item間の相互作用とslot間の距離による費用を最小化する。

代表的な目的関数は次の形になる。

```text
sum interaction[i][j] * distance[position[i]][position[j]]
```

グラフ描画、設備配置、座席配置、通信量を考慮した配置などへ利用できる。
既存のHungarian法は単項の割当費用だけを扱うため、pairwise interactionを持つQAPとは重複しない。

### 初版の範囲

- item数とslot数が等しい全単射
- 非対称なinteractionとdistanceにも対応
- 任意でitemとslotの単項費用を追加
- 容量、未配置、複数itemの同一slot配置は扱わない

### 状態と近傍

- `slot_of[item]`
- `item_at[slot]`
- 現在の総費用
- 2点swap
- 3-cycle

疎なinteraction graphでは入辺と出辺の双方向CSRを持つ。`deg(v)`を入次数と出次数の和とすれば、
swapを`O(deg(u) + deg(v))`で評価できる。密な行列では`O(n)`で評価する。
密行列は一次元の連続領域に格納し、対称性などの条件は構築時に固定してホットパスの分岐を減らす。

### 探索

- random permutation
- interactionとslot中心性を使ったgreedy
- 線形近似をHungarian法で解く初期解
- sampled-swap SA
- first/best improvement
- tabu search

全swapのdelta cacheを持つ版は`O(n^2)`メモリを必要とするため、軽量Stateとは分離する。

## 4. Set Cover・Maximum Coverage

### 対象問題

候補集合ごとの費用と被覆対象を与え、次のいずれかを解く。

- すべての要素を被覆する集合の費用を最小化するSet Cover
- 個数または予算の範囲で被覆価値を最大化するMaximum Coverage

### 初版に含めるもの

- 集合から要素、要素から集合への双方向CSR
- 非負の要素重み、正の集合費用
- 選択状態、各要素の被覆数、使用予算、現在の目的値
- lazy greedy
- gain/cost greedy
- randomized greedy
- redundant set removal
- add、remove、1-for-1、1-for-2 exchange
- ruin-rebuild

addまたはremoveの提案評価は、対象集合の要素数に比例する。採択後も被覆数だけを更新し、
他の全集合の正確な限界利得は更新しない。lazy greedyのheapには古い利得を上界として保持し、
取り出した集合だけを再評価する。このheapは集合を追加する初期構築専用とし、removeやruinの後は
破棄または再構築する。密な入力向けbitset版と疎な入力向けCSR版は、同じホットパスへ実行時分岐を
入れず、別実装として持つ方がよい。

実装範囲が明確で、小さい入力の全探索と比較しやすいため、比較的短期間で完成度を上げやすい。

## 5. CVRP・容量制約付き配送

### 対象問題

固定depot、顧客需要、車両容量を持つCapacitated Vehicle Routing Problemを扱う。

既存の`MultipleTspState`は複数巡回路の状態と区間操作を提供している。ただし各routeに異なるdepotを
要求するため、通常の単一depot CVRPをそのまま表現できない。物理depotを車両数だけ複製するadapterを
用意するか、共有depotに対応したroute coreを抽出し、既存の差分操作を再利用する方針が必要になる。

### 初版の範囲

- 全顧客への訪問が必須
- 固定depot
- 一種類の需要と車両容量
- 固定または上限付きの車両数
- 非負かつ対称な辺費用

time window、pickup and delivery、optional prize、複数depotは初版に含めない。

### 初期解と近傍

- Clarke-Wright savings
- cheapest insertion、regret insertion
- route内2-opt、Or-opt
- route間relocate、exchange
- 2-opt star、cross exchange
- capacity repair
- ruin and recreate

単なる経路操作の追加だけなら既存TSPとの境界が曖昧になる。独立モジュールにする場合は、
容量を守る構築法とsolverまで含める。

## 6. Classical Job-shop Scheduling

スケジューリング全般は制約と目的関数の差が大きいため、初版は非preemptiveなclassical job-shopの
makespan最小化だけを扱う。

- 各jobは順序が固定された複数のoperationを持つ
- 各operationのmachineと処理時間は固定する
- machineごとのoperation順、各operationの開始・終了時刻、makespanをStateに持つ
- dispatch ruleによる初期解を用意する
- critical pathとcritical blockを抽出する
- critical block内の隣接swap、端点移動、短いinsertを近傍とする
- 局所探索とtabu searchを提供する

初版にはrelease time、sequence-dependent setup、納期、preemption、多目的を含めない。
独立jobの並列機械割当は順序が目的値へ影響しない場合が多く、`assignment/`に近いため別候補とする。

## 7. 二次元矩形パッキング

### 初版の範囲

- 軸に平行な固定サイズ矩形
- 整数座標
- 重なり禁止
- 90度回転はオプション
- 容器の幅を固定し、packing heightを最小化するstrip packing

### 構成候補

- shelf、skyline、bottom-leftによる初期解
- packing orderとdecoder、またはsequence pair
- 順列swap、insert、rotation
- compaction
- 衝突判定と正当性検査

表現とdecoderが性能を大きく左右し、近傍ごとの差分decodeも難しい。まず単純で正しいdecoderと
brute force用の小規模検証を作り、その後に差分化を検討する。

不定形、多角形、三次元、物理安定性、可変サイズ矩形は別問題として扱う。

## 8. 離散Facility Location

clientとは別に与えられる候補地点から施設を選び、各clientを開設施設へ割り当てる。

- 開設費用を持つuncapacitated facility location
- greedy add、open、close、facility swap
- clientごとの最良・第2施設キャッシュ
- add-drop local search

client自身から固定個数の代表点を選ぶK-medoidsは`clustering/`、clientとは異なる候補地点、
可変の開設数、開設固定費を扱う問題は`facility_location/`と分ける。

容量付き施設配置は、`assignment/`完成後に両者を組み合わせる方が、一枚岩のAPIにするよりよい。

## 後順位の候補

### QUBO・Ising

疎な二次擬ブール最適化として広い問題を表現でき、flip delta、SA、tabu searchを高速に実装できる。
一方、制約のペナルティ化と係数調整を利用側へ押し付けやすく、問題領域固有の強い近傍も失いやすい。
最初の追加対象にはせず、具体的な利用例が複数得られてから検討する。

### Graph Coloring・Set Packing

DSATUR、Kempe chain、TabuColや、最大重み独立集合向けのejection近傍は明確に実装できる。
ただし現時点では、上位候補よりAHCでの横展開が狭いと考える。

### Multi-Agent Path Finding

prioritized planning、reservation table、conflict repair、CBSなどをまとめられるが、状態とAPIが重い。
一般Gridと再利用型探索workspaceが整ってから着手する方がよい。

### 制約付きNetwork Design

terminalを連結しながら辺を選ぶprize-collecting Steiner networkや、予算・次数制約付きnetworkは
問題領域モジュールにできる。既存のMSTやsmall terminal向けSteiner treeを初期解、下界、検証へ
利用できる。一方、連結条件と目的の派生が多いため、着手時には一つの標準問題へ限定する。

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

基本APIは既存TSPに近い形が適している。

- `Problem`は入力と費用関数を保持する
- `State`は解、逆引き、集計値、現在スコアを一体で管理する
- `make_*_move`または`try_*`は状態を変更せず、差分付き`Move`を返す
- 採択後だけ`apply`する
- `Move`は作成元Stateのrevisionを保持し、状態変更後の古いMoveを検出する
- 初期解と局所探索はStateから分離する
- 問題固有のStateを既存SAへ組み込めるようにする

ホットパスでは、動的確保、仮想関数、`std::function`を避ける。疎・密、静的・動的などで
最適なメモリ配置が異なる場合は、実行時設定を増やすのではなく別実装またはテンプレートで分ける。

## 推奨ロードマップ

### 第一段階

次のどちらかを選ぶ。

- 独自性と将来の強さを重視: `graph_partition/`
- 適用範囲と他モジュールからの再利用を重視: `assignment/`

### 第二段階

- `quadratic_assignment/`
- `coverage/`

両者とも対象範囲が明確で、差分計算と小規模厳密解による検証がしやすい。

### 第三段階

- 既存TSPを土台とする`routing/`
- classical job-shopに限定した`scheduling/`

### 第四段階

- `rectangle_packing/`
- `facility_location/`

これらは既存モジュールとの境界や解表現の選択が性能へ強く影響するため、先に設計を固める。

## 現時点の結論

最初の候補は`graph_partition/`または`assignment/`がよい。

- `graph_partition/`はクラスタリングでは扱えない関係グラフを直接最適化でき、独自価値が大きい
- `assignment/`は割当、容量、修復という多くのAHCに共通する構造を提供でき、他モジュールの基盤にもなる
- `quadratic_assignment/`は離散配置問題として境界が明確で、定数倍を詰めやすい
- `coverage/`は比較的小さい投資で完成度の高い問題領域ライブラリにできる

万能な最適化Stateや、任意の制約と任意の近傍を登録するsolverは作らない。問題領域ごとのStateとMoveを
固め、複数モジュールで本当に共通した部分だけを後から抽出する。
