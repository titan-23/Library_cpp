# ビームサーチの並列化・BatchState・multi-start 設計監査

## 目的と前提

現行ビームサーチを汎用ライブラリとして並列化する場合の成立条件、意味論、API、メモリ上限を整理する。
実装コードと探索ロジックは変更していない。外部研究の実測値とリンクは
`research/beam_search/literature.md` の 5、6 節を参照し、この文書では重複して引用しない。

対象は主に次の三方式である。

1. 1 回の Beam の同一世代を CPU worker で exact に並列展開する。
2. ユーザー提供の `BatchState` で複数候補を CPU SIMD または GPU へまとめて渡す。
3. 独立した Beam を複数 seed で同時実行する multi-start。

ここでいう exact は、同じ rank、dedup reducer、terminal policy を使う canonical sequential oracle が
全候補から作る global top-`W` と、同じ候補集合を作るという意味である。現行実装の
未規定な同点順や、時刻依存の動的幅まで bit-identical にする意味ではない。

記号は次のとおり。

- `T`: worker 数
- `W`: global beam width
- `B`: 1 親当たりの平均分岐数
- `C`: 1 世代の全生成候補数。概ね `W*B`
- `U`: dedup 後の異なる key 数
- `P`: 生存候補が参照する異なる親数
- `F`: 同時に存在する active target pool 数
- `Q`: 平坦木の総 token 数
- `R`: 実際に走査して読む token 数
- `A`: 一時的に上位 `W` へ採用された候補イベント数
- `H`: ターンをまたぐ closed set が保持する異なる key 数
- `N`: live node 数
- `S`: 1 個の State が所有するメモリ
- `S_A`: 1 個の Action または保存 payload の byte 数

## 結論

- 現行の 1 mutable State + tour を細粒度 lock で共有しても並列化できない。State の現在位置が直前の
  apply/rollback に依存し、lock は探索本体を直列化する。
- CPU の exact backend は、worker ごとの独立 State、local dedup、local top-`W`、global merge とする。
- 各 worker は `W/T` でなく `W` 件を残す必要がある。`W/T` quota は別の近似・多様化アルゴリズムである。
- selective な min-rank reducer で local dedup してから local top-`W` を取れば、その和集合から
  exact global top-`W` を復元できる。Action メモリを全候補 C へ広げる必要はない。
- 並列 worker の cutoff は逐次版より緩くなりやすい。tie-aware な admissible cutoff なら exact 性を
  壊さないが、余分な `try_op()` が増え、並列 speedup を相殺し得る。
- GPU は top-k だけでなく、親状態、候補生成、score/hash 評価、dedup、選択を device resident にできる場合に
  限って有力である。
- core P0 後の並列層では、multi-start wrapper が最も低リスクである。ただし 1 run の latency を exact に
  短縮する機能ではなく、独立探索による throughput と品質多様化である。

## 現行 API をそのまま共有できない理由

### State の現在位置が逐次依存する

標準版は 1 個の `State` を作り (`beam_search.cpp:252-254`)、候補順に rollback と apply を行って親葉間を移動する
(`beam_search.cpp:344-369`)。次に展開する葉の状態は、直前の葉からどの Action を戻し、どの Action を適用したかに
依存する。

同じ State に mutex を付けて複数 worker が `apply_op -> enumerate -> rollback` を実行すると、その区間全体を
lock する必要がある。lock を細かくすると別 worker が途中の path 状態を観測し、粗くすると候補生成が直列になる。
State のデータ race だけでなく「現在どの葉にいるか」という論理状態が共有されることが問題である。

Compose 版も同じ tour 依存を持ち、さらに `compose_pass()` が Action と ghost 状態を変更する
(`beam_search_compose.cpp:82-115`)。Radix 版も DFS 中の一つの State を下り・上りで更新する
(`beam_search_radix.cpp:176-207`)。これらを shared mutable State のまま並列化しない。

### const メソッドだけでは thread-safe ではない

`enumerate_actions()` と `try_op()` が const でも、次は保証されない。

- State 内の `mutable` cache を書かないこと。
- global/static RNG、scratch buffer、統計を共有しないこと。
- Action が別 worker と同じ保存領域を参照しないこと。
- `init()` を複数回呼んでも同じ論理初期状態を作ること。

State 雛形には global な `brnd` がある。Zobrist 表の事前初期化だけなら問題ないが、探索中の乱数消費に使うと
data race と実行順依存が生じる。exact backend は乱数を使わないか、候補の stable ID から導く
counter-based RNG を使う。worker-local RNG は後述の stochastic policy に限る。

### Candidates と木更新も thread-safe ではない

共通 Candidates は `next_beam`、HashDict、segtree を push ごとに変更する
(`candidates.cpp:62-99`)。HashDict も control/key/value 配列を非 atomic に更新する。1 個の Candidates を lock で
囲むと、高分岐時に全 worker が最も熱い箇所で競合する。

また parent_leaf の採番、次世代 tour、世代 Action block は展開順を前提に構築される。worker はこれらへ直接
書かず、read-only な現世代木から thread-local descriptor を作り、barrier 後に coordinator が次世代木を作る。

`BeamParam` の時間・幅統計も共有 mutable なので、worker は更新せず世代終了時に集約する。

## exact な thread-local 生成と global merge

### 必要な全順序と dedup reducer

次を先に仕様化する。

- `rank(candidate)`: 例として `(score, tie_key)` の全順序。小さい方を残す。
- `RankCutoff`: 現在保持する最悪 rank。score と tie key の両方を持つ。
- `key(candidate)`: hash dedup なら State hash。
- `reduce_same_key(a, b)`: exact summary では、同じ key の min-rank を入力から一つ選ぶ。
- `tie_key`: 推奨は `(parent_global_order, child_ordinal)` またはユーザー提供の安定 ID。

reducer は selective、idempotent、associative、commutative で、`rank(reduce(a,b))=min(rank(a),rank(b))`
を満たす。canonical で結合順によらないだけでは不十分である。global reduce が local winner より
悪化する reducer では後述の証明が成り立たない。1 key に複数の Pareto label を残す dominance は
別 contract が必要で、この節の証明をそのまま使えない。

現行 code は score tie の完全な順序を公開していない。segtree の左右選択や `std::sort` の未規定 tie 順まで
逐次版と一致させるのでなく、並列 backend 導入時に tie_key を新たな明示仕様とする方が堅牢である。

### 世代同期パイプライン

固定深さ版の 1 世代を次の順で処理する。

1. coordinator が親候補へ global parent order を付け、T 個の連続範囲へ分ける。
2. 各 worker は独立 State で担当親を巡回し、Action を列挙・評価する。
3. 各 worker は同一 key を local に reduce し、その後 local top-`W` だけを保持する。
4. barrier 後、最大 `T*W` 件の descriptor を連結する。Action は worker arena の handle で参照する。
5. 同一 key を global に reduce し、global top-`W` を選ぶ。
6. 選ばれた Action だけを global 世代 block へ移し、coordinator が tour/tree を構築する。
7. 各 worker を共通 root 状態へ戻し、次世代へ進む。

finished 候補は各 worker が最良 1 件を持ち、barrier 後に同じ rank で reduce する。通常候補 W 件の selector と
混ぜない。ただし、通常候補の cutoff で潜在的な finished 候補を落とさない契約が必要である。

worker arena の handle は、global reduce/top-`W` と選択 Action の global block への移動が終わるまで
pin する。それより前に slot を再利用したり arena を破棄したりしない。

可変ターン版では target_turn ごとに幅 W の集合があるため、手順 3--5 を target_turn ごとに行う。worker-local
selector の総メモリが `T*F*W` に近づき得るため、固定深さ版より導入優先度は低い。
現行の target pool は作成世代から target turn 到達まで候補を持ち越す。従って target `t` の merge は、
既存 global pool `E_t` と各 worker の新規 summary の和集合に対して行う。古い Action は survivor が
確定してから解放する。

さらに turn 版の global `seen_hash` は、per-target selector に一度採用された候補だけを探索途中で登録する
(`beam_search_turn.cpp:319-353`)。登録時点が threshold と列挙順に依存するため、raw candidate に対する単純な
可換 reduce では現行挙動を再現できない。初期 exact backend は `clear_hash_every_turn=true` に限定する。後で
global-seen を扱うなら、threshold に依存しない pure な全評価後に、coordinator が概念上の submit 順で
threshold、seen、selector の更新を serial simulation する。そうでなければ、世代単位の新しい
dominance semantics として結果変更を明記する。登録順だけを合わせても現行結果は再現できない。

### local top-W だけで exact になる理由

worker i の全候補集合を `S_i`、local dedup 後の集合を `R_i`、その top-W を `L_i` とする。

global top-W の候補 x の min-rank winner が worker i から来たとする。もし x が `L_i` に無ければ、
`R_i` 内に x より rank が良い異なる key が少なくとも W 個ある。min-rank reduce により、
その各 key の global winner は local winner より悪化しない。従って global にも x より良い
異なる key が W 個あり、x は global top-W ではない。
これは矛盾する。

よって global top-W の全要素は `L_0 ∪ ... ∪ L_(T-1)` に含まれる。全候補 C や全 Action を gather せず、最大
`T*W` 件を global dedup + top-W すればよい。同じ理由で pairwise に

`summary(X, Y) = topW(reduce_by_key(X ∪ Y))`

を適用する tree merge も可能である。この summary は上記の selective min-rank reducer に対するものである。

重要な条件は、各 worker が local top-W の前に dedup して W 個の異なる key を数えることである。重複を含む
raw stream を先に W 件へ切ると、重複が slot を占め、後で必要な W+1 番目を失う。

dedup を無効にする policy では、通常の local top-W の和集合から global top-W を取れば同じ証明が成り立つ。

### `W/T` quota は exact ではない

各 worker が `W/T` 件だけ残す方式は、ある worker の範囲に global 上位候補が集中したときに候補を失う。
hash owner ごとに `W/T` を割り当てる方式も同じであり、global beam width W の逐次版とは別アルゴリズムになる。

これは通信量とメモリを減らし、多様性を増す場合があるため、`DistributedQuotaBeam` のような明示的近似 policy
にはなり得る。ただし `ExactParallelBeam` と同じ API flag で暗黙に切り替えない。

## threshold の意味と並列化

### worker-local threshold は安全だが緩い

min-rank local dedup 後に W 個が埋まれば、その worker 内だけで W 個の異なる良い key が
存在する。従って local worst rank 以上の通常候補は global top-W にも入れない。

この cutoff は score だけでなく `(score,tie_key)` の `RankCutoff` である。現行の scalar API を使う場合、
State は最終 `score > cutoff.score` と証明できた場合だけ早期 reject する。等点候補は tie key で
改善し得るため、
`score >= cutoff.score` だけで落とさない。等点が必ず悪いと証明できる domain contract がある場合は例外である。

ただし各 worker が見る候補数は全体の約 1/T なので、local set が W へ達するのが遅い。逐次版の global exact
threshold より `INF` の期間が長く、埋まった後も通常は緩い。並列版は selector を T 倍持つだけでなく、State の
早期打切りが減って余計な score 計算をし得る。

### optional な global published threshold

min-rank reduce した既観測候補から W 個の異なる key が確定していれば、その worst rank は
将来の真の cutoff 以上であり、安全な上限である。coordinator は local summary を定期 merge し、
単調に厳しくなる tie-aware snapshot cutoff を publish できる。

- stale で大きい値は余分な計算を増やすだけで、候補を誤って落とさない。
- 根拠のない予測値や `W/T` quota の worst を global cutoff として使ってはいけない。
- generic ScoreType を lock-free atomic に詰めるとは限らない。世代 epoch ごとの read-only snapshot で十分である。
- snapshot は barrier または double buffer で整合した immutable 値とし、更新中の selector を並行 read しない。
- cutoff 更新を頻繁に同期すると cache line contention が生じる。まず local threshold のみを基準にする。

BatchState は候補 batch の途中で cutoff を更新しにくいため、さらに緩い snapshot を使う。`evaluate_batch()` の
speedup が、失われる early cutoff を上回る条件でだけ有効になる。

### threshold を使う State 側の契約

exact backend では、cutoff は「候補の最終 rank が cutoff より良くならず、通常候補 top-`W` に
入らない」と証明できる場合だけ枝刈りに使う。cutoff に応じて乱数、候補種、列挙順を
変えない。early reject は副作用のない pure な判定とする。

通常候補の cutoff は terminal 選択の bound ではない。State は finished の可能性をこの cutoff で消さず、
次のいずれかを満たす。

- finished を判定してから通常候補 cutoff を適用する。
- terminal 候補にも適用できる別の admissible incumbent bound を使う。

現行ガイドの scalar `score >= threshold` reject は、等点 tie と terminal の両条件を満たさない限り、
そのまま parallel exact capability にはできない。

## worker State の作り方と巡回方式

### 独立 State は必須

各 worker は次のいずれかで同じ canonical root 状態を持つ。

1. `State clone() const` または copy constructor による deep independent clone。
2. immutable checkpoint から `restore()` する。
3. `StateFactory()` が同じ root 状態を再構築する。

shared immutable problem data や Zobrist table は共有してよい。盤面、score、hash、scratch、RNG は worker-local にする。
clone が shallow pointer を共有し、片方の apply が他方を変える設計は禁止する。
`apply_op()` と `rollback()` は exact inverse で、巡回後の論理状態を canonical root へ戻す。

State copy が小さく安いなら、そもそも連続 State-copy backend の方が Euler 巡回より速い可能性がある。parallel
Euler backend は「T 個の State は許容できるが W 個の State は重い」という中間領域を対象にする。

### State は世代ごとに clone しない

基本案は T 個の State を検索中ずっと保持し、各 worker が担当範囲の巡回終了時に canonical root へ rollback する。
一本道 prefix が確定したら、同じ Action を全 worker へ一度 apply する。これならメモリは T*S、clone は検索開始時
だけで済む。

検索開始時の `init()` が worker id や実行順で異なる値を作る場合は exact でない。canonical State を一度 init し、
そこから clone するのを既定とする。

### 親範囲は連続に分ける

現行 tour の利点を残すには、global DFS/leaf order の連続範囲を worker へ渡す。各 worker は canonical root から
担当最初の親へ入り、担当範囲内だけ Euler 巡回して root へ戻る。

worker 境界をまたぐ shared path は複数回 apply/rollback されるため、単一 State より辺走査数は増える。深い共通
prefix を root として先に確定する、または誘導部分木を T 個の連結 chunk に切ることで重複を抑える。

静的等分は分岐数や `try_op()` 費用が親ごとに偏ると load imbalance を起こす。前世代の子数・時間から連続範囲を
weighted partition する案を次に試す。細粒度 work stealing は worker の State 現在位置を壊し、task ごとの path
replay または checkpoint copy が必要になるため既定にしない。

### NUMA と false sharing

- worker State、candidate arena、hash table は担当 thread で first-touch する。
- worker counter と published summary は cache line を分ける。
- global merge は pairwise tree にし、全 worker が 1 table の lock を叩かない。
- thread pool は検索ごとに作らず、長い検索中は固定する。
- T は `min(hardware_threads, useful_parent_count)` と memory budget から決める。

## メモリモデル

exact CPU backend の追加メモリは概ね次である。

- worker States: `T*S`
- local selector/hash/Action: `T*O(W*(metadata + S_A))`
- merge descriptor: 最大 `T*W`。Action 本体をコピーせず `(worker_id, slot)` で参照可能
- merge scratch: pairwise merge なら各 merge 当たり O(W)、全体 workspace は実装方針で O(TW) または O(W)
- global tree/action history: 逐次版と同じ O(N)

local width を W から W/T へ減らせないことが、exact 並列化の主なメモリ費用である。`T*W*S_A` が上限を超える
場合は thread 数を下げる。Action を採用時だけ local arena へ保存し、全 C 件を保存しない。
arena の最大 live Action 数を W に抑えるには、追い出し slot を安全に再利用し、barrier 中の handle を
pin する必要がある。

これは active candidate hash が O(W) に制限されている前提である。現行共通 Candidates は一時採用数に応じて
hash table が増えるため、`candidate_pipeline.md` の bounded active table を並列 backend より先に実装する。

可変ターン版は target ごとに local W が必要で、最悪 `T*F*W*(metadata+S_A)` へ膨らむ。現行方式は
一時採用ごとに `new_candidates` と Action arena を増やすため、dirty slot / pending Action 化の前は
O(A) も加わる。active target が多い場合は、hash owner へ stream して owner-local selector を共有する方式も
候補になるが、worker 間 queue と Action lifetime が複雑になる。まず固定深さ版を対象にする。

global closed set を無制限に使う場合の O(H) は並列化しても消えない。世代単位で可換に更新できる policy なら、
read-only snapshot を全workerで共有し、更新をlocal logへ置いてbarrier後にreduceする。workerごとに複製すると
T*Hになる。探索途中の採否で更新条件が変わるorder-sensitive policyには、この方法を適用しない。
さらに barrier までの local log は policy により O(A) から O(C) になり得る。chunk ごとに可換 reduce するか、
別の上限を与えない限り、O(H) だけを hard memory bound としない。

`memory_limit_bytes` は、State/Action の所有 heap、closed-set log、workspace の必要量を見積もれる場合だけ
hard limit にできる。`StateMemoryTraits` または user hint がない backend では soft limit と明記する。

## 順序、乱数、再現性

### serial key

worker 完了順、queue 到着順、hash table 配置を候補順に使わない。各候補へ少なくとも次を持たせる。

- `parent_global_order`
- `child_ordinal` または domain 固有の stable action ID
- 可変ターン版では `target_turn`
- 必要なら `run_id`

`tie_key=(parent_global_order, child_ordinal)` とすれば thread 数や scheduling が変わっても同じ全順序を作れる。
enumerator が threshold で候補自体を省く場合、child_ordinal は「submit した順」ではなく概念上の Action ID にする方が
安定する。

### RNG

共有 RNG を lock すると順序依存になり、lock なしでは data race になる。乱数を使う場合は次を分ける。

- exact: `(base_seed, parent_id, action_id, purpose)` の counter-based RNG で、candidate stable ID ごとに固定する。
- stochastic: `(base_seed, generation, worker_id)` の worker-local RNG を認めるが、thread 数で結果が変わる。

逐次版の単一 RNG 消費列を完全再現するには概念上の全候補へ serial offset を割り当てる必要があり、可変分岐や
枝刈りと相性が悪い。canonical sequential oracle も同じ counter-based policy を使う。worker-local RNG を
使う mode は stochastic parallel policy であり、この文書の exact とは呼ばない。

### その他の非決定性

- floating score の演算順、NaN、GPU FMA は結果を変え得る。NaN を禁止し total order を定義する。
- `record_history` は worker-local に記録し、serial key で merge する。共有 vector へ lock push しない。
- `is_adjusting=true` は速度変化そのものが幅を変える。fixed-width exact と時間予算内の品質分布を分けて測る。
- exception は worker pool で最初のものを保存し、全workerをcancelしてjoin後に再throwする。

## CPU parallel backend の API concept 案

既存 `BeamSearchWithTree::search()` は変更せず、別 backend と opt-in API を追加する。

```cpp
ParallelBeamOptions opt;
opt.threads = 8;
opt.memory_limit_bytes = ...;
opt.deterministic = true;

auto result = search_parallel<materialize_final_state>(state_factory, param, opt, executor);
```

### CloneableParallelState

最低限の追加契約は次のとおりである。

```cpp
struct State {
    State clone() const;                  // writable storage を共有しない
    void apply_op(const Action&);
    void rollback(const Action&);

    template<class Submit>
    void enumerate_actions(int turn, const Action& last, Submit&&) const;

    EvalResult try_op(Action&, const RankCutoff&) const;
};
```

`clone()` の代わりに StateFactory / CheckpointPolicy を受けてもよい。現行 scalar threshold の fallback は、
等点で early reject せず、ライブラリ側が完全な rank で比較する。C++ concept が確認できるのは signature だけであり、
deep independence、determinism、shared global なしは文書契約と debug test が必要である。

推奨 capability は次である。

```cpp
struct ParallelStateTraits {
    static constexpr bool independent_clone = true;
    static constexpr bool deterministic_enumeration = true;
    static constexpr bool deterministic_evaluation = true;
    static constexpr bool exact_apply_rollback = true;
    static constexpr bool rank_cutoff_is_admissible = true;
    static constexpr bool cutoff_preserves_terminal = true;
    static constexpr bool action_is_thread_safe_readonly = true;
};
```

false の capability があれば compile error にするのでなく、逐次 backend へ明示 fallback する。ただし fallback 理由を
ログまたは status で確認できるようにする。

### Candidate descriptor と reducer

```cpp
struct ParallelCandidate {
    ScoreType score;
    HashType hash;
    uint32_t parent_order;
    uint32_t child_ordinal;
    uint32_t target_turn;
    PayloadHandle payload;
};
```

DedupPolicy は少なくとも次を提供する。

```cpp
auto key(const ParallelCandidate&) const;
bool better(const ParallelCandidate&, const ParallelCandidate&) const;
ParallelCandidate reduce_same_key(ParallelCandidate, ParallelCandidate) const;
```

`better` は `rank` と同じ全順序を表す。exact summary の `reduce_same_key` は selective min-rank であり、
associative、commutative、idempotent を trait にする。hash collision を実 State 比較で検証する DomainDedup は、
worker State を必要としない immutable equivalence key を descriptor へ出す。

### Executor と Workspace

core header が OpenMP、TBB、CUDAへ直接依存しないよう、backend は次の役割を持つ Executor を受ける。

- 固定 worker 数の `parallel_for_workers`
- barrier と cancel
- worker-local index
- exception propagation

候補 table、Action arena、tour scratch は `ParallelBeamWorkspace` に事前確保し、世代ごとの allocation を避ける。
memory limit から thread 数を下げる場合、結果を変えたくなければ tie/RNGをworker idから独立させる。

## BatchState API は二段階に分ける

### 段階 A: sibling batch

最小の拡張は、1個の現在 State から列挙した兄弟 Action をまとめて評価する API である。

```cpp
void try_ops(
    std::span<Action> actions,
    RankCutoff threshold_snapshot,
    std::span<EvalResult> out,
    BatchWorkspace& workspace) const;
```

現在の vector fallback に近く、1親内の B を SIMD 化できる。State copyや木backendの変更は不要である。一方、
batch 内で cutoff が下がらず、ActionをB件すべて構築する。Bが大きく評価が規則的な場合だけ有利である。

可変ターン版は Action ごとに target_turn が異なるため、target ごとの `RankCutoff` の
read-only snapshot を渡す。

### 段階 B: full ParentBatch backend

GPUや親間SIMDには、Euler Stateとは別に W個の親状態をbatch表現する backendが必要である。例示的な契約は
次のとおり。

```cpp
struct BatchStateBackend {
    using ParentBatch = ...;   // SoA、hostまたはdevice resident
    using ActionKey = ...;     // 小さく、可能ならtrivially copyable
    using EvalAux = ...;       // 選択後のAction構築に必要な情報

    void init(ParentBatch& root, BatchWorkspace&);

    void enumerate_batch(
        const ParentBatch& parents,
        RaggedBuffer<ActionKey>& keys,
        BatchWorkspace&) const;

    void evaluate_batch(
        const ParentBatch& parents,
        const RaggedBuffer<ActionKey>& keys,
        ThresholdView thresholds,
        std::span<BatchEval> out,
        BatchWorkspace&) const;

    void materialize_selected(
        const ParentBatch& parents,
        const RaggedBuffer<ActionKey>& keys,
        std::span<const BatchEval> selected,
        ParentBatch& next_parents,
        std::span<Action> path_actions,
        BatchWorkspace&) const;
};
```

`RaggedBuffer` は parent offset と連続 ActionKey を持つ。`BatchEval` は score、hash、finished、target_turn、parent index、
key index、EvalAux を持つ。top-W と dedup はこの小さい descriptor で行う。

### Action を全候補分保存しない契約

current `try_op()` は rollbackに必要な pre/nxt 情報を Actionへ書く。full batchで同じことをC件へ行うと、候補配列が
`C*S_A` になりGPU memoryとbandwidthを圧迫する。次のどちらかを明示する。

1. eager payload: evaluate_batchが全候補の完全Actionを出す。単純だが`C*S_A`メモリ。
2. deferred payload: 小さい ActionKey と EvalAuxだけを出し、選択W件だけ materializeする。

deferred では `materialize_selected()` が評価結果と同じ遷移を再現し、score/hashを変えない必要がある。二度目の
評価が乱数や外部状態へ依存してはいけない。再計算が高価なら、EvalAuxへ必要な差分を保存する。

BatchStateは通常、selected successorのfull next stateだけを作るためapply/rollbackを必要としない。これは現行
Euler backendの単なる高速pathでなく、State-copy/SoA系の別execution backendである。

### Batch capability

backend選択には少なくとも次を明示する。

- host / device resident
- deterministic evaluation / materialization
- maximumまたは可変branching
- eager / deferred Action payload
- Score/Hashのdevice対応
- tie-aware cutoff snapshotによるexact pruning可否
- workspace必要量を事前計算する `required_bytes(W, max_successors)`

## GPU backend が成立する条件

GPU top-k primitiveの速さだけでは導入理由にならない。次をほぼすべて満たす必要がある。

1. ParentBatch、ActionKey、score、hashが世代をまたいでdevice residentである。
2. 候補生成とscore/hash評価をkernelで実行できる。
3. `C=W*B` がlaunch、scan、sort/selectの固定費を十分に償却する。
4. StateとActionがSoAで、candidateごとのallocation、virtual call、string、例外を必要としない。
5. 分岐と処理量のばらつきが小さいか、ragged offsetとcompactionでwarp divergenceを抑えられる。
6. dedupをdevice hashまたはsort-by-hash + segmented reductionとして実装できる。
7. 選択W件だけを次ParentBatchへgatherし、世代ごとに全候補をhostへ戻さない。
8. device memoryにcandidate descriptor、workspace、W個の次状態が収まる。

CPUで `try_op()` まで行い、score配列だけGPUへ送りtop-k後に戻す方式は、通常PCIe転送とlaunchを回収しにくい。
GPU selectorはcore Candidatesの差替えではなく、full BatchState backendの内部に置く。

### GPUでのexact性と決定性

- unordered top-kの出力順はそのまま親順に使わず、`(score,tie_key)` で最終整列する。
- hash table atomicのwinner更新順に依存させない。sort 後に selective min-rank reduce する。
- float FMA、reduction順、fast-math差を許容するか、CPU/GPU同一結果を要求するかをpolicy化する。
- Batchのstale cutoffは緩い `RankCutoff` だけを使い、terminal safety も保つ。近似cutoffで候補を
  落とす場合はexact backendと呼ばない。

### GPU memoryをCに比例させない案

分岐数が非常に大きい場合、全Action payloadを保存せず、次を組み合わせる。

- compact ActionKey + EvalAux
- chunkごとのlocal top-W summaryとhierarchical merge
- bounded integer scoreならhistogram cutoff
- hash partitionごとのreduce

ただし chunk summary は、selective min-rank reducer で chunk 内 dedup してから top-W にする。global merge も
同じ reducer、tie key、terminal policy を使う。raw duplicate が slot を占める順序は exact でない。

## independent multi-startとの違い

### 定義

multi-startはT個の完全に独立したBeamを異なるseed、tie noise、parameterで走らせ、最後に最良Resultを選ぶ。
世代ごとのcandidate、hash、threshold、treeは共有しない。

exact intra-beam parallelは、1個のglobal幅Wと1個の世代木を維持し、同じ世代の候補生成だけを分割する。

| 項目 | exact intra-beam | independent multi-start |
|---|---|---|
| 目的 | 1 runのlatency短縮 | throughput、seed多様化、品質改善機会 |
| global beam | 1個、幅W | runごとに独立 |
| barrier | 各世代 | 原則なし |
| State memory | T個 + global tree | T個の探索全体 |
| candidate memory | local T*W + merge | 各runのW/tree/hash |
| 結果 | 明示 comparator で逐次集合と同じ | 幅合計が同じでも別探索 |
| scaling阻害 | merge、barrier、stale cutoff、load imbalance | memory bandwidth、run数、外部資源 |
| failure隔離 | worker失敗でrun失敗 | catchした例外はrun間で分離可能 |

幅Wの1 runと、幅W/TのT runは同じ探索ではない。狭いrunは早期に別の枝を失う一方、seed差で多様化できる。
品質は問題依存なので、speedupと同じグラフへ混ぜない。

### multi-start API案

```cpp
MultiStartOptions opt;
opt.runs = 8;
opt.max_concurrency = 8;
opt.base_seed = 12345;
opt.memory_limit_bytes = ...;

auto best = search_multi_start(state_factory, beam_param_factory, opt, result_better);
```

- StateFactoryは `(run_id, derived_seed)` を受ける。
- BeamParamはrunごとのcopyを使い、統計vectorやtimerを共有しない。
- seedはthread schedulingでなくrun_idから安定に導く。
- Result tieは `(score, run_id)` などで固定する。
- `materialize_final_state=false` で全runを走らせ、条件を満たす場合だけ winner を最後に再生する。
- memory limitに応じて同時run数を減らし、wave実行する。
- nested parallelを既定で禁止し、multi-start run内のthread数との積でoversubscribeしない。

winner 再生には、Result 比較が final State を必要とせず、factory が同じ問題入力、seed、config から
canonical initial State を再構築でき、`apply_op()` が deterministic であることを要求する。保存した
Action 列も外部状態や元 run の arena を参照しない。

現行 `search()` は State を内部で default construct して `init()` する。入力を再読みする State を外側から
並列呼び出しするだけでは安全でない。multi-start にも factory-aware な search entry point が必要である。
thread 内で catch した例外は run 単位で扱えるが、segfault、data race、`std::terminate` の隔離には
別 process が必要である。

successive halvingは途中scoreから悪いrunを止める別policyである。現行searchはresume checkpointを公開しないため、
単純wrapperでは安全に継続できない。途中scoreが最終品質を予測しない問題もあるので初期版へ含めない。

## 並列機能内の成立条件と局所優先度

| 方式 | 効果が見込める条件 | 悪化条件 | 意味論 | 局所優先度 |
|---|---|---|---|---|
| multi-start | 複数seed、十分な総予算、品質分散あり | 1runだけ必要、memory不足 | 別探索 | 局所P0 |
| sibling batch CPU | B大、規則的try_op、SIMD可能 | B小、cutoffが強い | 条件付きexact | 局所P1 |
| exact CPU generation | W*B大、State clone T個可、評価重い | W/B小、State巨大、世代短い | rank上exact | 局所P1-P2 |
| hash quota distributed | 通信・memory制約、diversityを許容 | exact集合が必要 | 近似 | 局所P2 |
| full CPU ParentBatch | 小さなSoA State、vector化可能 | 大State、遷移不規則 | 条件付きexact | 局所P2 |
| GPU BatchState | device resident、大C、高算術密度 | CPU生成、毎世代転送、分岐不規則 | 条件付きexact | 局所P3 |

局所 P0 の multi-start は、並列 API 候補の中では導入リスクが低いという意味である。研究全体の P0 は
active hash、turn 版 O(A)、Action 遅延保存の改善である。multi-start は 1 run の高速化率を測るものでなく、
全体計画では optional な P1 相当とする。exact CPU backend は O(A) 問題、Action 遅延保存、tie 規則を
先に整えてから着手する。

## 推奨実装順

以下は研究全体の P0 である O(A) と Action 所有権の改善後に、並列機能内で進める順序である。

1. `CandidateRank` と `DedupReducer` を明示し、local summaryをpairwise mergeする逐次helperを作る。
2. parent/actionへ安定したtie keyを付け、thread数に依存しない候補集合テストを用意する。
3. StateFactory、run seed、Result reducerだけを持つmulti-start wrapperを作る。
4. sibling batchのoptional `requires` 経路を合成Stateで比較する。strict-score の scalar fallback は残す。
5. fixed-width、clear-hash-every-turn、record-history=falseの標準版だけでexact CPU backendを試す。
6. worker-local W、pairwise global merge、static contiguous parent partitionから始める。
7. branch countによるweighted contiguous partitionとglobal threshold snapshotを個別に測る。
8. closed-set snapshot、history、Compose/Radixを追加する。turn版はmemory上限を設計してから扱う。
9. full ParentBatch conceptを別header/backendとして定義し、CPU SoA実装でAPIを検証する。
10. device residentな問題adapterが実際にある場合だけGPU backendを追加する。

## 検証指標

並列 speedupだけでなく、逐次版より増えた仕事とメモリを測る。

- wall time、CPU time、speedup、parallel efficiency
- workerごとの親数、候補数、try_op時間、barrier待機時間
- 逐次thresholdとlocal/global snapshot thresholdの差、追加try_op数、early-exit率
- local A、local dedup数、local W、global union `<=T*W`、global duplicate数
- State clone/init回数、apply/rollback回数、worker境界で重複したpath辺数
- local selector、Action arena、merge scratch、State、closed set別のpeak memory
- lock/atomic回数、queue traffic、false sharing、LLC miss、NUMA remote access
- thread 数 1 で canonical sequential oracle の candidate 集合と一致するか
- T を変えて candidate/action 列が一致するか
- fixed seedのscore/action、同点率、hash collision test
- 同 score で tie key だけが改善する候補と、cutoff より悪い finished 候補の回帰 test
- hard/soft memory limit の区別、local log 件数、arena の live/pinned Action 数
- multi-startはbest/median/worst score、seed分散、総CPU時間、peak RSSを別に報告
- GPUはH2D/D2H byte、kernel/launch時間、device occupancy、descriptor/payload memory

比較軸は少なくとも W、B、State size、Action size、try_op cost、重複率、共有prefix長、thread数、NUMA、
bounded/unbounded score、固定幅/動的幅を含める。小W・軽い候補ではbarrierが負けることを正常な結果として扱う。

## 採用判断

汎用既定は引き続き単一Stateの逐次Euler backendとする。メモリが小さく、State契約が単純で、WやBが小さい
領域で強いからである。

並列機能は一つのboolで有効化せず、次の別entry pointにする。

- `search()`: 現行逐次Euler
- `search_multi_start()`: 独立run
- `search_parallel()`: CloneableParallelStateを要求するexact世代並列
- `search_batch()`: BatchStateBackendを要求するCPU/GPU batch

これにより、逐次hot pathへthread分岐・mutex・大きいworkspaceを持ち込まず、利用者がStateの性質とmemory budgetに
合うbackendを明示的に選べる。
