# beam_search_compose デバッグ進捗メモ

## 現状

- `beam_search_compose.cpp` に Phase 1 (ghost 方式) compose を実装済み
- `gamex.cpp` に compose 実装を入れて検証中
- **compose 無効 (`return false`) では正常動作**（score=10501）
- **compose 有効では `apply_one` の `'x'` assert にヒットしてクラッシュ**

## 判明したバグ (修正済み)

### parent_leaf の index 方向が逆

`snapshot_leaf_actions()` の元実装:
```cpp
prev_leaf_action_ids[i] = cand[i].action_id;  // 誤
```

子 cand の `parent_leaf` は親 DFS の **reverse iter 順** (= `now_leaf_idx`) で
振られる。`cand_T_sorted[i]` は DFS で `i = size-1` から `i = 0` の順に処理され、
`now_leaf_idx = next_leaf.size() = size-1-i`。よって emit した子の
`parent_leaf = size-1-i`。

`parent_leaf = p` を持つ子の親 = `cand_T_sorted[size-1-p]`。snapshot は reverse 順に格納する必要がある:

```cpp
prev_leaf_action_ids[n - 1 - i] = cand[i].action_id;  // 正
```

この修正で turn 5 のクラッシュ → turn 6 まで進むようになった (= 部分的修正)。

## 残っているバグ (未解決)

- 修正後も turn 6 で同じ `apply_one` の `'x'` assert にヒット
- クラッシュ瞬間の action は **primitive (chain.size()=0)**、`dir='L'`
- そのときの状態で agent k7 の L 方向は `'x'` (`get_actions` が emit しないはずの方向)
- → state at apply ≠ state at emit

### 観測事実
- `compose_pass` のログでは合成自体は成功 (`child_slot.chain=1`, `parent_slot.ghost=1`)
- chain 拡張を禁止 (`chain.size()==0` のみ compose) しても同じターンでクラッシュ
- chain 拡張を許可しても同様 (chain 拡張だけが原因ではない)
- baseline (compose 無し) は `score=10501`、正常終了

### 仮説（未検証）

1. **rollback 順序の不一致**: composed の apply/rollback が完全な逆操作になっていないケースがある
   - apply: `apply_one(primary.dir)` → `apply_one(chain[0].first)` ...
   - rollback: chain reverse → primary。これは数学的には逆だが、`is_moved` フィールドが apply の各ステップで一致しないと壊れる
   - `is_moved` は **try_op 時点** で記録される。state at apply ≠ state at try_op だと bit が合わず rollback が破綻

2. **lca_dist / 深さ計算と composed の不整合**
   - 現状の lca_dist は depth ベース。composed が複数 game turn を 1 depth に圧縮するため、`leaf[]` のセグメント長と「ロールバック・適用すべき game turn 数」が乖離する場合がある可能性
   - ただし「ghost を含めて pair で考えれば depth = game turn」なので、理論上は整合するはず

3. **f=0 first iter の特殊性**
   - 各 outer turn の最初の iter (`f=0`) は rollback を `lca_dist` 回しか実行しない (subsequent iter の `lca_dist+1` ではない)
   - 前 turn 末の state (cand_{T-1}[0]'s leaf) が composed 経由で到達された場合、ここの state alignment が崩れている疑い

4. **score / hash の不整合**
   - candidates.push 時の cand.score は **child 単体の try_op スコア**。composed apply 後の state では合計 game turn 分の seen/score 加算が起きる
   - これは BeamCandidate の score / hash ストア値とは無関係なので（engine は cand.score を選別に使うだけ）影響しないはず

## 修正前後の挙動比較

| compose 状態 | クラッシュ時のターン |
|---|---|
| `return false` (無効) | クラッシュなし (score=10501) |
| chain.size()==1 制限 + 旧 snapshot (誤) | turn 5 |
| 任意の chain 拡張 + 旧 snapshot (誤) | turn 5 |
| chain.size()==1 制限 + 新 snapshot (正) | turn 6 |
| 任意の chain 拡張 + 新 snapshot (正) | turn 6 |

→ snapshot 修正は確実に効いている。残バグは別経路。

## 次にやるべきこと

1. **composed の apply → rollback → apply で state hash が一致するか単体テスト**
   - 単純な合成 Action を作って、apply・rollback の往復で `hash` / `score` / `pos` が完全に戻ることを確認
   - もし戻らなければ `is_moved` キャプチャに問題（state at try_op ≠ state at apply）

2. **turn 5 のクラッシュ寸前にちょうど state を比較**
   - composed action が turn 5 までに 1 つ適用されている (apply 208 chain=1)
   - その composed の apply / rollback が完全に対称か検証
   - 具体的には: 「composed apply → 同じ composed の rollback」で state が初期状態に戻るかをデバッグログで確認

3. **engine の `f=0` 経路を `f=1` に近づけ、最初の iter でも rollback を `lca_dist+1` 実行する** 変更を試す
   - 仮説 3 の検証

4. **compose を turn 4 以降のみで許可**（turn 1〜3 では `return false`）するなどして、bug 起きる最小ケースを特定

## 修正済みコード状態

- `beam_search_compose.cpp`:
  - `snapshot_leaf_actions()` 修正済み（reverse 順格納）
  - その他は Phase 1 ghost 実装のまま
  - デバッグログは削除済み

- `gamex.cpp`:
  - compose は現在 `chain.size()==0` 限定の 1 段のみ
  - `Action` に `vector<pair<char, short>> chain` を追加済み
  - `apply_op`/`rollback` を chain 対応 (`apply_one`/`rollback_one` ヘルパー導入)
  - `print_ans` の出力は composed 展開対応済み (`a.dir` + `a.chain[*].first` を順に出力)
  - デバッグ print は削除済み

## 重要メモ

- compose の **本懐は apply 回数削減**。state alignment が前提なので、ここが崩れると bug は表に出る
- ghost 方式は「構造を変えずに apply を skip」する設計だが、composed が複数 game turn を 1 depth に圧縮することで、engine の depth 一様前提と微妙にズレる可能性
- a.md §「効かない例」に挙がっている「apply が既に O(1) の問題」のように、game state が連鎖反応 (= state-dependent) する問題では特に bug が出やすい
- gamex.cpp は K=8 agents の同時移動なので、agent 間の干渉なし。state は agent ごと独立だが、global hash/score は依存

## 関連設計ドキュメント

- `a.md` — compose の motivation と設計判断
- `beam_search_compose.cpp` の冒頭コメント — Phase 1 ghost 方式の実装説明
