# Ghost-free compose 実装計画

> このファイルは context リセットを跨いで作業を再開するための durable な記録。
> 各ステップ完了時に「進捗」欄を更新すること。

## ゴール

`beam_search_compose.cpp` から ghost ノードを **構造的に完全削除** する。

- `tour_total`: 15.76M → 約 11.87M（ghost 分 ~389万を構造除去）
- `ghost_skip`: 389万 → 0
- `is_ghost` 分岐の消滅
- **score == 10738 を全ステップで維持**（正当性オラクル）

検証: `titan_cpplib/ahc/beam_search/gamex.cpp`。
- ビルド: `make -C titan_cpplib/ahc/beam_search gamex` → `a.out`
- 実行: `./a.out < titan_cpplib/ahc/beam_search/in/gamex.txt`（パスは実装時に確認）
- 正: stderr に `Score = 10738`、`[counters]`
- no-compose ベースライン: makefile / gamex.cpp に `-DNO_COMPOSE`

## 現状の理解（検証済み）

### データ構造
- `trace[d]`: 深さ d の ActionId。永続 DFS パス。turn を跨いで state と整合。
- `tour` / `leaf`: 直前世代 (gen T-1) の ascending-edge Euler tour。
  `leaf` は m = |gen T-1| 個。reverse-segment j = `tour[leaf[j-1]..leaf[j])`（leaf[-1]=0）。
- `cand`: 今世代 (gen T)。`(parent_leaf, score)` でソートし **reverse 走査**。
  `parent_leaf` = 親の reverse-iteration index。
- `lca_dist` = `max_{k∈[parent_leaf, li)} (leaf[k+1]-leaf[k])` = 親〜前位置間の最大 segment 長。
  rollback 数 = `lca_dist + f`（f: 最初の cand のみ 0、以降 1）。
- ghost: `compose_pass` が単一子の親 slot を no-op 化。`gblock_ghost`/`ghost_slab_pool`/
  `is_ghost`/`set_ghost`。apply/rollback は ghost を skip（`cnt_*_ghost`）。

### ghost が消えない理由
tour 再構成（`copy_tour_path` / `lca_dist`）が「全 leaf が同一深さ = turn」を
ハードコードしている。ghost を tour から外すと composed leaf の経路が 1 短くなり、
この一様深さ仮定が壊れる。**可変深さ対応が ghost 構造削除の必要条件。**

`next_tour` は毎 turn `trace` から再構築される → ghost-free な `trace` を作れば
ghost-free な `next_tour` が自動で得られる。**専用の tour 圧縮パスは不要。**

## redesign の核

1. **eff_depth[]**: `leaf[]` と並走する `eff_depth[k]` = leaf k の実深さ(action_count)。
   非 composed = turn、composed = turn-1。`leaf[]` を書く同じループで書く（追加走査ゼロ）。
   `next_eff_depth[]` も同様。
2. **CandIdx.action_count**: 子の深さ。非 composed = parent.action_count+1、
   composed 子 = parent.action_count（親 1 段を畳むので不変）。
3. **trace を深さ(action_count) index 化**。
4. **lca_dist → 明示 LCA 深さ dL**:
   `dL = min_{k∈[pC,pP)} (eff_depth[k] - segLen(k))`、siblings(pC==pP) は `eff_depth[pC]`。
   rollback_count = `dP - dL`、apply_count = `dC - dL`。
5. **copy_tour_path 深さ明示化**: entry を `eff_depth[k]` 基準で配置、深さキーの ratchet、
   `trace[dL+1..dC-1]` のみ書く（分岐部のみ → 定数倍維持）。
6. **compose（ghost 廃止）**: `gblock_ghost`/`ghost_slab_pool`/`is_ghost`/`set_ghost` 削除。
   compose 対象 = ちょうど 1 子の親（0 子の dead leaf は不要、overlay が処理）。
   composed 子 C は `copy_tour_path` で親 segment 末尾 1 entry をスキップ（O(1)/compose）。

## 実装ステップ（各ステップ後に score==10738 を確認）

- [x] **Step 0**: 現状ビルド・実行で score==10738 と現カウンタを再確認（既知良ベースライン）。
- [x] **Step 1**: `eff_depth[]`/`next_eff_depth[]` 追加、uniform（全 turn）で埋める。
      `CandIdx` に `action_count` 追加。既存挙動完全不変を確認。
- [x] **Step 2**: `lca_dist` を `eff_depth` ベースの明示 `dL` に置換（uniform 下で同値）。
      rollback/apply ループを `dL` ベースに可変深さ化。score==10738。
- [x] **Step 3**: `copy_tour_path` を深さ明示版に書換（uniform 下で同値）。score==10738。
- [ ] **Step 4**: ghost 機構を削除、compose を可変深さ化（composed 子 depth = parent depth、
      `copy_tour_path` で親 segment 末尾スキップ）。score==10738 / tour_total 減 /
      ghost_skip 廃止を確認。
- [ ] **Step 5**: 計測カウンタを ghost-free 版向けに整理、`a.md` に結果追記。

Step 1〜3 は uniform 深さ下での純粋リファクタなのでオラクルで完全検証可能。
Step 4 が唯一の可変深さ導入箇所 → リスク隔離。

## 検証済みの tour 機構（Step 4 の前提）

ターン T の DFS は `cand`(gen T) を (parent_leaf,score) ソートして **reverse 走査**。
反復 i での `now_leaf_idx = next_leaf.size() = size-1-i =: j` を、その反復で emit した
子の `parent_leaf` に振る。

- segment 定義: `S(k) = tour[leaf[k]..leaf[k+1])`。`copy_tour_path` はこの k 基準で読む。
- **S(k) = cand[size-1-k] の上昇 tail**（その cand の経路の、前 cand との LCA より下）。
- `parent_leaf=k` の子の親 = cand[size-1-k]。よって `copy_tour_path(parent_leaf=k,...)` が
  最初に読む `S(k)` がちょうど親自身の tail（最深）。以降 S(k+1..li-1) は浅い祖先。
- `tour[0..leaf[0])`（=S(-1)=cand[size-1] の tail）は copy_tour_path からは読まれない。
- 1 tour entry = 1 深さ階層（action_count 空間）。composed は 2 primitive step を
  1 entry で表すが trace スロットは 1 つ＝深さ 1 階層。

実装済みの深さ式（uniform で旧 lca_dist と一致を確認済み）:
- `dL = min_{k∈[parent_leaf,li)} (eff_depth[k] - (leaf[k+1]-leaf[k]))`、
  区間空(parent_leaf==li)は `dL = eff_depth[parent_leaf]`。
- `dP_state`(state の現深さ) = turn 開始時 `eff_depth.back()`、各反復後 `dC`。
- rollback: `for d=dP_state..dL+1`、apply: `for d=dL+1..dC`、
  next_tour へ `trace[dL+1..dC]` を insert、`trace[dC]=c.action_id`。
- `gL = min dL`（全 cand）。`confirm_and_free(gL+1)`。
- `copy_tour_path`: 深さキー ratchet。segment k を底 `eff_depth[k]`・長さ
  `leaf[k+1]-leaf[k]` で `trace[top..hi]` に配置（`top=底-長+1`）。

## Step 4 の要点（未着手）

- `gblock_ghost`/`ghost_slab_pool`/`is_ghost`/`set_ghost`/`materialize` を削除。
  `confirm_and_free`・`build_best_path`・最終 ret 構築の `is_ghost` 分岐を除去。
  `cnt_*_ghost` カウンタ削除、DFS の rollback/apply は単純化。
- `compose_pass`: 単一子の親 P をその子 C に compose。ghost 化の代わりに
  **C.action_count を P の深さに引き下げる**（dC が 1 浅くなる）。
  → 次ターン C を処理するとき `dC=C.action_count` が自動で 1 浅くなり、
  `next_eff_depth.push_back(c.action_count)` で eff_depth も縮む。
- composed C の `copy_tour_path`: 親 segment `S(parent_leaf)` の **底 1 entry**
  （=親 P 自身の action）をスキップする必要。P は C に畳まれ trace[dC] は C の
  composed action。S(parent_leaf) をそのまま貼ると trace[dC] に P が乗り上書き衝突。
  → composed cand のときだけ `copy_tour_path` で k=parent_leaf segment の
  `seg_len` を 1 減らす（底を除く）扱い。O(1)。
- `compose_pass` の state alignment（直前 apply 済み action の二重適用回避）は
  ghost 廃止後も必要 → 現状の rollback/re-apply ロジックは概ね流用。
- `next_tour` は trace から毎ターン再構築されるので、ghost-free trace から
  自動で ghost-free next_tour。専用圧縮パス不要。

## 進捗

- 2026-05-22: 計画ファイル作成。
- 2026-05-22: **Step 0〜3 完了**。各ステップ後 `make gamex` → `./a.out < in/gamex.txt`
  で score==10738・カウンタ完全一致を確認（uniform 下の純リファクタとして無欠）。
  現状値: tour_total=15,757,531 / apply real=11,870,690 ghost_skip=3,886,841 /
  rollback real=11,867,896 ghost_skip=3,887,136 / actions count=2114。
  次は Step 4（可変深さ・ghost 構造削除）。
