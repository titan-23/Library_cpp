# ビームサーチ探索木の構造監査

## 目的と範囲

固定深さ版、Action 合成版、圧縮木版、可変ターン版について、探索木の表現と状態の走査方法だけを比較する。
特定の問題を速くするためではなく、汎用ライブラリとして採用できる条件、採用できない条件、不変条件を明確にする。

対象は次の4実装である。

- `titan_cpplib/ahc/beam_search/beam_search.cpp`
- `titan_cpplib/ahc/beam_search/beam_search_compose.cpp`
- `titan_cpplib/ahc/beam_search/beam_search_radix.cpp`
- `titan_cpplib/ahc/beam_search/beam_search_turn.cpp`

`old/` と問題固有の `State` は対象外とした。本文は実装前の構造監査として固定し、その後に32 bit slot-only版、
direct parent oracle、cand導出parent版を別ファイルへ実装した。
現在の結果は `decision.md` と `benchmark_results.md` を参照する。
行番号は監査時点のものなので、コメント整備後に多少ずれる可能性がある。

通常版の後継構造に絞った再監査は [postorder_successor.md](./postorder_successor.md) と
[postorder_external.md](./postorder_external.md) に記録した。以下で LCA と呼ぶ箇所は葉間の共通祖先を表す説明であり、
現行版を一般 LCA data structure とみなしたり、現行版の最速性を主張したりするものではない。

## 記号

| 記号 | 意味 |
|---|---|
| `W` | その世代の最終生存候補数 |
| `L` | 現在の葉数 |
| `P` | 1 世代またはメタターンの候補に現れる異なる親の数 |
| `F` | 可変ターン版で同時に active な `target_turn` 別候補プール数 |
| `K` | 可変ターン版で、そのメタターンに実際に展開する葉数 |
| `D` | 確定接頭辞より先の最大深さ |
| `E` | 生存葉への経路の和集合に含まれる未圧縮 Action 辺数 |
| `N` | 現在の表現で live な node 数 |
| `Q` | PRE/POST/leaf 形式の平坦木に含まれるトークン数 |
| `R` | スキップ後に実際に読んだ平坦木トークン数 |
| `T` | `tour` に格納する ActionId 数 |
| `X` | 葉を巡回するために呼ぶ `apply_op()` と `rollback()` の合計 |
| `G` | 未解放の世代ブロックに確保済みの Action スロット総数 |
| `A` | 1 世代またはメタターン中の一時採用イベント数 |
| `S_A` | `sizeof(Action)` byte。Action 自身が所有するヒープ領域は含まない |
| `S_sort` | 親内の候補整列の合計コスト。最悪 `O(A log A)` |

`E`、`N`、`G` は異なる。圧縮後は1 node が複数の未圧縮辺を表し得る。後から子孫を失った Action も
世代 block に残るため、`G` は `E` や `N` よりかなり大きくなり得る。

## 結論

1. 標準版は、毎世代 root から全木を PRE/POST 走査する実装ではない。`tour`、`leaf`、`trace` を使う、
   traP の記事にある帰りがけ順の高速化後の方式である。これは現行構造の確認であり、最速性の主張ではない。
2. 標準版の LCA 区間走査は1世代全体で償却 `O(L)` であり、`O(W^2)` ではない。一般 LCA、RMQ、
   binary lifting を追加しても、必須の `apply_op()` / `rollback()` は減らない。
3. 隣接LCPと世代別parent-mapで `tour` を除く方式は比較実装まで完了した。選択後のsurvivor-parentだけで
   compact tourを遅延構築する方式は未実装である。前者は依存load、後者はsnapshotと再読を増やすため、
   現行の連続配列より常に速いとは限らない。
4. `beam_search_compose.cpp` は単一子 Action の呼び出しを ghost 化して省略するが、論理スロットと `tour` は圧縮しない。
   `beam_search_radix.cpp` は明示親木、単一子縮約、根の一本道確定、親バケットを既に実装している。
   圧縮木や仮想木を新案として重ねて実装するのは重複である。
5. 葉＋LCA方式の構造的な効果が最も大きい可能性があるのは可変ターン版である。`K << L` のとき、現在の
   平坦木を毎回走査・再構築せず、ターン別カレンダーから対象葉だけを取り出す親ポインタ方式に余地がある。
   ただし DFS 順の維持、後から追い出された未来葉の削除、ActionId の寿命管理が難しく、疎な場合だけ使う
   別ポリシーまたは疎密ハイブリッドとして検討すべきである。

## 現行4実装

### 固定深さの標準版

主要な構造は `beam_search.cpp:39-69` の世代ブロック、`trace`、`tour`、`leaf` である。世代確定時に
`Candidates` の Action を `gblock[gen]` へ移し、64 bit の `(gen, slot)` を ActionId として保持する
（`beam_search.cpp:83-99`）。

探索中は `cand` を `(parent_leaf, score)` で整列し、逆順に処理する（`beam_search.cpp:344-417, 453-457`）。
直前の親葉位置 `li` から次の `parent_leaf` まで `leaf` を逆走査して LCA 距離を求め、LCA まで rollback し、
別の葉への経路を `trace` に復元して apply する（`beam_search.cpp:347-369`）。これは葉間 LCA 遷移である。

#### 区間走査の償却量

`cand` は親葉順、走査は逆順なので `li` は単調に減る。候補の親葉を順に `p_0 >= p_1 >= ...` とすると、
LCA 計算が見る区間は `[p_i, p_{i-1})` であり、異なる親グループ間では重ならない。同じ親の兄弟では空区間になる。
したがって `beam_search.cpp:349-354` の合計反復数は高々 `L-1` である。

`copy_tour_path()` も同じ非重複区間を調べる（`beam_search.cpp:221-233`）。各区間では `prog` が単調に増えるため、
コピーする ActionId 数はその遷移で下る経路長以下であり、世代全体では `O(T)` である。
標準版の木処理は概ね次の量になる。

- LCA 境界の調査: `O(L)`
- 旧経路から `trace` への復元: `O(T_old)` 以下
- `next_tour` の構築: `O(T_new)`
- 状態遷移: `O(X)` 回の `apply_op()` / `rollback()`
- 候補整列: `O(W log W)`

従って、一般 LCA 構造だけで置き換えられるのは主に `O(L)` の境界調査である。`tour` の構築・コピーを
消すには、親ポインタや根から葉への経路列など、下り経路を復元できる別表現も必要である。
それでも、現行の State 契約では状態差分の適用回数 `X` は減らない。

#### 状態走査の下限

生存葉への経路の和集合からなる生存部分木を考える。単一の可変 `State` で全葉を訪れる場合、
始点と終点を結ぶ経路以外の辺は
下りと上りで2回通る必要がある。DFS 順で葉を連続させる走査は、各辺を高々2回通り、この下限にほぼ一致する。
LCA を `O(1)` で求めても、`State` のコピー、Action 合成、経路一括適用のいずれかを新たに許さない限り `X` は減らない。

#### 一本道確定と世代ブロック

全生存候補の LCA より前の Action は `confirm_and_free()` で `result_prefix` へ移され、世代ブロックが再利用される
（`beam_search.cpp:71-81, 448`）。探索中の状態は既にその共通接頭辞を適用済みで、以後の葉間遷移はそこより上へ
戻らない。一本道確定も既に実装済みである。

長所は Action の連続配置、世代単位の安価な解放、単純な寿命規則である。短所は、子孫を失ったスロットも世代全体が
確定するまで残るため、Action メモリが `O(G * S_A)`、粗い最悪で `O(WD * S_A)` になることと、
`tour` と `next_tour` の帯域を使うことである。

### Compose 版

`beam_search_compose.cpp` は標準版と同じ `tour` 表現に、世代ごとの ghost ビットを追加する
（`beam_search_compose.cpp:69-80`）。ある親に生存子が1つだけなら `Action::compose()` を試し、成功時に親を ghost、
合成結果を子スロットに置く（`beam_search_compose.cpp:82-116`）。探索時は ghost の `apply_op()` と `rollback()` を
省く（`beam_search_compose.cpp:476-491`）。

実現済みのものは次のとおり。

- 単一子経路での Action 合成
- 合成済み親に対する状態更新呼び出しの省略
- 非 ghost Action だけを結果へ出す処理
- 標準版と同じ世代ブロックの一括再利用

一方、ghost の ActionId は `tour` と論理深さに残る。`CandIdx::action_count` は常に世代番号であり、
`eff_depth` も実質的に論理深さである（`beam_search_compose.cpp:128-145, 186-190`）。従って次のコストは残る。

- ghost スロットの `is_ghost()` 判定
- ghost を含む `trace` と `tour` のコピー
- 世代ブロック内の ghost Action スロット
- 合成のため、現在状態に乗っている親を一時的に rollback する整合処理

物理的な圧縮リンクを追加して ghost を走査対象から外す案は可能だが、明示的にそれを行うのが Radix 版である。
Compose 版へ別の圧縮木を重ねるより、両実装の測定結果から残す方式を決める方がよい。

### Radix 版

`beam_search_radix.cpp:23-46` の `Node` は Action 辺、親、子数、子・兄弟リンク、スコアを持つ。
枯れ枝を削除し、単一子内部ノードを `Action::compose()` で縮約する
（`beam_search_radix.cpp:93-121, 259-264`）。根の子が1つなら Action を確定接頭辞へ移し、状態と root を進める
（`beam_search_radix.cpp:266-275`）。候補は密な `parent_leaf` でバケット分けし、親内だけをスコア順にする
（`beam_search_radix.cpp:212-257`）。

`compose()` が必要なすべての箇所で成功すれば、root 以外の内部ノードは次数2以上になる。葉数を `L` とすると内部ノードは
高々 `L-1` なので、live node 数 `N` は `O(L)`、具体的には概ね `2L-1` 以下になる。

ただし、この上限には次の条件がある。

- `Action::compose()` が失敗すると単一子 node が残る。失敗が続けば `N = O(WD)` まで増え得る。
- `N = O(L)` でも inline Action は `O(N * S_A)` である。ただし合成 Action が全 primitive の
  rollback 記録をヒープに持つなら、所有 payload は `S_A` では上限できず、`O(WD)` 記録になり得る。
- 長い `vector` 同士を毎回連結する実装では、合成時間やメモリコピーが二次的になる可能性がある。
- `pool` と `free_ids` は最大到達容量を保持するため、`N` が減っても peak capacity は下がらない。
- `free_node()` は意図的に `Node::edge` を破棄しない。Action がヒープを所有すると、free slot も再利用まで
  payload を保持する。合成失敗で大きくなった過去の pool 高水位は、live `N` が減っても
  Action 所有メモリの高水位として残り得る。

DFS は root から全葉を巡り、終了時に state を root へ戻す
（`beam_search_radix.cpp:176-207`）。圧縮後の辺数が小さければ問題ないが、合成失敗時は全未圧縮辺を往復する。

Radix 版で新規性のある改善候補は圧縮木そのものではなく、次である。

- `Action` と木メタデータを別 arena に分ける SoA 化。木の surgery と部分木最小値計算で大きな Action を
  キャッシュへ載せない。
- compose 成功率、合成長、合成時間、合成 Action 内の primitive 適用数を計測する。
- 子リンクの pointer chasing と、平坦な DFS 操作列を毎世代作る帯域のどちらが軽いかを State コスト別に比較する。

### 可変ターン版

`beam_search_turn.cpp:37-54` は内部辺を PRE/POST の2トークン、葉を1トークンとして `tree` に平坦化する。
PRE は `subtree_end` と部分木の最小 `target_turn` を持つ。`get_next_beam()` は未来だけを含む部分木を飛ばせるが、
現在ターンを含む経路では PRE/POST を処理する（`beam_search_turn.cpp:400-439`）。

`update_tree()` は `tree` を先頭から読み、死んだ葉の除去、展開葉から子への置換、空の PRE/POST の対消滅、
部分木最小値の再計算を行いながら `nxt_tree` を再構築する（`beam_search_turn.cpp:441-605`）。
一本道接頭辞の確定は `beam_search_turn.cpp:467-479` に既にある。

この方式は次の場合に強い。

- そのターンに木の大部分を展開する
- `TreeNode` の連続走査とコピーがキャッシュに収まる
- Action の親ポインタを辿るランダムアクセスよりメモリ帯域が安い

一方、長いターンジャンプが多く `K << L` の場合でも、木更新は基本的に `O(Q)` の走査・再構築になる。
標準版より、こちらの方が対象葉だけを使う LCA 方式の利益が大きい。
各 active pool の幅を `W_t` とすると `L <= sum_t W_t`。各 `W_t <= W` なら `L <= FW` である。
さらに `E <= LD` と `Q = O(E + L)` から、共通接頭辞を無視した粗い上限は `Q = O(FWD)` である。

## 葉＋LCA案

### 葉と LCA だけでは経路を復元できない

連続葉間の LCA 深さだけを保持しても、LCA から次の葉へ適用する Action 列は得られない。次のいずれかが別途必要である。

- 各 Action ノードの親ポインタ
- 根から葉までの経路列
- 圧縮辺が保持する Action 列または合成 Action
- State のスナップショット

標準版の `tour` は、LCA 境界と下り経路を連続配列で供給する方式である。従って、比較対象は
「tour 対 LCA」ではなく「連続した下り経路列 対 親ポインタ鎖」である。

### 親ポインタを使う葉間遷移

固定深さでは2葉の深さが等しい。現在状態の葉 `u` と次に展開する葉 `v` について、`u != v` の間、
両方の親へ1段ずつ上がる。`u` 側はその場で rollback し、`v` 側の ActionId は一時配列へ積む。
一致したノードが LCA であり、その後 `v` 側を逆順に apply する。

この方法では LCA 探索の1反復が必ず片側の rollback と反対側の将来の apply に対応する。LCA だけを先に
binary lifting で求める必要はない。

| 項目 | 現行 tour | 親ポインタ葉走査 |
|---|---|---|
| LCA 境界 | `leaf` の単調区間走査、世代合計 `O(L)` | 状態遷移と同時に親鎖を上る `O(X)` |
| 下り経路 | `copy_tour_path()` で `trace` へコピー | 右側の親鎖を一時配列へ積んで逆順適用 |
| 次世代構築 | `next_tour` へ `O(T)` 個の ID を書く | 各新規ノードへ親 ID を1個書く |
| アクセス | 連続配列中心 | 世代をまたぐ依存ロード |
| 追加メモリ | `tour` と `next_tour` | Action ノードごとの親 ID |
| State 契約 | 現行どおり | 現行どおり |
| 結果 | 現行順を保てば同一 | 現行順を保てば同一 |

時間計算量はどちらも `O(X + W log W)` を主項とし、親ポインタ方式の利益は `tour` の `O(T)` コピーを
親ロードへ置き換える定数倍である。`apply_op()` が重いと差は埋もれ、Action が軽く `T/W` が大きいときにだけ
利益が出る可能性が高い。親ロードは依存関係があり、`std::copy` より遅い場合もあるため、無条件の改善ではない。

### 一般 LCA 構造を採用しない理由

| 方法 | LCA | 追加メモリ | 問題点 |
|---|---:|---:|---|
| 親を1段ずつ上る | `O(距離)` | `O(E)` の親 | その距離だけ State 操作も必要なので仕事を融合できる |
| binary lifting | `O(log D)` | `O(E log D)` | LCA 後も各辺を apply/rollback する。前処理と帯域が純増する |
| Euler tour + RMQ | `O(1)` | `O(E)` 以上 | 動的に成長・削除する木では再構築または複雑な動的 RMQ が必要 |
| 世代ごとの offline LCA | `O(E+W)` | `O(E)` | 木を走査する費用が、消したい走査費用と同程度 |

一般 LCA は、State が経路一括適用やスナップショット復元を提供し、LCA までの各辺を触らずに移動できる場合にだけ
再検討する。

## Parent-slot 案

### 固定深さを利用した表現

標準版と Compose 版では、深さ `d` の Action は必ず `gblock[d]` にある。従って ActionId の世代番号は
経路上の位置から分かり、親も必ず1つ前の世代にある。候補ノードを概念上、次の SoA で持てる。

```text
action_block[d][slot]  : Action
parent_slot[d][slot]   : 深さ d-1 の親スロット (uint32_t)
leaf_slots[i]          : DFS 順の深さ current_depth の葉スロット
```

カーソルは `(depth, slot)` であり、親へ上がるときだけ `depth--` と
`slot = parent_slot[depth_before][slot]` を行う。永続データに64 bitの `(gen, slot)` を保存する必要はない。

候補確定時は `candidate.parent_leaf` から親葉スロットを引き、子の `parent_slot` に保存する。候補を現行と同じ
`(parent_leaf, score)` 順で処理すれば DFS 順と候補評価順を維持できる。

### 2段階で試す

#### 段階A: slot-only ActionId

`tour` 自体は残し、`trace`、`tour`、`next_tour` に64 bit ActionIdではなく32 bit slotだけを保存する。
適用時の世代は `trace` の深さから得る。これは親ポインタ方式より小さな変更で、ID帯域をほぼ半減できる。

公開 State/Action 契約は増えず、値が範囲内なら探索結果も変わらない。一方、内部契約として
32 bit slot 範囲と、「`trace[d]` の slot は必ず `gblock[d]` に属する」という深さから世代 block への対応が
必要である。Compose 版でも合成結果は子の論理世代 slot に置かれるため、この表現を使える。

#### 段階B: parent-slot で tour を除く

`parent_slot` を追加し、葉間遷移を親鎖で行う。`tour`、`next_tour`、`leaf`、`copy_tour_path()` を除く。
概算メモリ差は次のようになる。

```text
現行の主要 ID 容量       : 約 8 * (capacity(tour) + capacity(next_tour)) byte
parent-slot の追加容量   : 約 4 * G byte
葉カーソルと一時経路     : O(L + W + D)
```

世代ブロックに死んだ Action が多く `G >> E` なら parent-slot は不利である。反対に `T` が大きく、ほぼすべての
世代スロットが現在の生存木に寄与しているなら有利になり得る。`G/T` と ID copy bytes を必ず測る。

### 必要な State / Action 契約

公開契約は標準版と同じでよい。

- `apply_op(a)` と `rollback(a)` が完全に対称である。
- Action は候補確定後に探索中の状態へ依存して書き換えられない。
- 親 Action のスロットは、子孫が生きている間は参照可能である。
- Action の move 後も、実際に参照するスロット側に完全な rollback 情報が残る。

### 探索結果への影響

葉の DFS 順、同じ親内のスコア順、候補列挙順、同点時の比較を現行と同じにすれば、State が見る経路と候補集合は
同じにできる。ただし現行の `std::sort` 自体は完全同点の順序を規定しない。実装間の完全再現を要求するなら
列挙 ordinal を比較キーへ加え、順序を仕様化する必要がある。

### 実装難度と危険な不変条件

難度は中から高である。特に次を debug build で検査する。

- `parent_slot[d][s]` は `gblock[d-1]` の有効スロットを指す。
- 状態が現在乗っている `(depth, slot)` と実 State が常に一致する。
- rollback は現在葉から LCA へ向かう順、apply は LCA から次葉へ向かう順である。
- 世代解放後、未確定ノードから解放済みスロットへ上らない。確定境界を sentinel root として付け替えるか、
  境界ノードを1世代残す。
- 全葉の LCA より浅い Action だけを `result_prefix` へ移す。
- 完成解と最終解の経路復元が、確定接頭辞と未確定親鎖を重複または欠落なく連結する。

## 仮想木と圧縮木

### 仮想木

現在葉とそれらの LCA だけから仮想木を作れば、分岐骨格は高々 `2L-1` node になる。DFS 順と LCA が
既にあれば構築は `O(L)`、なければ整列や LCA 前処理が必要になる。

ただし仮想辺は元の複数 Action を表す。State 契約が現行のままなら、仮想辺を通るたびに内部の Action を順番に
apply/rollback する必要があり、`X` は減らない。元 Action の保存量も減らない。仮想木が消すのは主に構造メタデータである。

葉列、隣接葉 LCA、親 Action 鎖を持つ方式は、仮想木を明示的に構築しない暗黙表現とみなせる。
また、合成 Action を辺に置く明示仮想木は Radix 版とほぼ同じである。独立した第3の固定深さエンジンとしての
優先度は低い。

### Action 合成による圧縮

実際の State 呼び出し回数を減らすには、圧縮辺を1回で適用・取消できる必要がある。`Action::compose(child)` には
少なくとも次の契約が必要である。

- 成功後の合成 Action の apply は「親 Action、子 Action」の順に適用した状態と一致する。
- rollback は逆順に完全復元する。
- 失敗時は親と子のどちらも変更しない。
- 合成後の Action を `last_action` として渡しても、`enumerate_actions()` が必要とする最後の primitive 操作を
  取得できる。
- score、hash、`pre_*`、`nxt_*` の意味が合成前後で一致する。
- `compose()` の時間と追加メモリが、想定する合成長に対して許容できる。

この契約が満たされれば探索状態は同じにできるが、返却される Action の区切りと `actions.size()` は変わる。
また DFS 順、同点順、乱数消費順が異なれば、最終候補自体も変わり得る。

### Compose と Radix の使い分け

| 観点 | Compose 版 | Radix 版 |
|---|---|---|
| 木表現 | 標準版の連続 `tour` | 明示親・子・兄弟木 |
| 単一子 | 親を ghost にする | 親ノードを構造から除く |
| 状態呼び出し | ghost を分岐でskip | 圧縮辺を1回だけ処理 |
| 構造走査 | 論理深さ分が残る | `O(N)`、全 compose 成功時は `N = O(L)` |
| Action メモリ | `O(G*S_A)` | inline edge は `O(N*S_A)`、所有 payload と free slot は別途 |
| locality | ActionId 列が連続 | pool と兄弟リンクの pointer chasing |
| 一本道確定 | LCA による世代確定 | root の単一子を即時確定 |
| 追加整合費 | 現在適用中の親の一時 rollback | surgery 前に state を root へ戻す |

長い単一子区間が多く、compose が安価かつ常に成功するなら Radix を優先候補とする。分岐が多い、Action が大きい、
compose が失敗する、連続アクセスが重要という場合は標準版または Compose 版が安全である。

## 一本道確定

一本道接頭辞の確定は4系統ですでに次の形で実現されている。

- 標準版: 全候補の LCA を基準に `confirm_and_free()`
- Compose 版: ghost を除外しながら同様に確定
- Radix 版: root の子が1つの間、root を子へ進める
- 可変ターン版: 先頭 PRE と末尾 POST が同じ Action の間、接頭辞を `result` へ移す

全生存葉が共有しない辺まで確定すると探索結果が変わる。より積極的な確定は、問題固有の優越性や単調性を
新しい契約として要求しない限りできない。新規アルゴリズムではなく、不変条件の検査と Action の move/copy 削減が対象になる。

## 世代ブロック

### 適する場合

- 全 Action が論理深さを1ずつ進める固定深さ版
- ActionId の寿命を世代単位で管理できる
- 個別 free より連続配置と一括再利用を優先する
- `W * 未確定深さ * sizeof(Action)` がメモリ予算内に収まる

### 適さない場合

- `target_turn` が飛び、同じ世代の Action の寿命が大きく異なる
- 古い世代のごく一部だけが長く生きる
- Action が大きく、死んだスロットの保持が支配的になる

可変ターン版は free slot arena、Radix 版は node pool の方が寿命に合う。可変ターン版の arena を固定長チャンクにして
参照を安定化する案は有力だが、世代ブロックへの置換ではない。

## DFS 順最適化

### 構造上の原則

同じ部分木の葉を連続して処理する DFS 順は、単一 State の移動距離を最小級に保つ。全葉を score の大域順だけで
並べると、浅い LCA を何度も跨ぎ、最悪 `O(WD)` の状態遷移になり得る。

現行標準版は `parent_leaf` を第一キーにするため、親部分木の連続性を保っている。Radix 版も親バケットと子リストで
DFS 順を作る。可変ターン版は平坦木自体が DFS 順である。

### 比較ソートを減らす

`parent_leaf` は `0..L-1` の密な整数なので、標準版と Compose 版の全体 `std::sort` は次で置き換えられる。

1. 親ごとの生存候補数を数える。
2. 累積和で各親区間を決める。
3. 候補を親区間へ安定に振り分ける。
4. 親内だけ score 順にする。2件なら比較交換、少数なら insertion sort、多数だけ `std::sort` を使う。

これは `review/ahc_beam_search_constant_factor_optimization.md` に既出で、Radix 版では実装済みである。
探索順を完全に維持するなら、列挙 ordinal も振り分け時に維持する。

### 良い候補を先に展開する案

同じ親を持つ兄弟候補の中で良い候補を先に処理すれば `Candidates::threshold()` が早く下がり、
`try_op()` や `enumerate_actions()` の早期打ち切りが増える可能性がある。同一親の兄弟順変更なら、
親へ戻って子へ下る距離は不変である。より大きな部分木内の並べ替えで同じ上限を保証するには、
単にその部分木を連続にするだけでなく、全階層で各子部分木が連続する DFS 順を保つ必要がある。

ただし候補採否、同点順、乱数消費順は変わり得るため、既定動作を silently 変更せず探索順ポリシーにする。
Radix 版の子順はこの考え方を一部実装しているが、内部部分木の最新最小スコアで毎世代並べ直してはいない。

## 可変ターン版の疎密ハイブリッド

### 現行 dense 方式の計算量

平坦木の連続走査を dense 方式と呼ぶ。展開と更新の主要コストは概ね次である。

- 展開: `O(R + X)`。`R` はスキップできない PRE/POST/leaf token 数
- 更新: `O(Q + A)`。この前に `S_sort` があり、死んだ一時候補も残る現状では `A` の影響を受ける
- メモリ: `tree` と `nxt_tree` の最大 capacity、Action arena

`K/L` と `R/Q` が大きい場合は連続メモリの利点が強く、明示木に変える理由は小さい。

### sparse 方式の概念設計

可変ターン版は既に target turn 別の候補プールを持つ（`beam_search_turn.cpp:247-303`）。これを葉カレンダーとして使い、
現在ターンの最終生存葉だけを直接取り出す。

必要な構造は次のとおり。

- 安定 ActionId を持つ node arena
- 各 node の `parent`, 構造深さ、live child 数または参照数
- target turn 別の生存葉 ID
- 候補置換時に古い葉を即時削除する evicted-ID queue
- 全 frontier 葉の DFS 順を表す intrusive list または order-maintenance label
- 根から確定接頭辞を進めるための live child 情報

ターン `t` では次の処理を行う。

1. カレンダー `t` の最終生存葉を収集する。
2. 現在の DFS 順に整列する。
3. 親ポインタ LCA で葉間を移動し、対象葉だけ展開する。
4. 展開後、各親葉を生存子の連続区間で置換する。
5. 候補集合から追い出された未来葉を node ID で直接 unlink し、子を失った祖先を必要な範囲だけ更新する。
6. root の live child が1つなら接頭辞を確定する。

構造深さは論理 `target_turn` とは別に必要である。同じ target turn の葉でも Action 数が異なるため、LCA ではまず
構造深さを揃える。

### 計算量

対象葉の DFS 順が得られると仮定した概算は次である。

```text
dense  : O(R + Q + A + S_sort + X)
sparse : O(K log K + A + S_sort + X + changed_nodes + affected_ancestors)
```

order-maintenance か turn ごとの sort を使うため `K log K` とした。祖先更新を葉ごとに独立に行うと最悪 `O(KD)` に
なるので、同じ祖先を世代内でまとめる必要がある。sparse 方式にも無条件の漸近改善はない。

### 疎密の切替

単一実装を自動で切り替える場合は、少なくとも次を使って判断する。

- `K / L`
- `R / Q`
- 直近の平均 LCA 距離と `X / K`
- 木更新でコピーした bytes
- explicit node の変更数

切替時に平坦木を読む `O(Q)` と node を構築する `O(N)` の変換費が必要ならヒステリシスを設ける。
まずは dense と sparse を別ポリシーとして測り、
損益分岐が安定してから自動切替を検討する方が安全である。

### 探索結果への影響

現行の平坦木で葉を訪れる順を order label で再現し、同じ親内の候補順も維持すれば、State が見る列挙順を同じにできる。
任意順でカレンダー葉を処理すると、候補の閾値更新順、同点採否、乱数消費順が変わり、探索結果も変わり得る。

### 実装難度と危険な不変条件

難度は非常に高い。特に次が壊れやすい。

- 生存葉はちょうど1つの target-turn pool と frontier に属する。
- 候補置換で evicted ActionId を二重解放せず、将来の pool に残さない。
- 親は生きた子孫がある間、arena から解放しない。
- ActionId 再利用前に tree、candidate pool、history の参照が全て消えている。
- DFS order label は葉の削除と、1葉から複数子への置換後も全順序と一致する。
- LCA 遷移終了時の State と現在葉 ID が一致する。
- 根の child count、部分木最小 target turn、ターン別 threshold が候補追い出し後も一致する。

この方式は、測定で `K << L` と `Q` の再構築が支配的だと確認できた場合にだけ着手する。

## 案ごとの比較

`Δ` は疎な可変ターン木で変更した node と、更新が必要な祖先の合計数とする。

| 案 | 消すコスト | 時間 | 追加・削減メモリ | 新しい契約 | 結果への影響 | 難度 | 判定 |
|---|---|---|---|---|---|---|---|
| 32 bit slot-only tour | 64 bit ID帯域とdecode | 漸近同じ、定数減 | tour系を約半減 | 32 bit範囲と深さ対応 | なし | 中 | P1、先に試す |
| parent-slot 葉走査 | tour生成・経路コピー | `O(X + W log W)` | `+4G`, tour 2本を削除 | 内部の親寿命 | 順序維持でなし | 中〜高 | P2、比較試作 |
| binary lifting LCA | `leaf` の線形境界走査 | `O(W log D + X)` | `O(E log D)` | なし | なし | 中 | 不採用 |
| 明示仮想木 | 分岐骨格外のmetadata | build `O(L)`、`X`不変 | 骨格 `O(L)`、Actionは残る | span寿命 | 順序維持でなし | 高 | Radix/parent案と重複 |
| Compose ghost | State呼び出し | 論理tour走査は残る | ghost bitと全世代Action | 強い compose | Action区切り変更 | 実装済み | 計測して維持判断 |
| Radix | 単一子nodeとState呼出 | `O(N+X)` | node `O(N)`、全合成成功時 `N=O(L)` | 強い compose | 順序等で変化 | 実装済み | 適合時の本命 |
| Radix SoA | surgery時のAction cache汚染 | 漸近同じ、定数減 | metadata/action分離 | なし | なし | 中 | P1 |
| 一本道確定 | 共通prefixの保持・再走査 | 全探索合計 `O(D)` | 古いblock/nodeを再利用 | なし | なし | 実装済み | 重複実装しない |
| dense親バケット | 全候補比較sort | `O(W+L)+親内sort` | count/offset `O(L)` | 順序規則 | 同点順次第 | 低〜中 | 既出、Radix実装済み |
| turn sparse calendar | 非対象木走査 | `O(K log K+A+S_sort+X+Δ)` | node `O(N)` | 安定IDと順序 | 順序維持でなし | 非常に高 | 疎な場合だけP2 |
| turn dense/sparse自動切替 | workload偏り | 変換費込み | 両表現または変換領域 | 両方式の契約 | 切替順を守ればなし | 非常に高 | 最後に検討 |

Radix の node 数が `N = O(L)` になるのは、必要な単一子辺をすべて合成できる場合に限る。
合成に失敗し続けると `N = O(WD)`、時間は `O(N + X) = O(WD + X)` まで増え得る。sparse calendar は
`K/L` が低く `Q/K` が大きい場合だけ比較する。

## 優先順位

### P0: 測定と正当性基準

構造を変える前に、現行標準版が既に葉間 LCA 走査であることを基準にする。一般 LCA との比較ではなく、
ID copy bytes と親ポインタ依存ロードの比較を行う。世代ごとの生存候補 `(score, hash, parent order)` と
State checksum を記録し、順序を維持する案は探索結果が一致することを確認する。

### P1: 小さく、既存設計と整合する改善

1. 固定深さ版のActionIdから冗長な世代番号を外した32 bit slot-only tourは実装・初回測定済みである。
2. Radix の Node を Action payload と hot metadata に分離する SoA を比較する。
3. 標準版・Compose版の親グループ化を dense bucket にする既出案を、同点順を明文化した上で試す。
4. Compose/Radix の compose 成功率、ghost率、合成長、primitive仕事量を常時計測ではなく計測ビルドで取る。

### P2: 条件付きの構造試作

1. parent-slot葉走査はoracleを実装済みで、初回測定では標準optimized版を一律には上回らなかった。
2. 可変ターン版で `K/Q` が継続的に小さく、`update_tree()` の bytes が支配的な場合に sparse calendar 版を試す。
3. Compose の ghost metadata が支配的で Radix の pointer chasing が不利な場合だけ、世代 arena 上の
   compressed-parent link を検討する。

### P3: 現時点では採用しない

- 固定深さ版への binary lifting、動的 RMQ、一般 LCA
- Action 一括適用契約を伴わない明示仮想木
- 測定前の dense/sparse 自動切替
- Compose/Radix と同じ目的の第3の圧縮木実装

## 測るべき汎用指標

### 木と走査

- `L`, `P`, `F`, `N`, `E`, `Q`, `R`, `T`, `G`, `D` の平均・最大
- `T/L`, `E/L`, `N/L`, `G/E`, `R/Q`, `Q/K`
- LCA まで上る段数の合計、平均、最大、分布
- `apply_op()` / `rollback()` 回数
- 合成 Action 1回を primitive 操作へ展開した実仕事量
- `copy_tour_path()` と `next_tour.insert()` がコピーした ActionId bytes
- 平坦木で読んだ token 数、`nxt_tree` へ書いた bytes、スキップした subtree bytes
- 確定接頭辞が1世代で進んだ長さ

### 圧縮

- compose attempts / success / failure
- ghost slots / logical slots
- live radix nodes / leaves
- 合成 Action の primitive 長の平均・最大・分布
- `compose()` 自体の時間、allocation、copy/move bytes
- `cnt_compose_align` に相当する追加状態調整

### メモリとCPU

- Action payload、木 metadata、tour、候補、一時領域それぞれの live size と capacity
- peak RSS、allocation回数
- cycle / 展開葉、cycle / `try_op()`、cycle / 最終生存候補
- instructions、branch miss、L1/LLC miss
- Action サイズが 8 / 32 / 128 byte の場合の差
- noop、軽い、中程度、重い State でのライブラリ固定費

### 探索結果の比較

- 世代ごとの生存 `(score, hash, parent ordinal, enumeration ordinal)` のdigest
- 完成解と最終経路のdigest
- 各葉展開前後の State checksum
- `apply_op(a); rollback(a);` 後の root checksum
- 同じseedでの乱数消費回数

## ベンチマーク形状

1つの問題だけで決めず、少なくとも次を合成 State で直交させる。

- 長い共通接頭辞の後に広がる木
- 浅い LCA を頻繁に跨ぐ木
- ほぼ全区間が単一子の木
- 毎世代広く分岐し、圧縮できない木
- 後から死ぬ枝が多く `G/E` が大きい木
- 可変ターンで `K/L` が低い、中程度、高い木
- compose が常に成功、時々失敗、常に失敗する Action
- O(1) aggregate compose と、primitive列を保持する compose

## 最終判断

標準版の走査は既に単一 State で可能な DFS/LCA 下限に近い。汎用版で最初に狙うべきなのは一般 LCA の追加ではなく、
ID幅、Action配置、候補グループ化、計測分岐などの帯域と固定費である。parent-slot は `tour` のコピーが実測で
支配的なときの比較候補であり、既定実装を直ちに置き換える根拠はない。

構造を大きく変える価値があるのは、安価で正確な Action 合成を提供できる場合の Radix と、未来葉が多く
`K << L` になる可変ターン版である。前者は既に実装済みなので契約・SoA・測定を詰め、後者だけを新しい
疎走査ポリシーとして研究するのが、重複が少なく汎用ライブラリとして妥当である。
