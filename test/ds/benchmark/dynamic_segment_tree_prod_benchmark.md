# 動的セグメント木の prod と境界探索の高速化の測定記録

2026-11-13。DynamicSegmentTree、DynamicSegmentTreeInit、DynamicLazySegmentTree の prod と、通常版と初期値付き版の max_right、min_left を対象にした。

## 測定条件

- CPU は AMD Ryzen Threadripper 3990X (Zen 2)、コンパイラは GCC 12.3.0、`-std=c++17 -O2 -DNDEBUG`。
- 変更前は HEAD (`9634bd5`) のヘッダ。CPU 4 に固定し、交互に 5 回ずつ実行した中央値。チェックサムは全行で一致した。
- 操作別の値は [dynamic_segment_tree_prod_benchmark.cpp](dynamic_segment_tree_prod_benchmark.cpp) で測った。N=1e9 に 25 万個の点更新 (遅延版は 25 万個の区間加算) をしてから 100 万回の操作を測る。
- 混合の値は既存の [dynamic_segment_tree_benchmark.cpp](dynamic_segment_tree_benchmark.cpp)、[dynamic_segment_tree_monoid_benchmark.cpp](dynamic_segment_tree_monoid_benchmark.cpp)、[dynamic_lazy_segment_tree_benchmark.cpp](dynamic_lazy_segment_tree_benchmark.cpp) の既定の条件 (Q=5e5) で測った。

## prod

### 原因

- prod は、l と r が別の子に分かれたあと、左端側の経路を下りきってから右端側の経路を下りていた。
- 各経路は前のノードを読まないと次のノードが決まらない読み出しの連鎖で、2 つの連鎖が重ならない。
- 通常版では prod が 1 回あたり get の約 2.3 倍の時間 (約 1290 サイクル) かかっていた。

### 変更点

- 3 つのクラスの inner_prod で、分かれたあとの左端側と右端側の経路を同じループで 1 段ずつ交互に下るようにした。2 つの経路の読み出しが重なる。
- 初期値付き版は、経路の途中と分かれ目の初期値の隙間を、元の prefix_prod と suffix_prod と同じ規則で結合する。
- 分かれるまでの共通の経路は、子の選択を条件付き転送で書ける形にした。短い区間では共通の経路が長く、この部分が効く。
- inner_prod に `[[gnu::noinline]]` を付けた。呼び出し元のループに展開されると、操作別の測定で 5% ほど遅くなった。
- 使われなくなった prefix_prod と suffix_prod を削除した。

### 結果

操作別 (ns/op):

| 種類 | 操作 | 前 | 後 | 短縮 |
|---|---|---:|---:|---:|
| 通常版 | prod | 287.1 | 227.2 | 20.9% |
| 通常版 | prod_short | 153.5 | 134.0 | 12.7% |
| 通常版 | update | 275.6 | 274.0 | 0.6% |
| 初期値付き版 | prod | 299.6 | 233.8 | 22.0% |
| 初期値付き版 | prod_short | 155.8 | 135.8 | 12.9% |
| 初期値付き版 | update | 290.6 | 292.3 | -0.6% |
| 遅延版 | prod | 823.9 | 645.3 | 21.7% |
| 遅延版 | prod_short | 413.8 | 421.2 | -1.8% |

既存のベンチマーク (ms):

| ベンチマーク | 条件 | 前 | 後 | 短縮 |
|---|---|---:|---:|---:|
| 通常版 | wide | 129.4 | 117.6 | 9.1% |
| 通常版 | short | 101.2 | 95.5 | 5.7% |
| 通常版 | local | 128.4 | 117.5 | 8.5% |
| 初期値付き版 | wide | 133.2 | 122.2 | 8.3% |
| 初期値付き版 | short | 103.2 | 98.4 | 4.6% |
| 初期値付き版 | local | 134.8 | 123.5 | 8.3% |
| gcd | wide | 150.4 | 140.6 | 6.5% |
| affine | wide | 189.0 | 153.8 | 18.6% |
| max_sub_sum | wide | 224.6 | 213.2 | 5.1% |
| 遅延版 mixed | wide | 465.9 | 443.0 | 4.9% |
| 遅延版 mixed | short | 216.0 | 216.4 | -0.2% |
| 遅延版 mixed | local | 427.5 | 410.7 | 3.9% |
| 遅延版 search | wide | 454.0 | 449.2 | 1.1% |

- 既存のベンチマークは半分が更新なので、短縮は prod だけの場合より小さい。
- 遅延版の mixed は apply が大半を占める。apply は 1 回あたり prod の約 2.4 倍かかり、今回は変えていない。
- 遅延版の prod_short は 3 回の比較で 0.2〜1.8% 遅く、測定の揺れと同程度である。

### 採らなかった案

- 下降を分岐なしで書く案。整数とアドレスをマスクで選び、通常版の prod と set の分岐予測の失敗を 1 回あたり 17 回と 11 回から 3 回と 1 回に減らした。しかし set は約 20% 遅く、prod_short も約 10% 遅くなった。分岐予測が当たれば次のノードの読み出しを比較の前に始められるが、分岐をなくすと比較の結果を待つためと考えられる。GCC 12 は三項演算子で書いた選択を分岐に戻すので、マスクで書く必要があった。
- set を、経路を記録して下りたあとに下から集約値を直す反復の形にする案。分岐ありで 10〜17% 遅かった。既存の記録 [dynamic_segment_tree_iterative_candidates.csv](dynamic_segment_tree_iterative_candidates.csv) と同じ傾向である。
- 遅延版で、遅延作用の有無で分岐せずに恒等作用を合成する案。prod と get の差は ±3% の範囲で、効果がなかった。

## 境界探索 (max_right, min_left)

### 原因

- 通常版と初期値付き版の max_right と min_left は、左の子から順に調べる再帰で書かれていた。
- max_right で l へ下りる途中に右へ進む段でも、左の子を読んで全体が l より前にあることを確かめていた。この読み出しは下りる経路の外にある。
- 初期値付き版は、子の間の初期値の区間を調べる関数も段ごとに呼んでいた。

### 変更点

- 遅延版と同じく、境界まで下りながら残りの部分木を積み、境界に近い順に判定し、失敗した部分木へ下りる反復の形にした。
- 右へ進む段では左の子を読まない。
- 初期値付き版は、子の間の初期値の区間も部分木と並べて積む。min_left で左の子へ下りるとき、左の子の末尾から r までの初期値は左の子より先に調べる必要があるので、別に持って最初に調べる。
- 遅延版は以前から反復の形なので変えていない。

### 結果

操作別 (ns/op、prod の変更後との比較):

| 種類 | 操作 | 前 | 後 | 短縮 |
|---|---|---:|---:|---:|
| 通常版 | max_right | 422.9 | 378.3 | 10.5% |
| 通常版 | min_left | 400.2 | 380.2 | 5.0% |
| 初期値付き版 | max_right | 515.7 | 425.0 | 17.6% |
| 初期値付き版 | min_left | 491.3 | 412.5 | 16.0% |

既存の [dynamic_segment_tree_search_benchmark.cpp](dynamic_segment_tree_search_benchmark.cpp) (ms、HEAD との比較):

| 種類 | 条件 | 前 | 後 | 短縮 |
|---|---|---:|---:|---:|
| 通常版 | wide | 186.2 | 163.2 | 12.3% |
| 通常版 | short | 166.4 | 145.5 | 12.6% |
| 通常版 | local | 183.3 | 158.2 | 13.7% |
| 初期値付き版 | wide | 210.8 | 176.3 | 16.3% |
| 初期値付き版 | short | 188.2 | 157.3 | 16.4% |
| 初期値付き版 | local | 140.9 | 123.8 | 12.1% |

- 通常版の min_left の短縮が小さいのは、元の再帰でも r へ下りる途中に余分な読み出しがなかったためである。

### 採らなかった案

- 失敗した部分木を下りるときに反対側の子を先読みする案と、積んだ部分木を積むときに先読みする案。通常版でも遅延版でも差は 1% 未満だった。

## 検証

- 既存の dynamic_segment_tree_random、dynamic_segment_tree_generic_test、dynamic_segment_tree_util_test、dynamic_segment_tree_util_extra_test、dynamic_lazy_segment_tree_util_extra_test を `-O2`、`-O3 -march=native`、AddressSanitizer と UndefinedBehaviorSanitizer 付きの 3 構成で実行し、すべて通った。警告は出ない。
- prod に次の誤りを 1 つずつ入れると dynamic_segment_tree_random が失敗することを確かめた。初期値付き版で分かれ目の隙間を落とす、初期値付き版で左側の経路の隙間を落とす、通常版で右側の経路の左の子を落とす、遅延版で左側の葉の長さを誤る、遅延版で右側の経路の遅延作用を落とす。
- 境界探索に 8 通りの誤りを入れ、6 通りで dynamic_segment_tree_random と dynamic_segment_tree_generic_test が失敗した。初期値付き版の先頭の初期値を落とす、積む順序を逆にする、下りるときの子の間の初期値を落とす (2 方向)、積む初期値を落とす、通常版の min_left の答えを 1 ずらす。残り 2 通りは境界と分岐の座標が等しいときの扱いを変えるもので、部分木を一段細かく分けて調べるだけなので結果は変わらない。
