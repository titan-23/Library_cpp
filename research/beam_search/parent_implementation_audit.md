# direct parent backend 実装監査

## 結論

`beam_search_parent.cpp` を通常版、`parent_backend_spec.md`、`red_team.md`、`cost_model.md` と照合した

- direct parent 構造に関する重大な不整合は見つからなかった
- State の apply と rollback の列は通常版と一致した
- prefix 解放後に解放済み parent を読む経路は見つからなかった
- 同一 Beam instance の再利用時に前回 hash を引き継ぐ問題を1件検出した
- 上記は root の初回 reset だけ hash を強制破棄する修正と回帰testで解消した
- 同じ問題が現行標準版にもあったため、両backendを同じ条件で修正した

旧baselineにも再利用時の同じ挙動があるが、比較基準のロジック非変更条件に従って変更していない

## 不変条件の照合

### Action slot と frontier ordinal

`finalize_generation()` はselector slot順にActionを `action_block[depth]` へ格納する

実展開順は独立した `frontier_slot` に保持し、次を満たす

```text
frontier_slot[j] == cand[C - 1 - j].action_slot
```

子のparentは `parent_leaf` をslotとして使わず、確定済みfrontierを介して変換する

```text
parent_block[depth][child_slot] = frontier_slot[parent_leaf]
```

root候補2個以上、selector replacement、親gapを含む差分testで一致した

### root の順序

深さ1の `cand` はsortせず、selectorの格納順を逆に展開する

通常版のgeneration 1と一致し、`max_turn==1` のbest選択も格納順のstrict `<` scanを維持する

### parent の確定時点

parent mapは全葉の列挙とselector replacementが終わった後、`finalize_generation()` で作る

仮採用時のparent情報を保持しないため、同じselector slotが別親の候補へ置換されても古い親は残らない

### entry LCP と adjacent LCP

新frontierの親ordinalはsort後の逆走査で単調非増加になる

`build_next_frontier()` は旧endpointのordinal `C-1` から下降cursorを開始する

- 最初の子は `[parent, C-1)` の隣接LCP minimumを `next_entry_lcp` にする
- 同じ親の次の子は親depthをLCPにする
- 異なる親の子は2親間の隣接LCP minimumを使う
- `descend_to()` ごとにminimumをdepthへ戻し、cursorだけを引き継ぐ

この処理は親group間の互いに素な区間だけを読み、一般RMQを必要としない

### decode の停止境界

target pathは深い側から `trace` へ書き、深さ `h+1` のslotを書いた時点で止める

```text
if (depth > h + 1) slot = parent(depth, slot)
```

従って `parent[h+1]` は読まない

`freed_to==h` のとき、解放済み深さhを指すdangling parentが残っていても参照しない

### prefix 解放

実装はcurrent frontier全体の共通prefixを使う

```text
C == 1 なら keep_from = depth + 1
C >= 2 なら keep_from = min(adjacent_lcp) + 1
```

走査終了endpointはcurrent frontierの最後の葉そのものなので、この集合へ必ず含まれる

W=1では現在葉まで確定するが、次generationの `entry_lcp==depth` となるため解放済み親を読まない

複数葉では全葉に共通する深さだけを確定するため、endpointが次世代で脱落してもrollback経路を保持する

## 終了処理

### finished

finished Actionはfrontierへ格納せず、発見時の `result_prefix + trace suffix + Action` を保存する

発見後も同generationの全葉を走査し、より小さいfinished scoreがあれば置き換える

返却時は最後の走査endpointを元の `trace` でrootへ戻し、保存済みbest pathを適用する

root、最初と中間の葉、同generationの複数改善、materialize有無を差分testで確認した

### no candidates

`found_finished` を先に判定するため、finishedとnonfinished 0件が同時に起きた場合はFinishedを返す

rootではturn 0、通常generationでは現在turnを返し、final Stateを作らない

通常版とstatus、score、turns、空path、Stateの有無が一致した

### max turn

最終候補の祖先slotは `trace` と別の `result_slot` へ復元する

このため、返却Stateを作る際の旧endpoint rollback用 `trace` は破壊されない

復元は `freed_to+1` で停止し、その深さのparentを読まない

`max_turn==1`、bestがendpointでない場合、`freed_to>0`、materialize有無を確認した

最終depthのparent mapはpath復元に必要なため保持されている

一方、最終depthのfrontierとLCPは次の走査がなく不要なため、監査後の修正で構築を省いた

## history と telemetry

history node IDは `CandIdx.node_id` に分離され、parent slotへ流用されていない

callbackとvector fallbackについて、raw history JSONまで通常版と一致した

`compat_tour_size` は各遷移の `depth-h` を合計し、通常版の論理tour長を再現する

`pool_size_sum`、`beam_width_sum`、`turn_sum`、`width_hist` は差分testで一致した

動的幅ではbackend固有の実時間が次の幅へ影響し得るため、異なるbackend間の完全な探索結果一致は一般には保証しない

## 同一 instance の複数 search

### 検出した問題

修正前はrootの初回 `Candidates::reset()` が `param.clear_hash_every_turn` をそのまま使っていた

`clear_hash_every_turn=false` かつ `hash_window_turns=0` では、前回searchの生存hashが `-2` として残る

同じroot候補を持つ2回目のsearchが全候補をrejectし、fresh instanceと異なる結果になった

### 修正と確認

両新backendはsearch開始時のroot resetだけ `clear_hash=true` を渡す

通常generationのresetは従来どおり `param.clear_hash_every_turn` を使うため、単一search内のcross-turn意味論は変わらない

差分testへ次を追加した

- `clear_hash_every_turn=false`
- 同じBeam instanceを同じscenarioへ2回使用
- 2回目のResultとState event列をfresh Beam instanceと比較
- callbackとvector fallbackの両方を実行

新2backendは通過し、修正前のparent backendでは `status mismatch` を再現した

同じ `BeamParam` instanceを再利用するとtelemetryも累積する

これはBeam内部stateの再利用とは別であり、fresh探索と同じ動的幅が必要ならBeamParamも再初期化する必要がある

## Action lifetime と例外

通常終了時のlifetimeは次で確認した

- copy、move construct、move assign、destructorを数えるActionを使用
- prefix Actionをmove後に元blockをclearし、slabを再利用
- callback Actionは同期的な `push_lazy()` 内でcopyし、列挙側の再利用Actionをmoveしない
- vector Actionは採用された場合だけ同期的な `push_lazy()` 内でmove
- 複数searchと全Result破棄後にlive countが0へ戻る
- ASanとUBSanでuse-after-freeと不正accessなし
- `_GLIBCXX_DEBUG` で解放済みvectorへの添字accessなし

例外注入testは行っていない

Actionのcopyやmove、State callback、`try_op()`、`apply_op()`、`rollback()` がthrowした場合、例外は伝播する

vectorとunique_ptrによる所有物はRAIIで破棄されるが、Stateをrootへ戻す保証とsearchのstrong exception guaranteeはない

この性質は通常版と同じであり、現interfaceはuser callbackをno-throw相当として使う前提になる

## 残る注意点

### 最終generationのmetadata

監査後の修正で、最終世代の `build_next_frontier()` とfrontier/LCPのswapを省いた

parent mapとAction blockはmax-turn pathに必要なため保持する

### 完全同点の順序

通常版と同じ `(parent_leaf, score)` comparatorと `std::sort` を使う

同一toolchainの差分testでは一致したが、keyが完全同点の要素順はC++規格上未規定である

cross-toolchainで順序まで固定するには、全backendへ共通tie-breakを追加する別migrationが必要になる

### 確認用assert

release実装はhot loop内の次の不変条件をassertしていない

- `cand.size()==frontier_slot.size()`
- `cand[C-1-j].action_slot==frontier_slot[j]`
- parent ordinalの単調非増加
- `h>=freed_to`

現在のtestとdebug STLで間接確認している

必要なら専用debug macroでassertとparent read counterを追加し、release buildから除去する

## 実行した検証

```bash
BEAM_RANDOM_CASES=1000 bash test/ahc/beam_search_differential.sh
BEAM_SANITIZE=1 BEAM_RANDOM_CASES=300 bash test/ahc/beam_search_differential.sh
g++ -std=c++20 -O1 -g -D_GLIBCXX_DEBUG -DTEST_BEAM_PARENT -I. \
    test/ahc/beam_search_differential.cpp -o /tmp/beam_parent_debug_fixed.bin
BEAM_RANDOM_CASES=2000 /tmp/beam_parent_debug_fixed.bin /tmp 2000
```

全て通過した

差分testはResult、State event列、threshold、Action payload、final State、history、telemetryを比較する

手作り反例にはW=1、parent gap、replacement、endpoint脱落、finished、no-candidate、max-turn、完全同点を含む
