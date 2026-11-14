# beam_search_turn_microbench.cpp

## 計測用の候補の順序

- fill_action は、後に渡す候補ほど評価が良くなるように作る。
- 上限つきの候補集合は、一度埋まった後に残りを捨てるだけにならない。後の候補が既存の候補を繰り返し置き換える。

## Action の使い回し

- コールバック形式の API では、1 個の Action を使い回してよい。
- enumerate_actions は、Action の構築をループの外に置く。
- 計測用に作る Action の初期化の費用が、この計測の対象である候補の容器と arena の費用を覆い隠さないようにするためである。arena は beam_search_turn.cpp で Action を格納する領域である。
