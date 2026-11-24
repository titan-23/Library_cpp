# beam_search_turn_microbench.cpp

## 計測用の候補の順序

- fill_action は、後に渡す候補ほど評価が良くなるように候補を作成します。
- 上限付きの候補集合では、候補数が上限に達した後も残りの候補が破棄されるだけにはなりません。後の候補が既存の候補を繰り返し置き換えます。

## Action の再利用

- コールバック形式の API では 1 個の Action を再利用して構いません。
- enumerate_actions は Action の構築をループの外で行います。
- 計測用に作成する Action の初期化のコストによって、この計測の対象である候補のコンテナと arena のコストが判別できなくなることを避けるためです。arena は beam_search_turn.cpp で Action を格納する領域です。
