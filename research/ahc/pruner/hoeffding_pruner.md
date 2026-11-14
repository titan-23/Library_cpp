# hoeffding_pruner.cpp

## コンストラクタの引数

- `max_seeds` は評価に使うシードの最大数である。
- `score_range` は 1 つのシードでのスコア差の最大幅である。理論上の上限から下限を引いた値を渡す。
- `delta` は最良の候補を誤って枝刈りする確率の上限である。有意水準に当たり、例えば 0.05 を渡す。
- `min_samples` は、枝刈りの判定を始めるのに要る共通シードの最小数である。
- `is_minimize` は、最小化問題なら true、最大化問題なら false を渡す。

## メソッドの引数と戻り値

- `report(seed, score)` の `seed` は評価に使ったシード値で、`score` は評価関数が返したスコアである。
- `should_prune()` は、劣っていて枝刈りすべきときに true を返す。評価を続けるべきときは false を返す。
- `next_param(was_pruned)` には、途中で枝刈りして終えたなら true、最後まで評価したなら false を渡す。
