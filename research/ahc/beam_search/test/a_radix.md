# a_radix.cpp

- [a.cpp](../../../../titan_cpplib/ahc/beam_search/test/a.cpp) と同じ State と Action を、beam_search_radix.cpp の BeamSearchRadix で探索します。
- 以下の記録は [a.md](a.md) と同じ内容です。

## Action の合成

- 合成に対応するため、`chain` に 2 番目以降の操作を格納します。
- chain の各操作は、rollback で途中の状態を復元するために `pre_*` を保持しています。
- 先頭の `d`, `pre_*`, `nxt_*` は、chain が空であれば 1 回の操作を表し、chain が空でなければ先頭の操作を表します。
- `nxt_*` は常に、合成した操作をすべて適用した後の状態を表します。
- Action のメモリ使用量は少ないほうが望ましいです。score と hash の記録は廃止したいと考えています。

## enumerate_actions

- enumerate_actions と try_op を一体にした新方式のインターフェースです。
- 生成した action を submit に渡すと、try_op、採否の判定、候補への追加までを submit が行います。
- 途中で vector を作成しません。
- `submit.threshold()` で現在の最悪値を取得でき、早い段階の枝刈りに使用できます。この実装では枝刈りを try_op 側で行っています。
- 旧方式は、遷移可能な Action を引数の vector `actions` に格納します。`#if 0` で無効にした実装が残っています。

## gen_param の引数

- `max_turn` は探索する最大ターン数です。
- `beam_width` はビーム幅です。
- `time_limit` は制限時間です。
- `is_adjusting` は制限時間に合わせるかどうかを指定します。
- `clear_hash_every_turn` はハッシュ辞書を毎ターン消去するかどうかを指定します。
