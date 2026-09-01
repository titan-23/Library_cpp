# 通常版 beam search 実装前レッドチーム監査

## 結論

`beam_search.cpp`、`decision.md`、`parent_backend_spec.md` を独立に照合した

次の不変条件を守る限り、slot-only postorder と direct parent の構造正当性を崩す新しい反例は見つからなかった

- current frontier は実展開順を使う
- next frontier の親ordinalは単調非増加とする
- Action slot と frontier ordinal を分離する
- 最初の遷移に `entry_lcp`、以後に隣接LCPの区間最小値を使う
- prefix 解放では走査終了endpointを残す
- decode は確定境界直下のparentを読まない
- max-turn復元はState rollback用の `trace` と別scratchを使う

一方、実装前に解決すべきinterface上の反例を1件、性能比較上の注意を2件確認した

## 重大度と実装ゲート

| ID | 重大度 | 発見事項 | 実装ゲート |
|---|---|---|---|
| RT-H1 | 高 | callbackのActionをmoveすると現行と非同値 | callbackは採用時copy、vector fallbackだけmove |
| RT-M1 | 中 | oracle版direct parentのwrite式が1配列分不足 | 実測counterへ`frontier_slot` writeを含める |
| RT-M2 | 中 | telemetryとclock統合は観測値を変える | 互換modeと物理量modeを分離 |
| RT-L1 | 低 | 完全同点の順序は標準上未規定 | 同じ入力列、comparator、sortを維持 |
| RT-I1 | 情報 | blockのmove constructionは新しい型要件を加えない | 値型契約だけを明記 |

高は初回実装を止める条件、中は性能判断または公開観測値の確定条件、低は差分試験条件を表す

## RT-H1 callback Actionの所有権

現行callback経路は `Submitter::operator()(Action &a)` から `Candidates::push(..., a)` を呼び、Actionをcopyする
採用時だけ保存する方針自体は正しいが、次のlambdaは現行と同値ではない

```cpp
[&] { return std::move(a); }
```

利用側は1個のActionを再利用して列挙できる

```cpp
Action a;
for (int choice = 0; choice < n; ++choice) {
    a.choice = choice;
    submit(a);
}
```

Actionがvectorなどの共通payloadを持ち、loopがchoiceだけを書き換える場合、最初の採用でpayloadが空になる
2個目以降の `try_op()` が受け取る値まで変わるため、単なるcopy回数の差ではない

初回実装では経路ごとに次を固定する

```text
callback        : push_lazy(..., [&] { return a; })
vector fallback : push_lazy(..., [&] { return std::move(action); })
```

再利用lvalue、毎回新規Action、全棄却、一部置換を別々に差分試験する

## RT-I1 move constructionとsource compatibility

現行の世代block構築はdefault construction後のmove assignmentである
追加監査では `emplace_back(std::move(action))` が新しいconstructibility要件を加えるという反例は成立しなかった

現行 `Candidates::push()` 自体が、関数引数のActionを `std::move(action)` から候補Actionへ構築する
従って現行通常版も `Action&&` からActionを構築できることを既に要求する

世代blockのmove constructionへ、この理由だけでtrait fallbackを置く必要はない
ただしDAMMY Actionとcandidate配列のため、default constructible要件まで消えるわけではない

copy、move、代入、破棄の外部副作用まで同値にすることは最適化と両立しない
既存調査と同じく通常の値型契約を前提にし、その契約を公開文書とtest名へ残す

## RT-M1 direct parent oracleのwrite量

`decision.md` の概算は次である

```text
direct parent write = q * W + l * W
```

ただし最初のcorrectness oracleは、selector slotとfrontier ordinalの混同を防ぐため
`parent` と独立した `frontier_slot` を毎世代書く

共通の `cand` writeを除外しても、oracle版の固有writeは概ね次になる

```text
q * W parent + q * W frontier_slot + l * W adjacent_lcp
```

従ってoracle版の損益分岐を `M/W > 1` と解釈してはならない
正当性確認後は `cand[C-1-j].action_slot` からfrontier mappingを導出できるが、これは別最適化として測る

## RT-M2 telemetryとclock

初回未参照prefixを除くと物理 `tour.size()` は現行より小さくなる
direct parentには物理tour自体がなく、互換tour長とparent metadata量は別の量である

`BeamParam::pool_size_sum` はpublicであり、`timestamp()` の値はsearch後に観測できる
次の2 modeを混ぜないことを実装ゲートとする

- compatibility telemetryは現行が書いたはずの論理tour長を渡す
- physical telemetryはbackend固有のbyteまたは要素数として別fieldへ記録する

また、2回のtimer readを1回へまとめると `is_adjusting=true` の残時間入力が変わり、幅が1ずれる場合がある
固定幅の構造同値試験と、幅列record/replayを先に通し、時間調整込みの結果一致は要求しない

## slot-only postorder監査

64 bit IDからgenerationを除ける根拠を全read箇所で再確認した

- `trace[d]` は深さdのslotである
- `CandIdx` のslotはloopのturnから深さが分かる
- tour fragmentは既知のtrace深さ区間へcopyされる
- `confirm_and_free()` は確定する深さを持つ
- finished pathは `freed_to+1..turn` の深さを持つ
- max-turn pathは開始深さ付きでmaterializeできる

従ってtoken単体をdecodeするhelperを残さず、全accessを `act(depth, slot)` にすれば情報は失われない
iteratorだけを受け取る旧 `materialize()` は禁止し、開始深さを必須引数にする

先頭prefix省略後の `leaf[0]=0` も再確認した
`W=1` のtourが空でも、次世代の最初のtargetはtraceとcurrent Actionから復元できる

prefix境界は次で安全である

```text
C == 1 : confirm_and_free(d + 1)
C >= 2 : h = min(adj_lcp), confirm_and_free(h + 1)
```

entry jumpは次frontier内部の共通prefixを制限しないため、このminimumへ含めない

## direct parent監査

次の既知反例を再現し、仕様のgateが必要十分であることを確認した

- `parent_leaf == Action slot` はroot候補2個の逆走査で壊れる
- `entry_lcp` 省略はendpoint B、唯一survivor Aで壊れる
- 区間の端1個だけを見るLCPは `[2, 0, 2]` のgapで壊れる
- 仮採用時だけparentを書くとselector replacementで壊れる
- survivorだけでprefixを確定すると旧endpointのrollback Actionを失う
- 深さ `freed_to+1` のparentを読むと解放済みblockを参照する
- max-turn復元でtraceを上書きするとfinal Stateのrollback元を失う

新しい構造反例は見つからなかったが、初回実装では最適化を混ぜず次を維持する

- 独立 `frontier_slot`
- final selector集合からparentを構築
- absolute-depth `entry_lcp` と `adj_lcp`
- 現行と同じ `cand` 入力列、`std::sort`、逆走査
- prefixは互換境界から開始

## finished、no-candidate、max-turn、history

### finished

finished Actionはfrontierへ格納せず、発見時の `trace` からpathを保存する
同じgenerationの全葉を最後まで走査し、strictにscoreが改善した場合だけpathを置換する

returnはcandidate確定、history survivor snapshot、prefix解放より前であり、現行と同じ順を保つ

### no-candidate

`found_finished` を先に判定する
rootではturns 0、通常generationでは現在のturnを返し、`final_state` は構築しない

### max-turn

bestはsorted後を含む現行 `cand` の物理順でstrict `<` scanする
parent復元は別scratchへ書き、Stateは深さ `D-1` の旧endpointとtraceを保持する

### history

`parent_leaf`、history node ID、Action slotを同一視しない
node IDの増分位置、threshold読出し、survivor記録時点を現行と一致させる

snapshotの `active_node_ids` は `unordered_set` 由来なので、差分試験ではsetとして比較する

## State walk下限の追加監査

各generationについて、開始endpointをs、実展開順のtarget葉を `v[0..C)` とする
これらを結ぶ最小部分木をHとした

modelへ次のassertionを追加した

```text
dist(s,v[0]) + sum dist(v[i-1],v[i])
    == 2 * |E(H)| - dist(s,v[C-1])

dist(s,v[C-1]) == max_i dist(s,v[i])
```

第1式より、終点を最後のtargetへ固定したwalk下限を達成する
第2式より、target中で終点を自由に選べる場合も `2|E(H)| - max dist(s,v)` の下限を達成する

成立条件は通常版が持つ次の4点である

1. 全targetが同じ深さにある
2. frontierがDFS-compatibleである
3. sourceが旧frontierの走査終了側endpointである
4. 新frontierの親ordinalがsourceから離れる向きへ単調に進む

任意の葉reorder、可変深さtarget、途中のState relocationへはこの結論を拡張しない
特に親groupを交互に再訪する順序では同じ辺を3回以上通り得る

## model結果

`parent_backend_model.py` へprefix復元、open walk、自由終点のassertionを追加した
次のstressをassertion有効で実行した

```sh
python3 research/beam_search/parent_backend_model.py \
  --trials 100000 --max-depth 20 --max-width 12 \
  --exhaustive-depth 4 --exhaustive-width 3
```

結果は次である

```text
topologies=105079
generations=1120860
transitions=7209407
prefix_generations=1015782
prefix_reconstructions=19639116
open_walk_generations=1120860
end_free_generations=1120860
```

random slot permutation、width増減、親gap、単一survivor、長いjumpを含み、assertion failureはなかった

## 実装開始判定

RT-H1を実装方針へ反映すれば、次の順でprototypeへ進める

1. slot-only postorderを構造変更なしで作る
2. direct parent oracleを独立 `frontier_slot` 付きで作る
3. 固定幅、通常の値型契約でState呼出し列とResultを差分比較する
4. finished、no-candidate、max-turn、history、完全同点を個別に通す
5. 幅列record/replayでdynamic widthの構造差を除く
6. その後にfrontier導出、精密prefix、Action lifecycle最適化を1個ずつ加える
