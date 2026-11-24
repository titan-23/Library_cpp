# sa.cpp

使用方法の全体は [how_to_use_sa.md](../../../titan_cpplib/ahc/sa/how_to_use_sa.md) に記載しています。この文書にはソースから削除した引数の説明を記載します。

## 初期化済みの状態を渡す sa_run

- `TIME_LIMIT` は実行時間で、単位は ms です。
- `state` は初期化済みの状態です。
- `verbose` はログを出力するかどうかを指定します。
- 戻り値の `State::Result` は最良解です。

## seed を渡す sa_run

- `TIME_LIMIT` は初期化を含む実行時間で、単位は ms です。
- `seed` は乱数のシード値です。
- `verbose` はログを出力するかどうかを指定します。
- 戻り値の `State::Result` は最良解です。

## sa_multi_run

- 2 段階の多始点焼きなましを実行します。
- `TIME_LIMIT` は実行時間で、単位は ms です。
- `FIRST_PHASE_RATIO` は第 1 段階に割り当てる時間の割合です。例えば 0.2 の場合は時間の 20% を第 1 段階に使用します。
- `NUM_STARTS` は第 1 段階で実行する初期状態の数です。
- `seed` は乱数のもとになるシード値です。
- `verbose` はログを出力するかどうかを指定します。
- 戻り値の `State::Result` は最良解です。

## replica_run

- `TIME_LIMIT` は実行時間で、単位は ms です。
- `NUM_REPLICAS` はレプリカの個数です。
- `SWAP_ITER_INTERVAL` は熱浴を交換する間隔で、反復の回数で指定します。
- `verbose` はログを出力するかどうかを指定します。
- `record` は可視化用に記録するかどうかを指定します。
- `seed` は乱数のもとになるシード値です。
- 戻り値の `State::Result` は最良解です。
