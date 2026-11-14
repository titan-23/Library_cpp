# a.cpp

## Action の合成

- 合成に対応するため、`chain` に 2 手目以降を格納する。
- chain の各手は、rollback で途中の状態を戻すために `pre_*` を保持している。
- 先頭の `d`, `pre_*`, `nxt_*` は、chain が空なら 1 手を表し、chain があれば先頭の手を表す。
- `nxt_*` は常に、合成した手をすべて適用した後の状態を表す。
- Action のメモリ量は少ないほうがよい。score と hash の記録はなくしたいと考えている。

## enumerate_actions

- enumerate_actions と try_op を一体にした新方式の受け口である。
- 生成した action を submit に渡すと、try_op、採否の判定、候補への追加までを submit が行う。
- 途中で vector を作らない。
- `submit.threshold()` で現在の最悪値を取得でき、早い段階の枝刈りに使える。この実装では枝刈りを try_op 側に任せている。
- 旧方式は、遷移可能な Action を引数の vector `actions` に入れる。`#if 0` で無効にした実装が残っている。

## gen_param の引数

- `max_turn` は探索する最大ターン数である。
- `beam_width` はビーム幅である。
- `time_limit` は制限時間である。
- `is_adjusting` は制限時間に合わせるかどうかを指定する。
- `clear_hash_every_turn` はハッシュ辞書を毎ターン消すかどうかを指定する。
