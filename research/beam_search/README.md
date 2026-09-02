# ビームサーチ性能調査

## 状態

2026-09-01 時点のコード監査と外部調査を記録する。特定の test を速くするための調整ではなく、型、ビーム幅、
分岐数、重複率、木形状が異なる利用者へ適用できるライブラリ設計を対象とする。

調査時は旧 `beam_search.cpp` を基準として高速化を別backendへ実装し、固定幅の差分試験と合成benchmarkで検証した。
2026-09-02に高速化版を標準の `beam_search.cpp` へ昇格し、旧実装は差分試験用baselineへ移した。

## 読む順序

1. [decision.md](./decision.md): 通常版についての最終判断と実装順
2. [constant_factor_gate.md](./constant_factor_gate.md): 現方式の定数倍改善を行単位で監査した実装ゲート
3. [parent_backend_spec.md](./parent_backend_spec.md): direct parent 構造の不変条件と反例
4. [red_team.md](./red_team.md): 独立監査と反例モデルの結果
5. [cost_model.md](./cost_model.md): postorderと2種類のparent版の byte、State 操作、依存 loadの損益分岐
6. [postorder_alternatives.md](./postorder_alternatives.md): 帰りがけ順より後の構造を統合した詳細調査
7. [performance_audit.md](./performance_audit.md): beam search 全実装にまたがる性能監査
8. [candidate_pipeline.md](./candidate_pipeline.md): top-W、hash、Action、メモリ、配置
9. [parallel_and_batch.md](./parallel_and_batch.md): CPU 並列、BatchState、GPU、multi-start
10. [literature.md](./literature.md): 外部一次資料と現行コードへの適用条件
11. [benchmark_plan.md](./benchmark_plan.md): 汎用性と探索結果を保つための測定計画
12. [benchmark_results.md](./benchmark_results.md): 旧基準版、現行標準版、2種類のdirect parent版の実測
13. [constant_factor_microbench_results.md](./constant_factor_microbench_results.md): Actionとtimerの独立測定
14. [topology_microbench_results.md](./topology_microbench_results.md): postorderとparentのbuild、decode分離測定
15. [parent_implementation_audit.md](./parent_implementation_audit.md): direct parent実装の独立監査
16. [research_synthesis_audit.md](./research_synthesis_audit.md): 証明、式、実測、文書間整合の統合監査
17. [parent_compact_implementation.md](./parent_compact_implementation.md): cand導出版の実装順と検証

通常版の実装判断は `decision.md`、全 backend にまたがる順位は `performance_audit.md` を正本とする。
他の文書の P0/P1 は、その分野内での実装順または比較優先度であり、全体順位ではない。

横断的な結果表では `benchmark_plan.md` の記号を使い、`P` は親種類数、`F` はactive target pool数、
`Q` は平坦木の総token数、`R` は実際に読んだtoken数とする。詳細な導出では各文書の局所記号表を優先する。
文書間で比較するときは `M_tour`、`L_parent`、`H_tree` のような説明的な名前へ展開する。

## 実装と検証の入口

- [beam_search.cpp](../../titan_cpplib/ahc/beam_search/beam_search.cpp)
  先頭prefix除去、32 bit slot、prefix確定、Action保存、timer改善を含む現行標準版
- [beam_search_baseline.cpp](../../test/ahc/beam_search_baseline.cpp)
  64 bit ActionIdを使う差分試験用の旧基準版
- [beam_search_parent.cpp](../../titan_cpplib/ahc/beam_search/beam_search_parent.cpp)
  独立 `frontier_slot` を持つdirect parent correctness oracle
- [beam_search_parent_compact.cpp](../../titan_cpplib/ahc/beam_search/beam_search_parent_compact.cpp)
  frontierを `cand` から導出するdirect parent比較版
- [beam_search_differential.sh](../../test/ahc/beam_search_differential.sh)
  4 backendのResult、State呼出し、history、telemetryの差分試験
- [parent_backend_model.py](./parent_backend_model.py): parent写像、LCP、prefix、State walkの構造model
- [cost_model.py](./cost_model.py): postorder、parent oracle、parent compactのbyte model
- [topology_microbench.cpp](./topology_microbench.cpp): metadata buildとsuffix decodeだけを分離するdriver
- [run_topology_microbench.sh](./run_topology_microbench.sh): topology kernelの再現script

`benchmark_results.md` のsource hashは昇格前の計測snapshotを示す。昇格では性能ロジックを変えず、配置と公開名を
変更した。性能計測は取り直していない。

## 現時点の要約

- 固定深さ標準版は、traP の記事にある帰りがけ順の `tour / leaf / trace` を使う高速化後の方式である。
- これは現行処理の説明であり、帰りがけ順方式が常に最速、または後継構造が存在しないという意味ではない。
- 通常版の成立条件では、現行順は固定した開始 cursor からの状態辺遷移回数の下限を厳密に達成する。
- 毎世代の `next_tour` 再構築はその下限に含まれず、32 bit slot と parent-map を別々に比較できる。
- direct parent は `M_tour/W` が大きい形で write を減らせるが、単一親、依存 miss、`G>>E_live` では負け得る。
- topology kernelでも長いsuffixのparent decodeはslot decodeの5.41倍から21.83倍となり、依存loadの弱点を確認した。
- 最優先の仮説は、active hash と可変ターン版の一時候補・Action に採用イベント数 `A` が漏れる問題である。
- Action の遅延保存、target-turn pool の過剰確保、segtree 初期化、不要統計は低リスク側から測りやすい。
- State copy、Euler tour、Radix、疎な可変ターン木は、State と木形状に応じて勝敗が変わる別 backend とする。
- 並列化は一つの mutable State を共有せず、独立 State と local top-W、global merge を使う別 backend にする。
- best-first、multi-start、多様化、bounded history は探索結果を変え得るため、定数倍改善と分けて評価する。

## Backend 選択の入口

- 現行標準版を通常利用し、旧baseline、parent oracle、parent compactを検証・比較用backendとして残す。
- State copy が小さく、apply/rollback が相対的に高価なら naive 版も候補にする。型サイズだけでは自動選択しない。
- 単一子が多く `compose()` が安価なら Compose / Radix を比較する。連続配置と物理縮約の損益は実測で決める。
- 可変ターンは dense 平坦木を基準とし、`K << L` かつ `Q/K` や `R/K` が大きい場合だけ sparse 版を試す。
- exact CPU 並列は clone 可能な独立 State、BatchState は明示的な SoA/batch 契約を要求する別 entry point とする。
- multi-start は1回の探索の高速化ではなく、独立 seed の throughput と品質多様化を測る wrapper とする。

完全同値は固定幅と通常の値型契約で検証する。動的幅では高速化自体が後続幅を変えるため、同じ時間予算での
品質分布を別に測る。backend 名、selector、hash、calendar、telemetry、停止条件を結果へ必ず記録する。

構造モデルは `parent_backend_model.py`、byte モデルは `cost_model.py` で再現できる。前者は既定の30,000 randomと
73,085 topology の別全探索をassertion有効で通し、後者は `W=1`、`P=1`、`P=W`、`G>>E_live` を再計算する。
