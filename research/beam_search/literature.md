# 高速な汎用ビームサーチに関する一次資料調査

## 目的と範囲

この文書は、特定のテストケースではなく、C++ の汎用ビームサーチライブラリを速くするための外部調査を
まとめる。主な対象は次の実装である。

- `beam_search.cpp`: 1 個の可変 `State` を差分更新し、連続した tour で生存木を表す実装
- `beam_search_compose.cpp`, `beam_search_radix.cpp`: Action 合成や明示木を使う実装
- `beam_search_turn.cpp`: 展開ターンが葉ごとに異なる実装
- `candidates.cpp`, `hash_dict.cpp`: 上位 `W` 件の選択と重複排除

NLP のデコーダを対象にした研究も調べたが、任意の `State` / `Action` へ移植できないものは明確に区別する。
外部資料の調査日は 2026-09-01 である。

現行コードだけから得た性能仮説と測定計画は、[performance_audit.md](./performance_audit.md) に分離している。

## 証拠のラベル

- **[測定]**: リンク先の論文または著者実装が実時間、メモリ、展開数、解品質を測定している。
- **[著者経験]**: 実装者による解説やコンテスト提出に基づく。再現条件が限定される可能性がある。
- **[理論]**: 論文で証明された成立条件、計算量、正しさに関する性質である。
- **[コード確認]**: 現行ライブラリのコードから直接確認できる事実である。
- **[適用推論]**: 外部資料と現行コードから導いた提案であり、このライブラリでは未測定である。

同じ項目に複数のラベルが付くことがある。別分野の測定結果を、そのままこのライブラリの速度向上率とは
みなさない。

## 結論

外部資料と現行構造を合わせると、優先して比較する価値が高いのは次の項目である。

1. 現世代の重複表を生存候補数に近い大きさへ保ち、可変ターン版の一時候補・Action と target pool も
   最終生存量または実 occupancy に比例させる。
2. 現行の逐次 exact cutoff を既定に残し、`2W` バッファ、整数 histogram、重複排除なしを選択器 policy として
   比較する。既に拡散済みの Zobrist hash には、明示的な identity hasher を選べるようにする。
3. tour 方式を基準として残しつつ、明示 linked tree、一本道で root へ戻らない open traversal、Action の
   合成を比較する。通常版を単純に「葉と LCA」に置換する優先度は低い。
4. 並列化は、単一 `State` の tour を細かくロックするのではなく、worker ごとの `State` と候補バッファを
   持つ別 backend にする。最初に試す並列化は独立 multi-start が最も単純である。
5. best-first、anytime、多様化は、定数倍改善ではなく探索結果を変える別 policy として提供する。
6. GPU top-k は、候補評価そのものを GPU で batch 実行できる場合だけ有力である。CPU で候補を生成して
   top-k だけ GPU へ送る設計は優先しない。

## 1. 差分更新木と Euler tour

### 1.1 Rafbill の Euler-tour beam search

Rafael Bocquet の公開実装は、各候補へ `State` 全体を持たせる方式、履歴を持つ方式、Euler tour 方式、
圧縮 Euler tour 方式を同じ課題で比較している。

- [著者 README と測定表](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search/-/raw/main/README.md)
- [Euler tour 実装](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search/-/raw/main/03_euler_tour.hpp)
- [公開 repository](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search)
- [histogram 選択を使う実装](https://gitlab.com/rafaelbocquet-cpcontests/euler-tour-beam-search/-/raw/main/01_histogram.hpp)

README にある `N=16`, `W=100000` の代表値は次のとおりである。

| 実装 | 実時間 | 最大メモリ |
|---|---:|---:|
| State copy の基準実装 | 5.869 s | 495.442 MB |
| Euler tour | 1.243 s | 6.930 MB |
| 圧縮 Euler tour | 1.705 s | 874.230 KB |

**[測定]** 大きな `State` と大きな幅では、誘導部分木を 1 個の可変 `State` で巡回する方法が、全状態を
保持する方法より大幅に少ないメモリで高速だった。一方、小さい問題や小さい幅では Euler 方式が常に最速とは
限らない。圧縮版はメモリをさらに減らす代わりに通常 Euler より遅い。

**[コード確認]** `beam_search.cpp` は同じ系統の設計であり、前世代の葉の間を差分 Action で移動して
次世代 tour を作る。この資料は全面的な方式変更より、現行方式を基準実装として残す根拠になる。

**[適用推論]** 圧縮 tour はメモリ上限が厳しい環境向けの別 backend にはなるが、Action が既に小さい通常の
C++ 実装では、bit packing と復号の費用が増えるため既定値には向かない。

AHC021 の公式解説にも同じ著者の実装説明がある。

- [AHC021 公式解説](https://atcoder.jp/contests/ahc021/editorial/6681?lang=ja)
- [該当提出](https://atcoder.jp/contests/ahc021/submissions/42958119)

**[著者経験]** 著者はこの問題で Euler tour によりおよそ 2--5 倍の高速化を見込み、幅 6000 の 1 回より、
乱数の異なる幅 2000 の 3 回がわずかに良かったと報告している。後者は multi-start の根拠の一つだが、
問題固有の観測である。

### 1.2 rhoo の linked tree 実装

競技プログラミング向けの著者解説は、固定 pool の双方向 linked tree、差分更新、候補選択、hash、幅調整、
探索の多様化まで扱っている。

- [木で差分更新するビームサーチ](https://qiita.com/rhoo/items/f2be256cde5ad2e62dde)
- [高速なビームサーチの実装と探索戦略](https://qiita.com/rhoo/items/2f647e32f6ff2c6ee056)

**[測定]** 前者の例では、通常実装の平均 3249 ms に対して木の差分更新が 2714 ms で、およそ 1.2 倍
だった。後者には小さな `State` で通常 copy が 776 ms、差分木が 948 ms となった反例もある。

**[著者経験]** 著者は Euler tour と linked tree の時間・空間は概ね同程度であり、枝の skip や並べ替えは
linked tree の方が柔軟だとしている。また、枝が一本の区間では `State` を root へ戻さず、そのまま先へ進む
最適化を重視している。

**[適用推論]** `beam_search_radix.cpp` はこの比較を行う実験台に近い。特に次を測定する価値がある。

- DFS の最後に必ず root へ rollback せず、現在位置を次の走査開始点にする open traversal
- 単一子 path の `Action::compose()` による縮約
- 木メタデータと大きな `Action` を別配列にする SoA / arena
- ノード数が 65535 以下と保証できる構成で 16 bit index を使う特殊化

open traversal は `apply_op()` / `rollback()` を減らせる一方、検索終了時に `State` がどの頂点にいるかを
追跡し、木の削除・縮約と整合させる必要がある。

### 1.3 帰りがけ順だけを持つ tour

2026年の実装者解説は、固定深さの木上ビームサーチでは完全な Euler tour を保持せず、帰りがけ順を調整した
`tour`、葉境界 `leaf`、現在経路 `trace`、候補 `cand` だけで走査できることを示している。

- [木上のビームサーチ：高速化編](https://trap.jp/post/2920/)

`leaf[i+1] - leaf[i]` が隣接葉の LCA までの距離に対応し、候補を葉順の逆向きに処理しながら次の `tour` を作る。
**[著者経験]** AHC063 の提出で、著者は従来の木上実装から約20%速くなったケースを報告している。問題固有の値であり、
このライブラリへ速度比を外挿しない。

**[コード確認]** 現行 `beam_search.cpp` の `tour`、`leaf`、`trace`、`cand` と逆向き走査は、既にこの方式と
同じ構造である。したがって、これは新しい置換案ではなく現行標準版を維持する根拠である。次に減らせるのは
完全 Euler tour ではなく、ActionId の幅、次世代 `tour` のコピー、候補ソートなどである。

### 1.4 State copy と差分木は両方必要

上の二つの著者資料は、差分木が常に速いわけではないことも示す。`State` が小さく trivially copyable で、
`apply_op()` / `rollback()` が重い場合、連続した State 配列の copy が勝ち得る。

**[適用推論]** 汎用ライブラリでは、問題の型だけから一意に決めず、少なくとも次の backend を比較可能にする。

- State copy + 連続配列
- 1 State + Euler tour
- 1 State + linked tree / composed path

自動選択を行う場合も、数ターンの warm-up で実測してから選ぶ方が、`sizeof(State)` だけの判定より安全である。

## 2. 上位 `W` 件の選択

### 2.1 online exact cutoff と `2W` バッファ

`Candidates` は、上位 `W` 件に入った候補を即時に保持し、segment tree で現在の最悪値を正確に返す。
候補が一時的に採用されるたびに `O(log W)` の更新が必要だが、`State::try_op(threshold, ...)` へ厳しい
cutoff を渡せる利点がある。**[コード確認]**

rhoo の解説は、候補が `2W` 件へ達するたびに下半分を `nth_element` 相当で捨てる方法を挙げている。
また、全候補数を `C` とすると、`W` が `C` より十分小さい場合は `partial_sort`、そうでなければ
`select_nth_unstable` を使い分けている。**[著者経験]**

**[適用推論]** 次の方式なら全候補 `C` を保存せず、最大約 `2W` 件で最終的な exact top-`W` を求められる。

1. 最初の `W` 件で `(score, tie_key)` 全体の rank cutoff を作る。
2. rank cutoff より良い候補をバッファへ追加する。
3. `2W` 件になったら、全順序 `(score, tie_key)` で上位 `W` 件へ圧縮する。
4. 圧縮間は直前の rank cutoff を使い、終了時にもう一度圧縮する。

最小化問題では真の `W` 番目の rank は探索中に悪化しないため、古い cutoff は真の cutoff より緩い。
したがって余計な候補を評価することはあっても、候補を誤って落とさない。最終圧縮は exact にできる。
一時的な `Action` メモリは約 `2W`、選択の期待計算量は一時採用数に対して線形になる。
現行のように scalar score しか State へ渡さない場合、同点はtieで改善し得るため完全評価し、
`score > cutoff.score` を証明できた場合だけ早期 reject する。

不利な点は次のとおりである。

- cutoff が古いため、`try_op()` の枝刈りが弱くなる。
- 重複 key の更新と圧縮時の index 移動を扱う必要がある。
- 同点順序を現在と同じにしたい場合、明示的な tie key が必要である。
- `record_history` 用の一時 node を、落選時に回収できなければならない。

従って一律置換ではなく、`CandidateSelector` の別 policy とするのがよい。候補管理が支配的なら buffered、
候補生成が重く cutoff が強く効くなら online exact が有利と予想される。

### 2.2 histogram / bucket 選択

Rafbill の histogram 実装と、後述する Frohner らの並列 Beam は、score が狭い整数範囲に入る場合に
histogram から cutoff bin を求める。

**[測定]** Rafbill の比較では histogram 方式が State copy の基準実装より速い条件がある。Frohner らは
thread-local histogram を併合し、累積個数で cutoff bin を見つけ、境界 bin だけ partial quickselect する。

**[適用推論]** `ScoreTraits` が整数の下限・上限を与えられる場合、計算量を
`O(C + score_range + boundary_bin)` に近づけられる。次の場合には向かない。

- score range が `C` より大きい。
- 浮動小数や大きな 64 bit 値で、分布に合う動的 bin が作れない。
- 同点が cutoff bin に集中し、そこで大きな追加選択が必要になる。

### 2.3 SIMD select

NumPy が利用する公式 C++ ライブラリは、AVX2 / AVX-512 の quickselect、key-value select、argselect を
公開している。

- [NumPy x86-simd-sort](https://github.com/numpy/x86-simd-sort)

**[測定]** リポジトリは組込み型や object sort の速度を測定しているが、このビームサーチの候補構造での
測定ではない。

**[適用推論]** candidate を score と index の連続配列にした buffered backend なら利用可能性がある。
ただし C++17 依存、ISA dispatch、外部コード量が増える。まず `std::nth_element` と小さな descriptor で
支配的な改善が出ることを確認し、その後の optional backend とする。

## 3. 重複排除と hash table

### 3.1 重複排除は常に得ではない

Libralesso らの Sequential Ordering Problem 向け tree search は、prefix equivalence による重複排除と、
複数の lower bound の費用対効果を測定している。

- [Tree Search for the Sequential Ordering Problem](https://arxiv.org/abs/1911.12427)

**[測定]** prefix equivalence は開いた node 数を平均でおよそ 4--5 分の 1 にした一方、1 秒当たりの node 数も
およそ 4--5 分の 1になった。重複密度が低い問題では不利だった。より強い MST bound が開く node を
1000--10000 分の 1にしても、その評価費用のため、時間内の解品質が悪い例もある。

**[適用推論]** 現行 `Candidates` は常に hash lookup を行うため、次の policy が必要である。

- `NoDedup`: score の top-`W` だけを選ぶ。
- `HashDedup`: 現在と同じ 64 bit fingerprint の一致を同一視する。
- `DomainDedup`: ユーザーが equivalence key や、同じ key 内の dominance 比較を与える。

評価では「何件 prune したか」だけでなく、`try_op/s`、候補処理 cycle、最終品質、hash table の peak capacity
を同時に測る。

### 3.2 生存候補と過去訪問状態を分ける

**[コード確認]** 現行 `Candidates::push()` は追い出した hash の値を `-1` にするが、key 自体を削除しない。
従って、同一ターンに一時採用された異なる key の数に応じて表が増え得る。`clear_hash_every_turn=false` の
場合には、前ターンの生存 hash を `-2` として予約する別の意味も同じ表へ載せている。

**[適用推論]** 次を別構造にすると契約と容量が明確になる。

- 現在の selector 内の重複排除: live な `W` または `2W` 件だけを指す、削除可能な表
- ターンをまたぐ closed set: 世代 window、容量上限、置換方針を明示した表

ある cutoff で top-`W` から追い出された key は、後から同じ key のより良い候補が来れば新規候補として
再評価すればよい。selector 用表では追い出した key を実際に erase し、tombstone が増えたら live key から
再構築できる。closed set を使う場合は、この erase とは別の意味になる。

可変ターン版の global seen は、ローカル候補へ一度採用した時点で key を登録し、その候補が後で追い出されても
保持する。現行の意味論を完全に保つ closed set は一時採用 key 数に比例するため、最終 survivor だけからの
再構築へ変えてはいけない。容量を制限する場合は、window または置換方針による探索結果の変化を明示する。

### 3.3 Swiss table と pre-hashed key

Abseil の公式設計資料は、1 byte metadata、7 bit の H2 fingerprint、16 slot の SIMD probe、flat storage を
説明している。

- [Abseil Swiss Tables design](https://abseil.io/about/design/swisstables)
- [Swiss Tables announcement](https://abseil.io/blog/20180927-swisstables)

**[コード確認]** 現行 `HashDict` も 16 個の metadata を SIMD で調べる近い設計である。外部 table への
全面置換より、erase、世代管理、容量上限を先に検討する価値が高い。事前確保の不具合は HashDict 本体でなく、
Candidates 側の `inner_len() == 1` という成立しない条件にある。

rhoo は、Zobrist hash が既に一様なら標準 hasher で再度混ぜず no-op hasher を使う案を説明している。
**[著者経験]** 一方、短い 16 bit hash の直接表では衝突により品質へ影響した経験も記している。

**[適用推論]** hasher は次の明示 policy に分ける。

- `MixedHash`: 任意のユーザー key を受ける安全な既定値
- `IdentityPrehashed`: 全 bit が十分拡散された hash であるというユーザー契約

identity を自動判定してはならない。偏った key や攻撃的入力では probe が集中する。

### 3.4 bounded transposition table の注意点

Akagi、Kishimoto、Fukunaga は、IDA* へ transposition table を組み合わせたとき、素朴な実装と任意の
replacement policy が最適性や完全性を壊し得ることを示している。

- [On Transposition Tables for Single-Agent Search and Planning](https://doi.org/10.1609/socs.v1i1.18164)

**[理論]** 同じ state へ別 path から到達したとき、table に保存する cost と iteration 情報、容量超過時の
置換規則が search の正しさへ影響する。論文は IDA* 向けであり、元から近似的な Beam の解品質保証を直接
与えるものではない。

**[適用推論]** `clear_hash_every_turn=false` の closed set を容量制限する場合、単なる「hash が存在したら
常に拒否」ではなく、少なくとも次をユーザー契約にする。

- 同一 key のどの path を残すかを決める dominance / cost 比較
- 世代または turn window
- 容量超過時の置換が探索結果を変えること
- 64 bit fingerprint 衝突を同一 state とみなすか、必要なら実 state で検証するか

## 4. LCA、virtual tree、tour 走査

virtual tree の標準的な性質として、必要頂点を Euler 順に並べ、隣接頂点の LCA を追加すれば、必要な
圧縮木を作れる。

- [AtCoder 公式解説 PDF の virtual tree 解説](https://img.atcoder.jp/tkppc3/editorial.pdf)

**[コード確認]** 固定深さの `beam_search.cpp` は `parent_leaf` 順に候補を処理し、`leaf` の境界をほぼ
一方向に走査する。隣接する採用親の間で調べる区間は重なり続けないため、LCA 境界 index の走査は1世代で
`O(L)` である。これとは別に、経路復元と次 tour の構築は `O(T_old + T_new)`、状態操作は `O(X)` を要する。
全葉の各組に対して Euler tour 全体を走査しているわけではない。

**[適用推論]** 固定深さ版を leaf ID + binary lifting LCA に置換しても漸近改善はない。むしろ次が増える。

- 深さごとの ancestor table、または RMQ 用配列
- 親ポインタのランダムアクセス
- LCA ごとの `O(log D)`、または大きな `O(1)` RMQ 構造

また、隣接する生存状態間を移動するには、結局その誘導部分木の辺で `apply_op()` / `rollback()` が必要である。
Euler walk はこの辺走査回数について本質的に小さい。

ただし `beam_search_turn.cpp` で総葉数 `L` に対して展開対象 `K` が小さく、平坦木 token 数 `Q` に対する
実走査・再構築が大きい場合は別である。対象葉だけを turn bucket から取り出し、parent tree 上の virtual tree を
作る backend は比較対象になる。これは通常版の改善ではなく、疎な可変ターン展開向けの仮説である。

### 4.1 単調 parent 写像の圧縮と依存ロード

固定深さの frontier を DFS 順に置くと、子から前世代の親 ordinal への写像は単調になる。親 `i` の子数を
`c_i` とした `1^c_i 0` の連結は、LOUDS と同じ unary degree sequence であり、親と子の幅が同程度なら
およそ node 当たり2 bitで表せる。

- [LOUDS を含む succinct ordinal tree の論文](https://www.imsc.res.in/~vraman/pub/algorithmica_05.pdf)
- [Elias--Fano を用いる quasi-succinct monotone sequence](https://arxiv.org/abs/1206.4300)

**[理論]** LOUDS は順序木を `2n+o(n)` bitで表し、rank/select補助構造により親や子の navigation を支える。
Elias--Fano 系は単調整数列を universe と要素数に応じた空間で表し、random access と sequential scan の両方を
提供できる。現行 frontier の parent map は一般の動的木ではなく、世代ごとの immutable な単調列なので、この二つの
表現を直接比較できる。

**[適用推論]** raw bit 数が小さくても、各深さの parent decode は直前の結果を次の index に使う依存列である。
direct `uint32_t` 配列が cache に収まる場合は、rank/select の命令数が純増する可能性が高い。反対に深い大幅 beam で
direct parent が LLC を圧迫する場合は、圧縮により miss を減らせる可能性がある。世代ごとに direct 16/32 bit、unary、
Elias--Fano を選ぶ比較が必要であり、byte 最小だけで既定形式を決めない。

異なる target leaf の親鎖は互いに独立なので、数本を round-robin に1段ずつ進めれば memory-level parallelism を
作れる。これは pointer chain を複数同時に進める AMAC の考え方に近い。

- [Asynchronous Memory Access Chaining](https://www.vldb.org/pvldb/vol9/p252-kocberber.pdf)

**[測定]** AMAC 論文は database の不規則な hash-chain lookup で、lookup ごとの状態を分離して複数の依存列を
重ねる方法を評価している。ビームサーチでの測定ではないため、報告された速度比は転用しない。

**[適用推論]** parent backend では次の数葉の ancestry metadata だけを先読みし、State の rollback、apply、
enumerate 順は元の逐次順に保てる。ただし scratch read/write が増え、兄弟中心で suffix が1辺なら重ねる miss 自体がない。
window 幅1、4、8、16を比較し、LLC miss が支配するときだけ有効化する。

## 5. CPU 並列化

### 5.1 固定配列と並列 histogram

Frohner らは、Permutation Flow Shop Scheduling と Travelling Thief Problem を対象に、data-parallel な
汎用 Beam を Julia で実装した。

- [論文 DOI](https://doi.org/10.1145/3547276.3548633)
- [書誌ページ](https://repositum.tuwien.at/handle/20.500.12708/193924)
- [公式リポジトリ](https://github.com/nfrohner/parbeam)

実装上の要点は次のとおりである。

- current、successor、next beam を固定長で事前確保する。
- 親 beam を worker へ分割し、worker 固有領域へ successor descriptor を書く。
- score histogram を並列に作り、累積 cutoff と境界 bin の選択だけを後で行う。
- full state の copy / transition は、選択された successor にだけ行う。
- histogram counter の false sharing を cache-line padding で避ける。
- 共有 lock heap ではなく、最大分岐数分の疎な配列を確保する。

**[測定]** 大きな幅では 46-core AMD EPYC 上で 30--42 倍、並列効率 60--90% を報告している。非常に
大きな幅ほど有利であり、小さい幅に同じ倍率を期待できない。重複排除を追加した実験では、問題によって
1.2--3.5% の候補を除き、実行時間は 0.2--0.4 秒増えた。解品質は改善する場合があった。

### 5.2 shared table と hash-distributed Beam

Kuroiwa と Beck は、domain-independent dynamic programming の complete anytime beam search を
multi-thread 化し、6 種類の組合せ最適化問題で比較した。

- [AAAI 2024 論文 PDF](https://ojs.aaai.org/index.php/AAAI/article/download/30062/31869)
- [公式実装 release](https://github.com/domain-independent-dp/didp-rs/releases/tag/parallel-aaai24)

比較した主な方式は次の二つである。

- Shared Beam Search: shard ごとに lock した concurrent hash table を共有する。
- Hash Distributed Beam Search: state hash で所有 worker を一意に決め、その worker の local table で
  dominance と重複を処理する。

**[測定]** 32 thread で逐次版に対して平均 9--39 倍の speedup を報告し、hash-distributed 方式が shared
方式より速かった。問題によっては探索順序の変化も品質へ影響した。論文は頻繁な worker 間通信を bottleneck
として挙げ、親子を同じ worker へ寄せる abstracted hash を将来案としている。

hash-distributed 方式は worker ごとに `W / thread_count` 件を残すため、全体で厳密な top-`W` を選ぶ逐次版と
同じ beam 集合にはならない。この差が探索の多様化として有利な場合も、不利な場合もある。

### 5.3 現行 C++ ライブラリへの適用

**[コード確認]** 現行の標準版は 1 個の可変 `State` を tour 順に更新するため、そのまま複数 worker から
操作できない。

**[適用推論]** exact な世代同期 backend は次の構造が考えられる。

1. 各 worker が独立した `State` または checkpoint を持つ。
2. 親葉を worker に分けて候補 descriptor を生成する。
3. 各 worker 内で重複を統合し、異なる key の local top-`W` だけを残す。
4. 最大 `thread_count * W` 件の union を全体で再び重複統合し、global top-`W` を選ぶ。
5. 選ばれた `(parent, Action)` だけで次世代木を作る。

この推論では、同一 key の reducer を、全順序 `rank=(score, tie_key)` による selective な `min_rank` とする。
一般化する場合も、結合・交換可能で、出力の rank が両入力より悪化せず、選んだ入力の payload を保持する必要が
ある。単に順序非依存な reducer というだけでは、global 代表が local 代表より悪化し、次の包含関係が壊れ得る。

上の reducer で worker 内を先に重複統合すれば、global top-`W` は各 partition の local top-`W` の union に
含まれる。反対に、raw stream を重複込みで先に `W` 件へ切ると、重複に押し出された候補が必要になり、exact
ではなくなる。逐次版と同じ tie を保つなら、親と Action の列挙順から一意な serial key も必要である。

cutoff も同じ全順序で比較する必要がある。scalar score しか渡せない API では `score > cutoff.score` の拒否は
安全だが、`score >= cutoff.score` は、後続の同点候補が tie で勝ち得るなら安全でない。後者を使うには、今後の
同点候補が cutoff の tie に勝たない契約、または `(score, tie_key)` 全体を渡す cutoff が必要である。

finished 候補は通常候補の幅 cutoff と別に集約する。`try_op()` は terminal 判定前に通常 cutoff だけで候補を
捨ててはならず、terminal にも使える安全な別 bound がない限り、finished 候補を worker ごとに保持して統合する。
古い cutoff snapshot は全順序上で緩い値だけを許し、cutoff の値や更新時刻が列挙、hash、RNG を変えてはならない。

メモリは `thread_count * sizeof(State)` と local buffer 分増える。候補評価が軽い、小さい `W`、分岐数が
小さい条件では barrier と merge が負ける。API も逐次 `enumerate_actions(submit)` だけでなく、次のどちらかが
必要になる。

- `State` を安価に clone / checkpoint できる契約
- ユーザーが複数状態をまとめて評価する `evaluate_batch` 契約

exact backend では、各 worker が同じ canonical な世代 root から始まり、clone の mutable data を共有しないこと、
`apply_op()` と `rollback()` が逆操作であることも必要になる。候補集合、列挙順、score、hash は worker 数や実行順に
依存させない。乱数は global serial key などの意味的な識別子から決め、worker-local RNG を処理順に進めない。

最初から lock-free な単一木を作るより、別 backend として導入した方が逐次 hot path を損なわない。

### 5.4 独立 multi-start

Frohner らは Gaussian noise を加えた独立 run を MPI worker へ割り当て、複数 run から良い解を選んでいる。
AHC021 の著者解説でも、幅を一つへ集中するより複数の狭い Beam がわずかに良かった例がある。
rhoo の解説も独立試行と段階的な絞り込みを推奨している。

**[測定]** Frohner らの TTP 実験では、32 独立 run / 8 worker の構成が 22 instance 中 11 件で新しい
best-known solution を得ている。これは並列効率だけでなく探索品質の結果である。

この測定と AHC021 の観測は、複数 run から得る解品質を評価したもので、1 run の latency 短縮を示さない。
独立 run の同時実行は、同数を逐次実行する場合の wall-clock time を縮め得るが、総仕事量と peak memory は増える。

**[適用推論]** library wrapper としては次が実装しやすい。

- seed と時間配分を明示した複数 `BeamSearch` 実行
- 最初は全 run へ短い予算を与え、途中評価の悪い半分を止める successive halving
- 1 run を大きな幅にする方式との切替

winner だけを後から replay して `final_state` を作る最適化には、各 run の比較に final state が不要であること、
canonical initial state を再構築できる factory、保存済みの seed と設定、決定的な `apply_op()`、完全な Action 列が
要る。初期化が消費済み入力や外部状態に依存する場合は使えず、各 run の final state または checkpoint を保持する。

successive halving 自体の一次資料は次である。

- [Successive Halving 論文](https://proceedings.mlr.press/v51/jamieson16.html)

**[注意]** 論文の測定対象は best-arm / hyperparameter optimization であり、Beam run への転用効果は
**[適用推論]** である。途中 score が最終品質を十分予測できない問題では、良い seed を早く捨て得る。

### 5.5 時間予算に合わせた動的幅

rhoo の解説は、固定 turn 数で残り時間を使い切るため、`n` turn ごとに次の幅へ更新する例を示している。

`M_next = clamp(M * sqrt(n * remaining_time / (remaining_turns * elapsed_n_turns)), M_min, M_max)`

平方根と clamp は幅の急変を抑えるために使われる。**[著者経験]** 該当提出は動的調整を使っているが、
固定幅との差だけを分離した測定ではない。

**[コード確認]** 現行 `BeamParam` にも時間から幅を調整する機能がある。

**[適用推論]** 改善余地は式を一つに固定することではなく、selector や State の速度が turn によって変わる
場合に備え、実測 throughput の指数移動平均、更新間隔、最小・最大幅を policy として公開することである。
幅変更は解品質も変えるため、時間超過率と score 分布の両方で評価する。

## 6. GPU と batched top-k

RadiK は GPU 上の radix top-k を大きな `k` と batch に対応させた研究である。

- [RadiK 論文](https://doi.org/10.1145/3650200.3656596)
- [preprint](https://arxiv.org/abs/2501.14336)
- [公式リポジトリ](https://github.com/leefige/radik)

**[測定]** GPU 上に既にある入力に対して、従来法より non-batch で最大 2.5 倍、batch で最大 4.8 倍を
報告している。

NVIDIA CUB にも batched top-k の公式 primitive がある。

- [CUB DeviceBatchedTopK](https://nvidia.github.io/cccl/unstable/cub/api/structcub_1_1DeviceBatchedTopK.html)
- [top-k の決定性と要件](https://nvidia.github.io/cccl/unstable/cub/device_topk_requirements.html)

CUB は複数 segment の unordered top-k を扱い、同点時の決定性や temporary storage に明示的な契約がある。

**[適用推論]** 任意の C++ `State` を CPU で `apply_op()` / `rollback()` し、score 配列だけ GPU へ転送して
top-k を求める構成は、PCIe 転送と kernel launch の固定費を回収しにくい。GPU backend は次を満たす場合に
限って有力である。

- state と Action が SoA として device 上に存在する。
- 候補生成と score 計算を batch kernel にできる。
- `W * B` が十分大きい。
- tie と乱数の決定性変更を許容、または明示的に実装できる。

chunk ごとの summary から exact に統合する場合も、CPU 並列版と同じ `min_rank` reducer、global tie key、
cross-chunk dedup、finished 候補の別集約を使う。score だけの cutoff で同点を捨てる実装は exact ではない。
device と CPU で浮動小数評価が異なる場合は、CPU 逐次版との一致ではなく、別の数値 policy として扱う。

従って core の `Candidates` を GPU top-k に置き換えるのではなく、`BatchState` / accelerator backend を
別に設計する。

## 7. 候補を生成しない最適化

top-k のデータ構造を速くしても、分岐数が大きく score 評価が高価なら、落選候補を生成しない方が大きい。

Enhanced Partial Expansion A* は、domain knowledge により必要な score 層の子だけを生成する。

- [Partial-Expansion A* with Selective Node Generation](https://ojs.aaai.org/index.php/AAAI/article/view/8137)
- [拡張版の JAIR 論文](https://doi.org/10.1613/jair.4171)

**[測定]** pancake puzzle と multi-agent pathfinding で、余分な node の生成と保持を減らし、改善した条件を
報告している。対象は A* であり Beam そのものではない。

**[適用推論]** 現行の `State::try_op(threshold, submit)` は、この考え方を問題側で実装できる API に近い。
さらに次の optional 契約が考えられる。

- 子を良い下界順に列挙し、残りの全候補が cutoff 以上なら停止する。
- Action 全体を作る前に cheap bound を返す。
- 親ごとに上位 `L` action だけを逐次生成する iterator を返す。

停止が exact になるには「残りの候補が必ず cutoff より悪い」という保証が必要である。単なる近似 score で
先に filter する場合は探索結果が変わる。

Filtered Beam Search は、cheap な local score で filter width まで絞った後、expensive な global score を
計算する古典的な構成である。次の scheduling 論文は、total-cost evaluation の品質は高いが高価であり、
filter / recovering variant との速度・品質を比較している。

- [Beam search algorithms for the early/tardy scheduling problem](https://doi.org/10.1016/S0278-6125(05)80005-6)

**[適用推論]** `cheap_score`, `refine_score`, `filter_width` を policy にすれば高価な score の問題に使えるが、
cheap score が安全な bound でない限り近似 policy である。

## 8. best-first Beam

### 8.1 Best-First Beam Search

Meister らは、深さ優先の世代同期ではなく、score の良い partial hypothesis を優先 queue から展開し、各深さの
展開数だけを `k` に制限する Best-First Beam Search を提案した。

- [Best-First Beam Search](https://aclanthology.org/2020.tacl-1.51.pdf)

**[測定]** neural text generation では、beam size 500 で standard beam + early stopping に対して約 8 倍の
performance increase を報告している。score function 呼出しの削減が支配的だった。

**[理論]** partial path を伸ばすほど score が良くならない単調性があれば、通常 Beam と同じ `k`-optimal
集合を返せる。非単調でも、将来の改善量に安全な上界があれば拡張できる。queue は複数深さを保持するため、
素朴には `O(WD)` メモリである。論文は memory-reduced variant も示す。

**[適用推論]** AHC 型の評価は途中で改善し得るため、汎用既定値にはできない。次の trait がある場合だけ
別 policy として有効である。

- `score_is_monotone`
- `optimistic_bound(state)`
- 可変深さの goal と early termination

全 run が固定の `max_turn` まで必ず進む問題では、早期終了の利点が小さい。現行 radix 版の
`monotone_skip` を一般化する方が小さな変更である。

### 8.2 Memory-Bounded Best-First Beam Search

Gao らの MB2FBS は、毎回 `beta_1` 個の node を一段進め、`beta_2` 個の別深さ node を温存して再考する。

- [A Memory-Bounded Best-First Beam Search](https://ojs.aaai.org/index.php/SOCS/article/download/21754/21518/25797)

**[測定]** Halide の 15 pipeline では、Beam-32 の平均 1674.5 展開・106秒に対し、uncontrolled MB2FBS は
1834.0 展開・106秒、controlled MB2FBS は1552.0 展開・85秒だった。生成 schedule の平均実行時間は
Beam-32 の27821 usに対して二方式とも約21560 usで、cost model 上の品質も改善した。uncontrolled の展開数は
Beam-32 より多いため、「常に展開数も減る」とは結論しない。

**[適用推論]** これは定数倍実装改善ではなく、深さ間で比較可能な priority を必要とする探索 policy である。
controlled 版は最大で深さごと `beta` 件を展開するが、正しく前進するための記憶量が `O(WD)` になり得る。
fixed-horizon の軽量 core へ混ぜず、別 search class とする。

### 8.3 停止条件は性能比較の前に固定する

- [A Call for Clarity in Beam Search](https://aclanthology.org/2024.lrec-main.7/)

Kasai らは、完了列の保持と停止条件が実装間で異なり、結果と実行量の両方へ影響することを NLP decoding で
示している。**[測定]** 対象 domain は異なるが、「早く止まった実装」を同じ探索の高速化として扱わないという
比較上の注意は共通する。

**[コード確認]** 現行 naive 版は完成候補を見つけても未完了候補があれば探索を続ける一方、固定深さの木版は
完成候補を含む世代で返る。**[適用推論]** `stop_on_finished_generation` と `continue_after_finished` を明示 policy
に分け、定数倍の benchmark では同じ停止契約を使う。

## 9. anytime / complete variant

これらは「同じ 1 回の Beam を速くする」方法ではなく、追加時間を解品質向上へ使う方法である。

### Beam-Stack Search

- [Beam-Stack Search: Integrating Backtracking with Beam Search](https://cdn.aaai.org/ICAPS/2005/ICAPS05-010.pdf)

beam boundary を stack に保存して backtrack し、anytime かつ complete にする。divide-and-conquer 版は
メモリを抑える代わりに再展開が増える。**[適用推論]** 固定時間で最初の解だけを得る core の高速化には
ならないが、現在の探索 tree を再利用する長時間 policy として候補になる。

### Limited Discrepancy Beam Search / BULB

- [Limited Discrepancy Beam Search](https://ai.dmi.unibas.ch/research/reading_group/furcy-koenig-ijcai2005.pdf)

Beam の greedy な選択から外れる回数を制限して backtrack し、memory bounded かつ complete にする。
誤った上位選択から回復できるが、同じ領域の再探索が増える。

### Incremental Beam Search

- [Incremental Beam Search](https://www.sciencedirect.com/science/article/pii/S0020019013002391)

幅を広げる反復で既展開 node を再展開せず、幅とともに品質を単調に改善する anytime 方式である。
過去の search frontier を残すため、1 世代だけ保持する現在の API より多いメモリが必要になる。

### Rectangle Search

- [Rectangle Search: An Anytime Beam Search](https://arxiv.org/abs/2312.12554)

深さと幅を段階的に広げ、anytime / complete にする。複数深さの open list と closed list を使うため、
fixed-horizon tour の小さな差し替えではない。

### Monobeam / Bead

- [Beam Search: Faster and Monotonic](https://cdn.aaai.org/ojs/19805/19805-40-23818-1-2-20220613.pdf)

幅 `k+1` の解が幅 `k` より悪化しない slot-wise search と、非一様 edge cost で distance-to-go を用いる方法を
提案する。**[測定]** 標準 Beam が幅を増やすと悪化する benchmark があり、提案法は単調性を保証する。
一方、低い幅では遅い、または標準 Beam より悪い条件もある。duplicate elimination にも beam slot 情報が
必要である。これは幅の自動調整を安全にする別 policy であり、定数倍高速化ではない。

## 10. 多様性と独立探索

### 10.1 親・group ごとの多様性

Diverse Beam Search は beam を group に分け、先の group と異なる候補へ penalty を与える。

- [Diverse Beam Search](https://ojs.aaai.org/index.php/AAAI/article/view/12340)
- [sibling rank による多様化](https://arxiv.org/abs/1611.08562)

**[測定]** 主な測定対象は画像 caption と機械翻訳であり、汎用組合せ最適化ではない。sibling-rank 法は
同じ親の上位子ばかりが beam を占有することを、小さな rank penalty で抑える。

rhoo の AHC 向け解説にも、親・祖先順位を tie break へ入れる、位置ごとの quota を設ける、score へ小さな
乱数を加える方法がある。**[著者経験]**

**[適用推論]** 汎用ライブラリへ入れやすいのは次の opt-in policy である。

- 1 親から残せる子の最大数
- ユーザーの `group_key(candidate)` ごとの quota
- `(score, sibling_rank, ancestor_rank)` の比較
- 同点だけを乱数化する seed 付き comparator

これらは exact top-`W` を変え、品質向上の保証はない。hash dedup は完全に同じ key だけを除くため、近い
候補の集中を避ける diversity とは別機能である。

### 10.2 Stochastic Beam の適用限界

- [Stochastic Beams and the Gumbel-Top-k Trick](https://proceedings.mlr.press/v97/kool19a/kool19a.pdf)

Gumbel-Top-k により、factorized probability model の sequence を非復元抽出する。**[測定]** 確率的 sequence
model では原理のある多様化だが、任意の AHC score が正規化確率を持つとは限らない。一般 `Score` / `Action`
へ直接導入するのではなく、確率 policy を提供するユーザー向けの別 sampler とする。

## 11. 実装候補の優先順位

### P0: 現行コードで `W` から漏れる量を止める

1. selector 用 active hash table を `O(W)` に保ち、cross-turn closed set と分離する。
2. 可変ターン版の一時候補と Action を、採用イベント数でなく最終 dirty slot 数に比例させる。
3. `push_lazy()` 相当を全 backend で使い、採用前の Action copy を避ける。
4. 可変ターン版の target pool を実 occupancy に応じて確保する。

1--2 は外部資料の erase / preallocation と現コードを合わせた **[適用推論]**、3--4 はコード監査に基づく。

### P1: 条件別の selector と実行 backend

1. online exact を既定に残し、buffered `2W`、histogram、no-dedup を policy として比較する。
2. pre-hashed key 用 identity hasher を明示 opt-in にする。
3. State copy / Euler / linked tree と、linked tree の open traversal を同じ合成ベンチで比較する。
4. worker-local State と exact global merge を持つ CPU parallel backend を検討する。
5. seed と時間配分を扱う independent multi-start wrapper を別層にする。

### P2: 探索結果を変え得る、または適用条件が強い policy

1. 親内 good-first、sorted successor、cheap bound / refine score
2. monotone score / optimistic bound がある場合の best-first Beam
3. diversity quota、sibling rank、hash-distributed local quota
4. Beam-Stack、BULB、Incremental Beam、Rectangle Search
5. `K << L` かつ `Q/K` が大きい可変ターン向け leaf + LCA / virtual tree backend
6. device-resident `BatchState` がある場合の GPU backend

## 12. 比較実験で分けるべき軸

外部資料の速度向上率は問題と実装条件に強く依存する。少なくとも次を独立に振る。

- `W`, 分岐数、深さ、1 ターンの一時採用数
- `sizeof(State)`, `sizeof(Action)`, copy / apply / rollback の費用
- score の型、範囲、同点率、分布
- 重複率と hash の分布
- cutoff を問題側の `try_op()` がどの程度利用できるか
- 生存木の共有 prefix、単一子率、可変ターン版の `K/L`、実走査 token / `Q`、`Q/K`
- thread 数、NUMA 配置、worker ごとの State メモリ

比較する指標は wall time だけでなく、次を含める。

- `try_op()` / 秒、採用候補 1 件当たりの cycle
- selector 更新数、圧縮回数、stale cutoff により余計に評価した件数
- hash probe group、erase、rebuild、peak capacity
- `apply_op()` / `rollback()` / State copy / Action copy の回数
- peak RSS、LLC miss、branch miss、false sharing
- 同じ時間予算での解品質分布と seed 間分散

特に、重複排除、強い bound、多様化は、展開数を減らしても 1 秒当たりの探索量や品質を悪化させ得る。
「prune 数」だけを成功指標にしない。
