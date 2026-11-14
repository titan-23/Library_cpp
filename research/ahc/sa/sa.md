# sa.cpp

使い方の全体は [how_to_use_sa.md](../../../titan_cpplib/ahc/sa/how_to_use_sa.md) にある。ここには、ソースから外した引数の説明を残す。

## 初期化済みの状態を渡す sa_run

- `TIME_LIMIT` は実行時間で、単位は ms である。
- `state` は初期化済みの状態である。
- `verbose` はログを出力するかどうかを指定する。
- 戻り値の `State::Result` は最良解である。

## seed を渡す sa_run

- `TIME_LIMIT` は初期化を含む実行時間で、単位は ms である。
- `seed` は乱数のシード値である。
- `verbose` はログを出力するかどうかを指定する。
- 戻り値の `State::Result` は最良解である。

## sa_multi_run

- 2 段階の多始点焼きなましを実行する。
- `TIME_LIMIT` は実行時間で、単位は ms である。
- `FIRST_PHASE_RATIO` は第 1 段階に割り当てる時間の割合である。例えば 0.2 なら、時間の 20% を第 1 段階に使う。
- `NUM_STARTS` は第 1 段階で実行する初期状態の数である。
- `seed` は乱数のもとになるシード値である。
- `verbose` はログを出力するかどうかを指定する。
- 戻り値の `State::Result` は最良解である。

## replica_run

- `TIME_LIMIT` は実行時間で、単位は ms である。
- `NUM_REPLICAS` はレプリカの個数である。
- `SWAP_ITER_INTERVAL` は熱浴を交換する間隔で、反復の回数で数える。
- `verbose` はログを出力するかどうかを指定する。
- `record` は可視化用に記録するかどうかを指定する。
- `seed` は乱数のもとになるシード値である。
- 戻り値の `State::Result` は最良解である。
