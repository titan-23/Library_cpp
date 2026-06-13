# beam_search_turn.cpp 高速化

対象: `titan_cpplib/ahc/beam_search/beam_search_turn.cpp`(+ `titan_cpplib/ds/hash_dict.cpp`)

---

# 実装済み

いずれも出力が変わらないことを確認した上で実装した(修正前の実行とバイト一致)。

| # | 内容 | 実装日 |
|---|------|--------|
| S8 | 展開する葉が無い世代(min_target_in_tree > turn)の walk / update_tree / mark_and_sweep をスキップ。ahc064 では全 2566 世代中 505 世代(約 20%)が該当 | 2026-06-11 |
| M2 | Candidates::reset の O(2s) fill を build_segtree 側へ移動。dict 初期容量の死んだ条件(`inner_len() == 1`)を修正し、cap16 から rebuild を繰り返して育つ無駄を解消 | 2026-06-11 |
| M3 | update_min_target_in_tree を削除し、update_tree のマージループ中に root レベルの min を計算 | 2026-06-11 |
| M6 | BeamCandidate からどこからも読まれていない hash フィールドを削除(32B → 24B) | 2026-06-11 |
| S3 | mark_and_sweep(毎世代の全走査 3 パス + alive_gen)を廃止し、update_tree 内の「死ぬ場所」6 箇所でのインライン解放に置き換え | 2026-06-11 |
| M1(ii) | threshold チェック(score >= thresholds[t])を seen_hash プローブと arena 予約の前に移動 | 2026-06-11 |
| M5(a)(c) | HashDict にテンプレート引数 `USE_HASH_FUNC`(既定 true = 既存利用箇所は無変更)を追加。false 時は hash() が xor 1 回になり、inner_set の hash 再計算コストも消える。プール dict と seen_hash は key が Zobrist hash(一様)なので false に切替 | 2026-06-11 |
| S1b | 展開しない葉への leaf_id 書き込みを廃止し、par を「展開葉の通し番号」に変更(全体 sort は維持なので同点順も不変)。S4 の前提条件 | 2026-06-11 |

効果(ahc064, 同一探索軌道):

- ローカル: 探索時間 1441ms → 約 1375ms(約 4〜5% 短縮)
- AtCoder ジャッジ: 1741〜1869ms → 1640〜1689ms(約 5〜10% 短縮)
- 短縮の主因は S3(+S1b, S8)による「毎世代の冗長なメモリ走査・書き込みの削減」。
  mark_and_sweep は自己時間 2% だが、毎世代キャッシュを洗い流す副作用があったため
  壁時計では自己時間以上に効いた。M5 / M1(ii) は use_global_seen=off の構成では
  ほぼ寄与なし(dict プローブはキャッシュミス支配で、撹拌の数命令は影に隠れる)。

## 実装したが取り消したもの

- **S2a(target_turn の事前チェック)**: try_op が target_turn を設定してよい契約と矛盾し、
  submit 時点での読み取りが未初期化読みになって候補の誤棄却が起きた。
  再導入の条件は「未実装の案 > S2」を参照。
- **S1a(sort の par 区間化 + 同点タイブレーク)**: 同点候補の並びが変わって探索軌道が
  変化し、ジャッジで TLE が出たため取り消し。ロジック自体はタイブレークを揃えた比較で
  出力バイト一致を確認済み(バグではない)。再導入の条件は「未実装の案 > S1a」を参照。

## 探索の決定性と検証方法のメモ

sort のタイブレーク(score → aid)を入れると探索順が完全に一意になり、
「結果を変えないはずの内部変更」を出力の diff で検証できる。
今回の実装済み項目はすべてこの方法(ahc064 の出力バイト一致)で検証した。

---

# 未実装の案

| # | 提案 | 種別 | 期待効果 | 実装コスト |
|---|------|------|----------|-----------|
| S1a | sort の par 区間化 + 同点タイブレーク(探索結果が変わるため要タイミング) | 構造 | 小〜中 | 小(実装は検証済み) |
| S2 | try_op を呼ぶ前の枝刈り(target_turn 事前チェック(要 opt-in) / 2 段階評価 / hash 先行) | 構造 | 大(try_op が重い場合) | 小〜中 |
| S4 | update_tree で変更のない部分木を memcpy でコピー | 構造 | 中〜大 | 大 |
| S5 | Action プールを固定サイズブロックの配列に変更(アドレス安定化) | 構造 | 中(Action が大きい場合は大) | 中 |
| S9 | プールのセグ木を「バッファ + 一括選抜」に置き換え(償却 O(1) 化) | 構造 | 中 | 中 |
| M1(i) | finished 候補の処理を seen_hash より先に(探索結果が変わる) | 定数倍 | 小 | 小 |
| M4 | arena への書き込みコピー削減(emplace 型 submit) | 定数倍 | 小〜中 | 中 |
| M5(b) | HashDict の key と val を 1 配列に同居 | 定数倍 | 小〜中 | 中 |
| L1 | seen_hash の容量とメモリ使用量の見直し | メモリ | 小 | 小 |
| L2 | prefetch などの微調整 | 定数倍 | 小 | 小 |

## 適用順

1. ここで再計測する(実装済み分を反映した状態で)。
2. **S9, S2b/c**: 効果は大きいが、S9 はプール周りの改修、
   S2b/c は optional API の追加を伴う。
3. **S5, M4, S4, M5(b)**: 実装が大きい。再計測で Action のコピー・update_tree・
   dict プローブがそれぞれ大きく残っていたら入れる。
4. **S1a, M1(i)** は探索結果が変わるため別枠:
   コンテスト外のタイミングで入れて複数ケースで評価する。

---

## S1a. sort の par 区間化(取り消し済み、再導入はタイミングを選ぶ)

一度実装して動作も検証したが、**同点候補の並びが変わって探索軌道が変化する**ため
取り消した(2026-06-11)。

内容:

- 毎世代の全体 sort (par, score) を「par 区間ごとの score sort」に置き換える。
  new_candidates は生成時点で par ごとに連続しているので、同点の順序を除き
  同じ並びになる(O(C log C) → O(C log b)、b は親あたりの候補数)。
  実装は、get_next_beam で葉 1 つの展開が終わるたびに、その葉が追加した区間を
  score で sort するのが簡単(update_tree 側は変更不要)。

分かったこと:

- 同点の並びは std::sort の実装依存なので、これは「同点の順序だけが変わる」変更になる。
  enumerate_actions が乱数で候補を間引く State(ahc064 など)では、同点の並びが 1 箇所
  変わるだけで以降の乱数消費がずれて探索全体が分岐する。今回それで解の発見が
  約 9% 遅いターンになり、ジャッジ上の実行時間が伸びて TLE になった。
- 同点タイブレーク(score → aid)を入れると順序が完全に一意になり、
  旧実装・新実装の両方に同じタイブレークを入れた比較で出力がバイト一致することを
  確認済み(= ロジックにバグは無い)。
- 再導入するときは、**タイブレークとセットで、結果が変わってよい
  タイミング(コンテスト外)で入れて複数ケースで評価する**こと。
- ahc064 での速度差は ±2% の誤差範囲だった(sort は支配的コストではないため)。
  効果を期待するのは候補数が多く sort がプロファイルに見えている問題。

関連メモ: search() には時間打ち切りが無い(time_limit は is_adjusting の
幅調整にしか使われない)ため、軌道が変わると実行時間も変わる。

---

## S2. try_op を呼ぶ前に枝刈りする

try_op が重い場合は、呼び出し回数自体を減らすのが最も効く。

### (a) target_turn の事前チェック(State の opt-in が必要)

`action.target_turn > max_turn_global` の候補は try_op を呼ばずに捨てられるはずだが、
**現在の契約では target_turn は try_op が設定してよい**(ahc064 がそう)。
submit 時点で読むと未初期化値を読むことになり、実際に候補の誤棄却が起きた(2026-06-11、
一度実装して取り消し済み)。入れるなら「enumerate_actions が submit 前に target_turn を
設定する」ことを State が明示する opt-in(例: `static constexpr bool
target_turn_set_on_submit = true;`)とセットにする必要がある。
how_to_use には「submit 前に target_turn を設定すると速くなる」と書く。

### (b) 2 段階評価 API(optional)

`state.estimate_op(action)`(安価に計算できるスコアの下界)を optional に追加する。

```cpp
if constexpr (requires { state.estimate_op(action); }) {
    if (state.estimate_op(action) >= thresholds[action.target_turn]) return;
}
auto [score, hash, finished] = state.try_op(action, thresholds);
```

threshold が締まった後半の世代では多くの候補が下界だけで棄却できる。
`requires` で分岐するので、実装しないユーザーには影響がない。
(現在も Submitter の `threshold()` を使えば enumerate_actions 内で枝刈りできるが、
ライブラリ側で行えば確実に効く。how_to_use にも書く。)

### (c) hash を先に評価する(重複の多い問題向け、API 追加)

現在は「score を計算 → hash → 重複判定」の順。Zobrist hash の差分計算は
score 計算より大幅に安いことが多いので、try_op を

1. `try_hash(action)` → seen_hash とプール内の重複を先に判定
2. 通った候補だけ `try_score(action)`

に分割する optional API を用意する。同一局面への合流が多い問題では
score 計算の回数を数割減らせる。
ただし「同じ hash でも score が良ければ通す」規則があるため、完全にスキップできるのは
「hash が既出かつ target_turn で勝てない」場合のみ。それでも seen 側の target_turn が
小さい枝は score 計算なしで捨てられる。

---

## S4. update_tree で変更のない部分木を memcpy でコピーする

毎世代の tree → nxt_tree の再構築は O(tree) で、マルチターンでは
tree の大きさが W ×(抱えている target_turn の幅)に比例して大きくなる。
実際に変化するのは「この世代に展開した葉の周辺」と「追い出された葉」だけなので、
それ以外の部分木をそのままコピーできるようにする。必要な変更は 2 つ
(葉への書き込み廃止 S1b は実装済みで、触れていない部分木は既にバイト単位で同一):

1. **subtree_end を絶対 index から相対長に変える。**
   コピーで位置がずれても部分木内部の値を書き換えずに済む。
   読み飛ばしは `i = node.subtree_end` の代わりに `i += node.subtree_len` になる。

2. **追い出された葉の遅延削除。**
   現在は追い出された葉をその世代の update_tree で取り除くため、どの部分木も
   変更され得る。代わりに死んだ葉を木に残し(is_survived_node で判定できる)、

   - get_next_beam の展開時に is_survived_node を見て死んだ葉は読み飛ばす
   - 物理的な削除は、その部分木が別の理由で変更される世代か、
     その葉の target_turn が来た世代に行う

   とすると、subtree_min > turn の部分木は無条件にバイト単位で同一になり、
   `memcpy(&nxt_tree[dst], &tree[i], len * sizeof(TreeNode))` でコピーできる
   (TreeNode は 16B の POD)。

トレードオフ:

- 死んだ葉が一時的に木に残る分、木と action_pool の使用量が増える
  (各葉は遅くとも自分の target_turn の世代で削除されるので増え方には上限がある)。
- 死んだ葉が subtree_min を保持している場合、その turn に部分木を無駄に降りる
  (結果は正しいまま。apply/rollback が少し増えるだけ)。
- **死んだ葉の aid は物理削除まで解放できない**(S3 のインライン解放は実装済みなので、
  「追い出された旧葉」の解放箇所を物理削除時に移す必要がある)。
  解放前に slot が再利用されると、木に残った古い aid が別の Action を指してしまう。
- 「毎世代ほとんどの部分木が変更される」状況(W 小、進行が速い)では memcpy できる
  部分木が少なく、わずかに損になり得る。効くのは触れない部分木が大きい状況
  (W 大、target_turn の幅が広い)。

実装が大きいので、再計測で dt_update が大きく残っていたら入れる。

---

## S5. Action プールを固定サイズブロックの配列に変える(アドレス安定化)

get_next_beam の葉展開時:

```cpp
// ループ内の arena_put_reserve が action_pool を reallocate しうるので、
// 参照では保持できず、ローカルに copy する
Action action = act(node.aid);
```

Action が大きい場合、展開葉 1 つにつき 1 回の大きなコピー(世代あたり W 回)に加え、
vector の成長時にはプール全体の移動コピーが発生する。

対策: action_pool を固定サイズブロックの配列にする
(例: `vector<unique_ptr<Action[]>>`、ブロック 1024 個、`aid >> 10` と `aid & 1023` でアクセス)。

- アドレスが安定するので `const Action& action = act(node.aid);` のまま保持でき、
  展開時のコピーがなくなる
- 成長時の全要素移動もなくなる
- 代償は act() の 2 段インデックス(シフト + マスク + 間接参照 1 回)。
  act() を複数回呼ぶ箇所では参照をローカル変数に取れば十分相殺できる。

---

## S9. プールのセグ木を「バッファ + 一括選抜」に置き換える

Candidates は受理のたびにセグ木を更新して正確な worst を維持している
(受理 1 件あたり O(log W))。これを

- 容量 2W のバッファに溜めるだけ溜める
- あふれたら nth_element で score 上位 W に圧縮し、threshold を更新する

に置き換えると、受理 1 件あたりの処理が償却 O(1) になり、
セグ木のメモリと cands_set / build_segtree が不要になる。

- hash の重複は dict(hash → バッファ index)で従来通りその場で置き換える。
  圧縮時に dict を作り直す(O(W)。HashDict に削除が無いため死んだ key が
  溜まり続ける現在の問題も、これで実質解消される)。
- 最終的な選抜結果(score 上位 W、同 hash は best のみ)は現在と同一。
- 代償: thresholds[t] の更新が圧縮時だけになり、圧縮までの間は threshold が
  実際の worst より緩くなる。その分 try_op 側の枝刈りが弱まり、
  バッファに入る候補も増える。受理率 82% という計測からは現在も threshold は
  あまり締まっていないので影響は小さいと見込まれるが、要計測。
  圧縮の閾値を 2W ではなく 1.25W にすれば緩み幅は抑えられる。

---

## M1(i). finished 候補の処理を seen_hash プローブより先に行う

((ii) threshold チェックの前倒しは出力不変を確認して実装済み)

現在は完成解の候補も seen_hash の重複で落ちることがある
(より小さい target_turn で同じ hash を見ていた場合)。
完成解はプールに入らないので重複排除する必要がない。プローブ 1 回の節約に加えて
完成解を取り逃さなくなる。
※use_global_seen 有効時に探索結果が変わり得る(完成解を見つけやすくなる方向)ため、
S1a と同じく導入タイミングを選ぶこと。

---

## M4. arena への書き込みコピーの削減(emplace 型 submit)

- arena_put_fill は `action_pool[slot] = a;` のコピー代入。受理候補 1 件につき 1 回。
- emplace 型の submit API を足すとこのコピーをなくせる:
  `Action& slot = submit.reserve();` でユーザーが arena のスロット上に直接構築し、
  `submit.commit();` で判定する。棄却された場合は同じスロットを次の submit で再利用する。
  S5(アドレス安定化)と組み合わせると参照の無効化も起こらず、
  受理候補のコピーが完全に 0 になる。
- API が増えるので、Action が大きい問題で必要になったときに入れる。
- ドキュメント側: Action が大きい場合は Submitter API の使用を強く推奨する。

---

## M5(b). HashDict の key と val を 1 配列に同居させる

((a) hash 再計算と (c) Zobrist 用の撹拌省略は `USE_HASH_FUNC=false` の導入で実装済み)

keys と vals が別配列なので、ヒット 1 回で meta → keys → vals の
3 本のキャッシュラインを踏む。val が小さい場合(プール dict の int など)は
key と val を 1 つの配列に並べると(16B/entry)、keys と vals が同じラインに乗る。
HashDict 全体のレイアウト変更になるので、プローブがプロファイルで大きく出たときに行う。

---

## L1. seen_hash の容量とメモリ

デフォルトの容量ヒント `W * max_turn * 2` は、W=1000, max_turn=1000 で約 420 万 slot
→ keys 33MB + vals(pair 16B)67MB + meta 4MB で 100MB を超える。
メモリ制限に収まっても、プローブがほぼ毎回キャッシュミスになる。
実際の挿入数(受理 push 数 × 世代数)に合わせたヒントを設定できることを
how_to_use に書く。逆に小さすぎると探索中の rebuild(全再ハッシュ)で
処理時間のスパイクが出る。

候補の大半が「未登録」の問題では、seen_hash の手前に L2 キャッシュに収まる
サイズのビットフィルタ(hash の下位ビットで引く 1bit 表)を置き、
「確実に未登録」の候補は本体のプローブを省く手もある。未登録の候補は
受理時の挿入で結局本体に触るので、効くのは「プローブ数 ≫ 受理数」の場合のみ
(M1(ii) 実装済みの現在はプローブ数が受理数に近く、効果は限定的)。

---

## L2. その他の微調整(計測してから)

- **prefetch**: walk 中に `__builtin_prefetch(&act(数ノード先の aid))`。
  aid は LIFO のフリーリストで再利用されて散らばるためハードウェア prefetch が効かず、
  Action が大きい場合は apply/rollback のキャッシュミスを隠せる可能性がある。要計測。
- **action_pool の局所性**: 世代を重ねると aid の並びが木の順から乖離していく。
  update_tree の再構築時に Action を木の順に詰め直す方法もあるが、
  「Action のコピー」対「キャッシュミス」の交換なので、prefetch で足りない場合の手段。
- **TreeNode の詰め込み**: 現在 16B(int×4)。POST_ORDER は target_turn と subtree_end を、
  葉は subtree_end を使わないので 12B にできるが、アラインメントが崩れる。
  S4(memcpy 化)の後ならコピー量の削減として意味が出るかもしれない程度。

---

# 参考情報

## 計測結果 (2026-06-11, ahc064 / 実装前のもの)

ahc064.cpp の 1 ケース(max_turn=4000, W=100, 実行 5.4s, use_global_seen=off)のプロファイル。
ahc064 は enumerate_actions(候補生成)が重い問題なので、ユーザー側コストの比率は大きめに出ている。
実装済みの項目を反映する前の計測なので、次の判断の前に再計測すること。

| 区分 | Total(ms) | 全体比 | 備考 |
|---|---|---|---|
| try_op | 2210 | 約 41% | 6.30M 回, 平均 0.35µs |
| enumerate_actions の生成処理分 | 約 1666 | 約 31% | 4453 − 内側の計測点(try_op + push + fill)の合計 |
| cands_push (セグ木 set 込み) | 501 | 約 9% | 1.07M 回 |
| 木の処理 (update_tree + mark_and_sweep + sort + walk) | 約 480 | 約 9% | update_tree 308(うち mark_and_sweep 107)+ sort 49 ほか |
| leaf_action_copy / arena_fill | 54 / 76 | 約 2% | この State では Action が小さい |

注意: 候補 1 件ごとに入る計測点(try_op, cands_push など)には start/stop 約 8M 回分の
オーバーヘッド(推定 1〜1.5s)があり、その分は外側の計測点(enumerate_actions, expand_leaf)の
Total に上乗せされる。絶対値ではなく比率と Count を見ること。

### 計測から分かったこと

1. **try_op の呼び出し回数が支配的。** 6.30M 回のうち push に至るのは 1.07M 回(17%)で、
   残りは score >= INF などで捨てられている。呼び出し回数自体を減らす S2 の効果が最も大きい。
2. **push の受理率は 82% と高く、threshold による早期棄却はあまり効いていない。**
   受理 0.88M 件に対し最終的に残るのは約 0.26M 件で、受理後にビームから追い出される候補が
   約 7 割ある。入れ替わりが多いほどプール dict に削除されない key が溜まる
   (S9 の圧縮時 dict 再構築で実質解消できる)。
3. **木の処理は合計で全体の 9% 程度。** S4 は現時点では割に合わない。
   木が大きくなる問題(W 大、1 世代で進む turn 数が小さい)では比率が上がるので保留扱い。
4. **Action のコピーはこの State では軽い**(leaf_action_copy 54ms / arena_fill 76ms)。
   S5, M4 は Action が大きい問題向けとして残す。

### ライブラリ側のコスト内訳

計測からユーザー側のコスト(try_op 2210ms + enumerate_actions 約 1666ms ≒ 3.9s)を除いた、
ライブラリ側のコストの内訳。

| ライブラリ側のコスト | 規模(計測ケース) | 対応する提案 |
|---|---|---|
| cands_push(プール dict のプローブ + セグ木 set) | 約 501ms / 1.07M 回 | S9, M5(b) |
| update_tree の全ノード再構築 | 約 200ms | S4 (空世代スキップ実装済みのため再計測) |
| mark_and_sweep(新旧 tree と候補列の再走査) | 約 107ms | 解消済み (S3) |
| leaf_action_copy + arena_fill(Action のコピー) | 約 130ms | S5, M4 |
| cands_reset / build_segtree ほか | 小 | 解消済み (M2, M3) |

## 木の走査で同じ処理を繰り返している箇所

### 1 世代の中では重複はない

get_next_beam の walk は Euler tour を 1 回なめるだけで、

- この世代に展開する葉を含まない部分木は subtree_end で読み飛ばす(中のノードに触らない)
- 連続する展開葉の間は LCA までの rollback と apply だけで移動する

ため、apply/rollback の回数はこの世代の展開対象に対して最小になっている。

### 世代をまたぐと残っている重複

1. **共通祖先の apply の繰り返し。** ある内部ノードの部分木に target_turn の異なる葉が
   k 種類あると、そのノードの apply/rollback は k 世代にわたって繰り返される。
   マルチターン化で葉の成熟時期が分散するほど回数が増える(対策は見送り済み、
   「検討して見送った案」参照)。
2. **update_tree の全ノード再構築。** 変更のない部分木も毎世代コピーし直している(→ S4)。

(mark_and_sweep の再走査と展開しない葉への書き込みは S3 / S1b で解消済み)

## Action のコピーの現状

頻度の高いコピーは 2 箇所:

| 箇所 | 頻度 | 削減方法 |
|---|---|---|
| `Action action = act(node.aid);`(get_next_beam の葉展開時。submit 中の arena_put_reserve で action_pool が再確保されると参照が無効になるため、コピーで退避している) | 展開葉ごと = 世代あたり W 回 | S5(アドレスが安定すれば参照のままでよい) |
| `arena_put_fill(aid, action)` = `action_pool[slot] = a` | 受理された push ごと | M4(arena のスロット上で直接構築) |

低頻度のコピー(探索全体で O(turn) 回程度なので問題ない):

- `best_finished_action = action;`(完成解の更新時)
- update_tree の幹確定での `result.emplace_back(act(...))` / `state.apply_op(act(...))`
- get_result の経路復元(`result` への push/pop と `best_action`)

コピーしていない箇所:

- submit → process_candidate → try_op は `Action&` の参照渡し
- `BeamCandidate` / `new_candidates` は aid だけを持つ(Action は持たない)
- walk の apply/rollback も `act(aid)` の参照渡し

## 検討して見送った案

再提案を防ぐため、検討した上で採用しなかった案と理由を残す。

- **HashDict への削除(tombstone)の導入**: 不採用と決定(2026-06-11)。
  プール dict に死んだ key が溜まる問題は S9 の圧縮時 dict 再構築で実質解消できる。
- **内部ノードへの State スナップショット保存**(共通祖先 apply の繰り返し対策):
  不採用と決定(2026-06-11)。State へのコピー API 要求・メモリ・保存方針の設計が
  重いわりに、効く条件(apply 重・木深・成熟分散)が狭い。
  関連の「世代の終わりに root まで戻らず次世代の最初の展開葉へ LCA 経由で移動する」案も、
  幹確定と tree 再構築の絡みで複雑なわりに世代あたり経路 1 本分しか節約できないため見送り。
- **ポインタベースの木 / 木の in-place 編集**: 毎世代の再構築をなくせるが、
  walk がポインタの追跡になって局所性を失い、削除の穴を埋める圧縮も結局必要になる。
  Euler tour 配列の再構築は圧縮を兼ねており、S4(memcpy 化)の方が筋として上位互換。
- **世代内の展開順を score 順にする**: threshold が早く締まり、push の入れ替わりと
  try_op の通過数が減る。ただし展開順が木の順から外れると葉ごとに root からの
  apply が必要になり、walk のコストが跳ね上がる。親内の score 順 sort(現行の
  全体 sort の第 2 キー)がこの効果の安価な近似になっているので、それ以上は割に合わない。
  なお展開順は最終的な選抜結果には影響しない(threshold は単調にしか締まらないため)。
- **プール dict と seen_hash の統合**: 候補 1 件あたりのプローブを 2 回 → 1 回に
  できそうに見えるが、同じ hash が複数の target_turn のプールに同時に存在できる
  仕様のため、value の設計が複雑になり、プール解放時の整合も取れない。
- **並列化**: AtCoder では使用不可。
- **is_survived_node の bitset 化**: メモリは 1/8 になるがランダム書き込みが
  read-modify-write になる。byte 配列のままでよい。

## 計測の進め方

1. param.timestamp_meta が dt_expand(get_next_beam)と dt_update(update_tree)を
   世代ごとに記録しているので、まず両者の比率を実際の問題で確認する。
2. PROFILE を定義すると profiler の start/stop が有効になる(未定義なら完全に消える)。
   try_op 呼び出し数、push の受理/棄却数、dict の rebuild 回数を見ると
   S2/S9 の効果を事前に見積もれる。

## 互換性メモ

- S4, M5(b) は内部だけの変更で、結果もユーザー API も変わらない(出力 diff で検証できる)。
- S1a は同点の順序が変わるため探索結果(と実行時間)が変わる。
  コンテスト中の差し替えは避け、複数ケースで評価してから入れる。
- M1(i) も use_global_seen 有効時に探索結果が変わり得る
  (完成解を seen_hash で落とさなくなる方向)。
- S9 は最終的な選抜結果は同一(同点の順序を除く)だが、threshold の締まり方が
  変わるため try_op に渡る候補数は変わる。
- S2b/c, M4 の emplace submit は optional API の追加
  (`requires` で分岐するので既存コードはそのまま動く)。
