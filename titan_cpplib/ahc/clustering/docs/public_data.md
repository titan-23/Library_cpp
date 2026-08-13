# 公開されているクラスタリング問題

## 結論

TSPのTSPLIBほど一つに定まった問題集はない。目的に応じて次を組み合わせる。

- 最良費用が分かる小規模問題: 実装と解の質を厳しく確認
- 正解ラベル付きの中規模、大規模問題: 分類上のまとまりと規模への対応を確認
- 個数固定の最適値付き問題: 個数制約付きK-meansとSAを確認
- 固定した人工問題: AHCで現れやすい規模、重なり、外れ値、個数条件を確認

公開データそのものはこのディレクトリへ含めない。各配布元の利用条件を確認し、変換後の問題も手元の `data/` に置く。

## SOS-SDPの公開結果がある通常クラスタリング

[Ground truth clustering is not the optimum clustering](https://github.com/shudianzhao/GroundTruth_VS_OptimalCluster) には、二乗誤差を目的とする通常クラスタリングをSOS-SDPで解いた小規模問題がある。配布元は結果を厳密解と説明している。変換処理はコミット `e60cb1c2dedc717a54a67a2c1caac543c49a34b9` に固定している。

次のコマンドは固定したコミットを一時ディレクトリへ取得し、13種類の実データを `.tcb` へ変換する。一つのデータに複数のクラスタ数の結果があるため、`データ名 × クラスタ数` ごとに別の問題を作る。公開されている所属から二乗誤差を再計算し、`best_known_cost` に記録する。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  download-sos-sdp-reference \
  titan_cpplib/ahc/clustering/benchmark/data
```

取得URL、コミット番号、ZIPのSHA-256は `sos_sdp_reference_source.txt` へ保存する。

すでに配布元を展開している場合は、通信せずに変換できる。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  sos-sdp-reference \
  /path/to/GroundTruth_VS_OptimalCluster \
  titan_cpplib/ahc/clustering/benchmark/data
```

配布元はGPLv3である。取得したデータと結果の再配布条件は配布元の `LICENSE.md` を確認する。

この問題集の `best_known_cost` は、`Results_clustering` に実際に存在する各 `class_k_*` 列から再計算した公開参考費用である。分類ラベルの種類数と、結果が公開されているクラスタ数は一致するとは限らない。例えば `arrhythmia` の分類ラベルは13種類だが、公開結果はクラスタ数2〜7である。

`opt_mssc_real_data_true_label.csv` の数値は `best_known_cost` に使わない。データによっては座標表と尺度が一致せず、公開結果の費用でもないためである。このCSVは変換する13種類のデータ名を選ぶためだけに使う。点の順序は、座標表、分類ラベル付き座標表、公開結果付き座標表を行ごとに照合する。

`reference_labels` は元データの分類ラベルであり、公開結果の所属とは限らない。参考費用との差と調整ランド指数を別に見る。

実測では `iono, k=4` において、Hamerly法の複数初期値が公開参考費用を約0.03%下回った。したがって、この配布物だけを根拠に最適性が証明済みとは扱わない。`difference_from_best_percent` が負なら異常として捨てず、より良い解が得られた候補として所属と費用を再確認する。

## 中規模、大規模問題

[Clustering basic benchmark](https://cs.uef.fi/sipu/datasets/) には、次のような点群がある。

- S1-S4: 5000点、15クラスタ
- A1-A3: 3000点から7500点、20から50クラスタ
- Birch: 100000点、100クラスタ
- Unbalance: 大きさが不均衡なクラスタ
- DIM、G2: 次元数や分離の難しさを変えた問題

正解ラベルや中心は比較用の情報であり、二乗誤差の大域的な最適解を保証するものではない。大規模問題での速度、安定性、調整ランド指数を見るために使う。

ヘッダのない空白区切りの座標表は `convert-table` で変換できる。次は二次元、15クラスタの例である。

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  convert-table s1.txt s1.tcb \
  --name s1 \
  --source "SIPU clustering basic benchmark" \
  --dimension 2 \
  --clusters 15 \
  --labels s1-label.pa \
  --label-base 1
```

ラベルファイルの形式は配布物ごとに確認する。`prepare_public_data.py` は、点数と同じ個数の整数が空白区切りで並ぶファイルを受ける。

[Clustering Benchmarks](https://clustering-benchmarks.gagolewski.com/) は複数の既存問題集を版付きでまとめている。さまざまな形状や正解ラベルを横断的に比較する場合に向く。版を固定し、使った問題名を結果と一緒に残す。

## 個数固定の最良費用付き問題

[Cardinality-constrained minimum sum-of-squares clustering](https://link.springer.com/article/10.1007/s10107-023-02021-8) では、クラスタごとの個数を固定した29個の実データ問題について、最適性を確認した結果が報告されている。実装は [cc-sos-sdp](https://github.com/antoniosudoso/cc-sos-sdp) で公開されている。

配布場所や元データの形式が問題ごとに異なるため、自動取得には含めていない。数値表を次の形へ直すと、一般変換を使える。

```text
点数 次元数
x11 x12 ...
x21 x22 ...
...
```

```bash
python3 titan_cpplib/ahc/clustering/benchmark/prepare_public_data.py \
  convert input.txt output.tcb \
  --name problem_name \
  --source "cc-sos-sdp benchmark" \
  --clusters 3 \
  --sizes 20,30,50 \
  --best-known 123.456
```

論文や配布結果から個数と最良費用を転記するときは、座標の前処理、クラスタ数、個数の順序、費用の定義が同じかを必ず確認する。

## 独立した比較実装

[University of Eastern Finlandのクラスタリングソフトウェア](https://cs.uef.fi/ml/software/) にはRandom Swapなどの実装がある。ライブラリ内の手法だけで順位を付けると共通の誤りや偏りを見逃すため、最終確認では同じ入力と総時間で独立実装とも比較する。

現時点の `run_benchmark.cpp` は外部実装を直接呼ばない。まず内部手法の正当性と近傍の比較を固め、その後、外部実装の入出力を変換する小さな実行用プログラムを別に追加する。

## 取得記録

結果を残すときは、少なくとも次を一緒に記録する。

- 配布元URL
- 版、公開日、またはコミット番号
- 元ファイルのSHA-256
- 変換コマンド
- 座標の標準化や列削除の有無
- クラスタ数と個数条件
- 最良費用が既知か、単なる参考値か

同じ名前のデータでも、標準化、欠損値処理、分類列の除去によって二乗誤差は変わる。最良費用を比較するときは、点群が完全に同じであることを優先して確認する。
