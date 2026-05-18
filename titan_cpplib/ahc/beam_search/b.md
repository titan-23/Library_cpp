# ライブラリ定数倍高速化メモ（beam_fast.cpp / candidates.cpp）

対象は問題コードではなくビームサーチ本体。ホットパスは
1. 毎ターンのツアー再構築での Action コピー
2. `Candidates::push` の segtree 上り
の2点。以下それぞれの方針。

---

## 1. Action プール化（arena ＋ 添字ハンドル）

### 現状の問題

beam_fast.cpp は `vector<Action> trace, tour, next_tour;` を持ち、各候補ごとに

- `next_tour.insert(next_tour.end(), trace+turn-lca_dist .. trace+turn+1)`
- `copy_tour_path(...)` の連続コピー
- 毎ターン `swap(tour, next_tour)` と全再構築

を行う。いずれも **sizeof(Action) 単位**でメモリを動かす。Action が大きい問題
（例 a.cpp は約48B）では、これが支配的コスト。総コピー量はおよそ O(E·sizeof(Action))。

### 方針

- append-only の `vector<Action> arena;` を1本持つ。Action 実体はここに1回だけ置く。
- `tour` / `trace` / `next_tour` を `vector<int>`（arena への添字）に変更。
- `apply_op` / `rollback` / コピー系は `arena[idx]` を介してアクセス。
- コピー・insert・swap が 48B → 4B になり、支配経路のトラフィックが約10倍減。
- 挙動は完全不変（純粋なメモリ表現変更）。

### 難所：arena の世代管理

- ターン内の LCA 走査で祖先 Action を `rollback` するため、**祖先 Action は安定して
  参照できる必要がある**。
- 設計案：世代付き arena。
  - 当ターン新規生成の Action は「今世代ブロック」に追記。
  - 祖先（前世代までに確定した経路上）の Action は前世代ブロックの添字で安定参照。
  - 確定（root 確定＝result 送り）した分は古い世代ブロックごと解放／使い回し。
- 添字は「世代内オフセット」または「グローバル連番＋世代基準」のどちらか。
  trace は turn で添字付けされているので、trace→arena 添字の対応表だけ持てばよい。

### リスク / 留意

- 効果大・挙動不変だが、世代管理の実装が中程度に複雑。
- Action が trivially copyable なら arena の確保・移動も memcpy 化できる。
  `static_assert(is_trivially_copyable<Action>)` を beam_fast の契約として明示すると
  最良コード。非 POD Action はもともと beam_fast の想定外。

---

## 2. Segtree 高速化（candidates.cpp `Candidates::push` / `set`）

### 現状の問題

```cpp
using T = pair<ScoreType, int>;
void set(int k, T v) {
    k += s;
    seg[k] = v;
    while (k > 1) {
        k >>= 1;
        seg[k] = seg[k<<1].first > seg[k<<1|1].first ? seg[k<<1] : seg[k<<1|1];
    }
}
```

- push 成功ごとに `set()` が `log w` 段上る。子総数 × log w が毎ターンの実コスト中心。
- `threshold()` は `seg[1].first`。try_op の早期枝刈りに使うので
  「online で worst を保持する」性質自体は捨てられない。
- seg[1] は worst の (score, slot) 両方を返す必要がある（eviction でスロット要）。
  よって segtree から index は外せない。論点は AoS（pair）か SoA（分離）かだけ。

### 訂正：pair をやめるのは無条件改善ではない（前案を撤回）

当初「pair を分離して POD 2本」を安全高速化として挙げたが、再検討の結論：

- **`ScoreType=int` では pair 維持が正解**。`pair<int,int>` は 8B 密パック。
  climb の `seg[k]=seg[2k].first>seg[2k+1].first?seg[2k]:seg[2k+1]` は -O3 で
  子2つを各8Bロード→`.first`比較→**8B を cmov→8Bストア**。既に分岐レス・
  1配列・連続・1キャッシュライン。非常にタイト。
- **分離（SoA）は climb で 2 ストリーム＝2キャッシュラインを触り、むしろ悪化**
  しがち。利点は `threshold()` が 4B 読みになる程度で L1 ヒットなので誤差。
- 分離が勝つのは限定条件のみ：`ScoreType` が大、または pair にパディングが出る
  とき（例 `int64_t`＋`int` で 16B＝4B パディング）。このとき SoA が移動量減・
  パディング消滅で勝つ。
- 結論：AoS/SoA は**型特性で選ぶ条件分岐**であり、無条件の安全高速化ではない。
  既定は pair 維持。

### 方針A（差し替え）：climb 早期終了（表現非依存・確実・挙動不変）

点更新が上位 max を変えないことは多い（満杯時、多くの push は最大スロットを
触らない）。親の再計算値が旧値と一致したら、その時点で打ち切る：

```cpp
while (k > 1) {
    k >>= 1;
    T nv = better(seg[k<<1], seg[k<<1|1]);
    if (nv == seg[k]) break;   // 以降の祖先も不変
    seg[k] = nv;
}
```

max-segtree の標準的早期終了。`threshold()`＝`seg[1]`、eviction＝`seg[1]` の
正しさを保ったまま、支配項 log w の**平均段数を実測で大きく削る**。
pair のまま入り、挙動完全不変。**安全版の本命はこれ**（旧 POD 分離案は撤回）。

### 方針B：大改造版（ScoreType が整数・範囲既知のとき O(1)）

- worst 取得・置換だけなので、整数 ScoreType かつ範囲が小さい／既知なら
  **バケット（単調キュー / バケット優先度キュー）**で push・worst 置換を
  O(1) 化し segtree を撤去。
- `threshold()` 早期枝刈りは最大バケット位置から O(1) で取得でき維持可能。
- ジェネリック性は型特殊化で担保：整数＋範囲ヒント→バケット、他→segtree。
- 効果大・変更大。方針A（早期終了）を入れて計測してから判断。pair 論争は二次。

### リスク / 留意

- 方針A（早期終了）は無リスク・挙動不変。
- AoS/SoA 切替を入れるなら型特性ベースの条件分岐として（無条件分離は不可）。
- 方針B は範囲ヒント API と特殊化分岐が要る。
- いずれも `threshold()` の意味（現 beam の worst）を変えないこと。崩すと
  try_op 側枝刈りと不整合。

---

---

## 確定設計：index 管理（議論済み）

### 世代付き arena とは

素朴案は両方ダメ：
- 単一の伸びるプール（解放なし）→ O(深さ·W) でメモリ破綻。
- 毎ターン reset して詰め直す → 祖先 Action 再挿入で O(E·sizeof(Action)) 復活。

要件は「再コピーしない（時間）」かつ「無限に溜めない（メモリ）」の両立。
世代付き arena ＝ 次の3不変条件：

1. **write-once**：ノードが採用された瞬間に Action をプールへ1回だけ書き、
   以後書換・移動しない。`tour/trace/next_tour/cand` は Action でなく int 添字を運ぶ。
   毎世代のツアー再構築は int 入替のみ。祖先は既存スロットを添字で再参照。
2. **世代ブロック割り**：プールを生成ターン t ごとのブロックで確保。
   ターン t 採用ノードの Action は block(t) へ。
3. **世代単位バルク解放**：そのブロックが不要になった瞬間ブロックごと free list へ。

### 添字付与タイミング

- get_actions の `actions` はスクラッチ（従来どおり実体）。
- try_op で memo を埋め、`candidates.push` で**採用された時点**でプールへ1回コピー。
  `BeamCandidate` は `Action` でなく `int action_slot` を持つ。
- 不採用候補はプールに入らない。書き込みは生存ノード1個につき1回だけ。

### 解放戦略は A に確定（速度優先）

- **戦略A（採用）**：root 確定でバルク解放。確定先頭一本道を `result` に送り、
  該当世代ブロックを丸ごと free。確定総長 ≤ 総ターン数 → **ならし O(1)/ターン**、
  連続・バルク・好局所性。
- 戦略B（不採用）：per-node 子カウンタ＋連鎖 free。毎ターン O(W) の
  散在スロット書込＋親ポインタ追跡でキャッシュミス。速度で A に劣る。
  利点は beam_fast 単体完結のみ。

裏取り：beam ノードは毎ターン新規、祖先は経路上にのみ存在し木は葉まで連結。
よって生存窓は常に [global-LCA, 現ターン] の連続区間。中間世代だけ全滅は
起きないので、解放可能なのは確定接頭辞のみ＝A が最良、かつ
**メモリ = 生存窓長 × W が原理的下限**。

### A 採用に伴う付随作業と特性

- beam_fast.cpp に **root 確定を追加**（現状なし）。global-LCA が前世代から
  d 前進したら祖先 d 本を `result` へ append、該当ブロック free、生存窓を d 前進。
  ならし O(1)/ターン。
- **副次メリット**：確定接頭辞を毎世代歩き直さなくなり、ツアー走査自体も高速化。
- **唯一の注意点（メモリのみ）**：多様性高で global-LCA が長く進まないと
  プールが 深さ×W まで膨張。速度は不変。必要になれば B 型枝刈りを
  A と直交に後付けオプション化可能（排他ではない）。

### 効果

- 従来：毎世代 O(E·sizeof(Action)) の再コピー。
- 本方式：生存ノードごと1回 O(sizeof(Action)) ＋ 毎世代 O(E·sizeof(int)) の添字入替。
  Action 48B / int 4B なら支配経路 約1/12。挙動完全不変。

### 変更箇所

- `BeamCandidate.action` → `int action_slot`（candidates.cpp、push でプールへ1回コピー）。
- beam_fast.cpp の `trace/tour/next_tour` → `vector<int>`、`apply_op(pool[id])` 等。
- プール本体：世代ブロック ＋ free list。`best_finished_path` は確定時に実体コピー。
- beam_fast へ root 確定ロジック追加。

---

## 3. get_actions / try_op 一体化（sink）

### 現状の問題

`get_actions` が分岐を全部 `actions` に push_back し、呼び出し側が回して
try_op→大半を threshold/dedup で破棄。1 action ごとに sizeof(Action) の
バッファ書込→読戻し→大半廃棄。確保は warmup 後ゼロだが、このトラフィックが
Action 大・分岐広の問題で支配的。

### 方針：生成ループと評価ループを1本に畳む

try_op は State メソッドのまま。ライブラリが渡す emitter から呼ぶ。中間
`actions` バッファだけ撤去。State 側は単一 emitter を受ける template に変更：

```cpp
template<class Emit>
void get_actions(int turn, const Action& last_action, Emit&& emit) {
    Action a;                       // 1個持ち回す。毎回構築しない
    for (各分岐) {
        a.<fields> = ...;
        if (cheap_lb(a) >= emit.threshold()) continue;  // 任意の早期枝刈り
        emit(a);                    // try_op + INF/finished 判定 + push + history
    }
}
```

- threshold は引数で渡さず `emit.threshold()` がライブの最新 worst を返す。
  push が進むたび縮むのでスナップショットより枝刈りが強い。
- `emit(a)` は lvalue 参照。push が move で抜く。Action は trivially
  copyable 契約なので move==copy、`a` は使い回せる。1 action 構築ゼロ。

### emitter：ライブラリ側ネスト構造体

`candidates`/`history`/`found_finished` 等は BeamSearchWithTree 内なので
参照を持つ軽量 POD。`threshold()` を名前付きメソッドで出すため lambda でなく
構造体。

```cpp
struct Emitter {
    BeamSearchWithTree& bs;
    int parent_leaf, parent_node_id, turn_label;
    inline ScoreType threshold() const { return bs.candidates.threshold(); }
    inline void operator()(Action& a) {
        ScoreType th = bs.candidates.threshold();
        auto [score, hash, finished] = bs.state_ref.try_op(a, th);
        if (score >= INF) { /* record: status 2 */ return; }
        if (finished)     { /* best 更新 + record: status 0 */ return; }
        /* else: push(score,hash,parent_leaf,a,nid) + record: ok?0:1 */
    }
};
```

operator() は今のループ本体をそのまま移す。`if constexpr (record_history)`
ブロックも丸ごと移植。呼び出し側はループが消える：

```cpp
int now_leaf_idx = next_leaf.size();
Emitter emit{*this, now_leaf_idx, c.node_id, turn + 1};
state.get_actions(turn, c.action, emit);
next_leaf.push_back(next_tour.size());
```

Phase1 は `Emitter emit{*this, 0, -1, 1};` で同形。

### 挙動完全不変の保証

- `node_id_counter` は emit 呼出順＝生成順に増える。旧コードの push_back 順と
  同一なので record true/false の tie 順・node_id 付与順は不変。既存の単一
  sort 設計と完全整合。
- 契約：emit を呼ぶ順序は旧 get_actions の push_back 順と同じ。同じ生成
  ループなので自明に満たす。
- threshold は emit 内で毎回読む＝旧 per-action `candidates.threshold()` と
  同鮮度。

### overhead 会計

| | 現状 | 一体化後 |
|---|---|---|
| actions バッファ書込 | sizeof(Action)/action | 0 |
| 読み戻し | sizeof(Action)/action | 0 |
| actions.clear / vector 機構 | 毎ノード | 撤去 |
| try_op 呼出 | 1回/action | 1回/action 不変 |
| 関数境界 | get_actions + ループ | emit は Emit&& template で完全インライン |

compute 不変、メモリトラフィックのみ純減。48B Action なら約 96B/action が
ホットパスから消えレジスタ渡し参照に。`std::function` は不可、`Emit&&`
template で静的ディスパッチし try_op まで get_actions にインライン。

### 後方互換

`-std=c++20` の requires で分岐：

```cpp
if constexpr (requires { state.get_actions(turn, last, emit); }) {
    // 新パス
} else {
    actions.clear();
    state.get_actions(actions, turn, last, candidates.threshold());
    for (auto& a : actions) { /* 旧ループ */ }
}
```

旧シグネチャの State は無改修。新パスはオプトイン。機能的損失は「action
一覧の事前ソート/間引き不可」のみ。必要な State は旧パス継続。

### リスク / 留意

- get_actions の template メソッド化。ヘッダ方式の本リポジトリでは問題なし。
- emitter は参照メンバの POD。寿命は get_actions 呼出中のみ。alloc なし。
- 契約明記：emit を retain しない／emit 後に同じ Action 使い回し可。
- arena 添字化と直交。両方入れると push は実体でなく添字1回コピーになり
  さらに軽い。

---

## 優先度まとめ

| 項目 | 効果 | リスク | 挙動 |
|---|---|---|---|
| 1 Action arena 添字化 | 最大 | 中（世代管理） | 不変 |
| 3 get_actions/try_op 一体化 | 大 | 中（API変更・オプトイン） | 不変 |
| 2A segtree climb 早期終了 | 中 | 低 | 不変 |
| 2B segtree バケット化 | 大 | 大 | 不変 |

着手順の候補：2A を先に入れて計測 → 3 sink 化 → 1 を本命として実装 →
必要なら 2B。1 と 3 は直交で併用可。
