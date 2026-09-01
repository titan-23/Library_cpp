# cand-derived direct parent backend 実装記録

## 目的

`beam_search_parent.cpp` の正しさ優先oracleから次の2配列を除く

```text
frontier_slot
next_frontier_slot
```

frontier ordinalからAction slotへの写像は、既に成立している不変条件から導出する

```text
frontier_action_slot(j) = cand[cand.size() - 1 - j].action_slot
```

oracleは比較対象として残し、compact版を
`titan_cpplib/ahc/beam_search/beam_search_parent_compact.cpp` に分離した

## old cand の寿命

単純に `frontier_slot` を削ると `finalize_generation()` の `cand.clear()` 後に親写像を失う

compact版は二重 `CandIdx` bufferを追加せず、次の順で確定する

1. Action blockとparent blockの格納先を準備
2. 全最終candidateのparent slotを旧 `cand` から `parent_block[depth]` へ書く
3. parent書き込み完了後に旧 `cand` をclear
4. candidate selector順にActionをmove construct
5. 同じ順に新 `cand` を構築

親写像は次になる

```text
parent_block[depth][child_slot] =
    cand[old_cand_size - 1 - candidates.next_beam[child_slot].parent_leaf].action_slot
```

深さ1だけはroot sentinelを使う

この順序によりAction slot、Actionのmove順、`cand` の入力順をoracleから変更しない

## frontier walk

current frontierのtarget Actionは、逆走査中の `CandIdx` から直接読む

```text
i = cand.size() - 1 - j
c = cand[i]
target_slot = c.action_slot
```

rootの `cand` はsortしない

generation 2以降だけoracleと同じ `(parent_leaf, score)` comparatorでsortする

`parent_leaf` は引き続きfrontier ordinalであり、Action slotと同一視しない

## LCP

current frontier幅は次から復元できる

```text
old_size = adjacent_lcp.size() + 1
```

frontierは常に非空のgenerationだけLCPを構築するため、W=1でも `old_size==1` になる

entry LCPと隣接LCPの下降区間minimum、同一親兄弟のLCP、decode停止境界はoracleと同じになる

最終generationではoracleと同様に不要な次frontier LCPを構築しない

## prefix と終了処理

current frontier幅の判定だけ `frontier_slot.size()` から `cand.size()` へ置き換えた

current-frontier共通prefix、endpoint保持、W=1の全prefix確定、dangling parentの停止条件は変更していない

finished、no-candidate、max-turn、history、telemetryもoracleの処理を維持する

search開始時のroot candidate resetはhashを強制clearし、同一Beam instance再利用時の前探索hashを捨てる

通常generationでは `param.clear_hash_every_turn` を尊重するため、単一search内のcross-turn重複除外は変わらない

## コスト差

削減する定常metadataはcurrentとnextのfrontier slot容量になる

幅が同程度なら概ね `8W` byteのresident容量と、非terminal generationごとの `4W` byteのfrontier writeを除く

一方、旧 `cand` を保持したparent先行確定のため、最終candidate列を2回読む

dependent parent load数、parent block容量、adjacent LCP処理、State操作数は変わらない

従ってoracleより速いか、slot-only postorderより速いかは4 backend benchmarkで判定する

## 検証

差分testへ `TEST_BEAM_PARENT_COMPACT` を追加し、次の4 backendを同じ出力へ比較した

1. 通常版
2. slot-only optimized版
3. direct parent oracle版
4. cand-derived direct parent compact版

比較対象はResult、Action列、State event列、threshold、history JSON、final State、telemetryになる

次を実行して全て通過した

```bash
BEAM_RANDOM_CASES=1000 bash test/ahc/beam_search_differential.sh
BEAM_SANITIZE=1 BEAM_RANDOM_CASES=300 bash test/ahc/beam_search_differential.sh
g++ -std=c++20 -O1 -g -D_GLIBCXX_DEBUG -DTEST_BEAM_PARENT_COMPACT -I. \
    test/ahc/beam_search_differential.cpp -o /tmp/beam_parent_compact_debug.bin
BEAM_RANDOM_CASES=1000 /tmp/beam_parent_compact_debug.bin /tmp 1000
g++ -std=c++20 -O2 -DTEST_PARENT_COMPACT -DTEST_NON_MOVABLE -I. \
    test/ahc/beam_result.cpp -o /tmp/beam_result_compact_nonmovable.bin
/tmp/beam_result_compact_nonmovable.bin
```

手作りcaseにはroot非sort、selector replacement、親gap、endpoint脱落、W=1、finished、no-candidate、
max-turn、history、callback Action再利用、vector fallback、複数searchを含む

`backend_benchmark.cpp` の15 scenarioでもoracleとcompactを各1回instrumentation実行し、次が全て一致した

- expanded、apply、rollback
- Actionのdefault construct、copy construct、move construct、copy assign、move assign、destruct回数
- score、turns、status、path長、result digest、State stream digest
