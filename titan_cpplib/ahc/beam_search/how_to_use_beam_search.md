## ビームサーチ実装ガイド

### 0. ライブラリ概要

`flying_squirrel::BeamSearchWithTree` は木上の差分更新型ビームサーチ。

DFS オイラーツアー型で、各ターンで以下のように木を辿る。

```
state->init()                                  // 初回1回だけ

for each turn:
    for each node in beam tree (DFS順):
        PRE_ORDER  : apply_op(action)          // 子へ進む
        LEAF       : get_actions(turn, last, emit)   // 合法手を生成し1手ごとに
                     // emit(action) -> try_op + 判定 + 候補push を内部実行
        POST_ORDER : rollback(action)          // 親へ戻る
```

`apply_op` と `rollback` は完全に対称であること。同一の `action` に対して `apply_op → rollback` を呼ぶと状態が完全に元へ戻る必要がある。

情報は `Action` を介して流れる。`get_actions` が手を生成して `emit(action)` を呼ぶと、ライブラリが `try_op` を呼ぶ。`try_op` は `State` を変更せずスコアとハッシュを計算して `action` に `pre_*` 等を書き込み、生き残った枝で `apply_op` がそれを参照して差分更新し、`rollback` が `pre_*` で差分を巻き戻す。`try_op` の `const` は `State` を変えない意味で、書き込み先は `action`。各関数の詳細は Section 2。

前提

- `State` オブジェクトは探索を通して 1 つだけ生成され、ディープコピーは発生しない。
- スコアは最小化で扱う。最大化問題はスコアを負にして返す。
- `try_op(action)` が返す score は、その候補をビームで選ぶための評価値。累積スコアそのものでも、累積しない一時的な評価を加えた値でもよい。
- `State` が保持する累積スコアは `try_op` の評価値を計算するための状態。`apply_op`/`rollback` で正しく差分更新・復元する。
- `try_op(action)` が返す hash は、その同じ action を `apply_op` した直後の状態を表す値にする。内部の計算を現在値からの XOR で行うのは構わないが、返す hash は差分ではなく次状態の値。
- 関数のシグネチャは変更しない。

---

### 0.5 セットアップ

#### 使用ファイル

| include するファイル | 役割 |
|---|---|
| `titan_cpplib/ahc/beam_search/beam_search.cpp` | 木・差分・インプレース更新版。`State` は1個、コピー無し。`search()` を提供 |
| `beam_param.cpp` | `BeamParam`。`beam_search.cpp` から自動 include されるので個別 include 不要 |

#### 自分で定義すべき記号

ライブラリ側では定義しない。`State` を置く名前空間内で宣言する。`ScoreType`・`HashType`・`INF` はテンプレート引数として渡すので未定義だとコンパイルできない。

| 記号 | 例 | 意味 |
|---|---|---|
| `ScoreType` | `using ScoreType = int;`(`long long`/`double`) | スコア型。最小化。最大化問題は符号反転 |
| `HashType` | `using HashType = uint64_t;` | Zobrist ハッシュ型 |
| `INF` | `const ScoreType INF = 1e9;` | `try_op` 打ち切り時の番兵。到達しうる任意の実スコアより大きく、`ScoreType` でオーバーフローしない値にする。実スコアが `INF` 以上になる設計は不可。`-INF` が `ScoreType` で表現できること(内部で番兵に使用) |
| `brnd` | `titan23::Random brnd;` | Zobrist 乱数用にユーザーが使う乱数器。`brnd.rand_u64()`。`random.cpp` を include |

#### 最小テンプレート(これをコピーして State 中身だけ書く)

```cpp
#include <bits/stdc++.h>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/timer.cpp"
#include "titan_cpplib/algorithm/random.cpp"
#include "titan_cpplib/ahc/beam_search/beam_search.cpp"            // search() 本体
using namespace std;

namespace beam_search {
    using ScoreType = int;                 // 最小化
    using HashType  = unsigned long long;
    const ScoreType INF = 1e9;
    titan23::Random brnd;                  // brnd.rand_u64()

    struct Action {
        // 操作パラメータ + pre_/nxt_* (Section 2)
        friend ostream& operator<<(ostream& os, const Action&){ return os; }
        string to_string() const { return ""; }
    };
    class State { /* init/try_op/apply_op/rollback/get_actions(emit)/print/get_state_info */ };

    vector<Action> search(flying_squirrel::BeamParam &param, const bool verbose=false, const string& history_file = "") {
        flying_squirrel::BeamSearchWithTree<ScoreType, HashType, Action, State, INF> bs;
        return bs.search(param, verbose, history_file);
    }
}

int main() {
    flying_squirrel::BeamParam param(/*max_turn=*/2000, /*beam_width=*/100,
                                     /*time_limit_ms=*/-1, /*is_adjusting=*/false);
    auto res = beam_search::search(param);
    for (auto &a : res) { /* a を根→葉の順に適用して解を再生・出力 */ }
}
```

#### `BeamParam` 仕様

コンストラクタ:
`flying_squirrel::BeamParam param(int max_turn, int beam_width, double time_limit_ms, bool is_adjusting=false, bool clear_hash_every_turn=true, int hash_window_turns=0);`

| 引数 | 型/単位 | 既定 | 意味 |
|---|---|---|---|
| `max_turn` | int。ターン＝木の深さ | — | 探索木の最大深さ。これを超えると打ち切り。1手1ターンなら最大手数 |
| `beam_width` | int | — | ビーム幅。`is_adjusting=true` のときは実効幅の基準値(base 版では超えて増えうる。turn 版では上限) |
| `time_limit_ms` | double, ms | — | `is_adjusting=true` 時に実効幅を見積もる時間予算。`Timer::elapsed()` と同単位で1秒1000。ハードな打ち切りではない。探索ループは時間では止まらず、`max_turn` か `finished` まで走る。`is_adjusting=false` のときは無視される |
| `is_adjusting` | bool | `false` | `true` で残り時間予算から実効幅を動的に増減する。実行時間そのものを制御するわけではない。通常は `false` 推奨 |
| `clear_hash_every_turn` | bool | `true` | 毎ターン重複排除 hash を clear する安全既定。`false` はターン跨ぎ最適化用で、`State` の hash がターン情報を含まないと、前のターンの entry と重複扱いになって候補が捨てられる。理解した上でのみ使う |
| `hash_window_turns` | int | `0` | `clear_hash_every_turn=false` のときだけ有効。`>0` なら K ターンごとに hash dict を全 clear し、蓄積を抑える。`0` は無制限蓄積。推奨は 0 か 1。base 版のみ有効(turn 版では無視される) |

`is_adjusting=false` で使うときは実行時間が `time_limit_ms` で制御されないので、実行時間は `max_turn` と `beam_width` で合わせ込む。時間内に収めたいなら `is_adjusting=true` にして `time_limit_ms` に予算を渡すが、これも幅を調整するだけでループ自体は `max_turn` まで走る。

#### 戻り値の意味

`search()` は根から最良葉(`finished` 優先・最小スコア)までの `Action` 列を、`apply_op` すべき順序で返す。
解の再生は先頭から順に適用するだけ。途中状態の再構築は不要。

既定では `max_turn` ターンまで進むため、`finished` に到達しなければ戻り値の長さは `max_turn` になる。`finished` で打ち切られたときだけ、その時点までの短い列になる。

---

### 1. 実装上の制約と高速化の指針

Action 構造体のサイズ最小化
- `Action` は毎ターン多数コピー・生成される。ロールバックに必要な情報のみを持ち、型は必要十分な最小サイズを選ぶ。`int` のかわりに `short` や `int8_t` で済むなら縮める。

`score` / `hash` を `Action` に持たせるかの判断
- 持たせる: `try_op` で計算した値を `action.nxt_*` に保存し、`apply_op` では `score = action.nxt_score` するだけ。実装が単純だが `sizeof(Action)` が増える。
- 持たせない: `apply_op` 内で操作パラメータから再計算する。`Action` は小さくなるが、`try_op` と `apply_op` で計算が二度走る。
- 判断基準: 差分計算コストが軽いなら「持たせない」、それ以外は「持たせる」が無難。

スコア計算の早期打ち切り
- `try_op` に渡される `threshold` は現在のビーム内の最悪スコア。ビームが `beam_width` 個埋まるまでは `threshold == INF`。
- 差分計算の途中で次のスコアが `threshold` 以上になると確定した時点で `{INF, 0, false}` を返す。
- この場合その `Action` が `apply_op` されることはないので、ロールバック情報の書き込みも省く。
- スコアを大きく変化させる要因から先に計算し、`threshold` 超えを早く検出する。

`get_actions` 側でスコアまで計算する選択肢
- `try_op` の差分計算が `get_actions` の合法手生成中に参照する情報をそのまま使えるなら、`get_actions` 内でスコアまで計算して `Action` に保存する方が速いことがある。`try_op` は保存値を返すだけになり、データ構造への二重アクセスを避けられる。
- この場合でも `emit.threshold()` での枝刈りは emit 前に行う。

ホットパス `try_op` / `get_actions` でのコピー回避
- ビームサーチ中で最も多く呼ばれる箇所。
- `State` メンバの一時コピーは禁止。`auto tmp_board = board;` のような盤面コピーが該当する。
- スコア・ハッシュの計算はスカラーのローカル変数だけで完結させる。
- 内部で `vector` を生成・コピーしない。ヒープアロケーション全般を禁止。

Zobrist Hash の設計
- XOR による差分更新を実装する。
- ライブラリは同一ハッシュ値の候補を同一状態とみなし、スコアの良い方のみ残す。
- ハッシュ化する対象は「その後の状態遷移やスコア計算に影響する要素」のみに限定する。無関係な情報を含めると同一視すべき状態が別物として扱われ、ビームの多様性が落ちる。
- 乱数テーブルは `brnd.rand_u64()` で事前に初期化する。

状態の差分更新
- `State` 内の盤面配列などのディープコピーは禁止。変更箇所のみ計算・更新する。

`get_actions` での枝刈り
- 悪化手・逆操作を emit しない。良さそうな手を先に emit して `emit.threshold()` を早く下げる。詳細は Section 2 の `get_actions`。

---

### 2. 各関数の仕様と実装要件

#### Action 構造体

- 状態遷移を表す「操作パラメータ」と、`rollback` に必要な「巻き戻し情報」を一体で保持する。
- スケルトン内のメンバ変数は一例。問題に応じて自由に変更・追加してよい。
- `try_op` で上書きされる前の値 `pre_*` と、`apply_op` 後の値 `nxt_*` を持つのが基本パターン。詳細は Section 1「持たせるか判断」。
- `string to_string() const` はデバッグ出力用(テンプレート第6引数 `record_history`、既定 `false`。`true` で探索履歴を `search()` の `history_file` へ JSON 出力)。中身は空でよいが、削除はしない。

#### `State::init`

- シグネチャ: `void init()`
- 探索開始時の初期状態を構築する。`search()` から 1 回だけ呼ばれる。
- 盤面・スコア・ハッシュ値を初期化する。標準入力からの問題データ読み込みもここで行う。
- Zobrist Hash 用の乱数テーブルは一度だけ初期化する。`State` は探索を通して1個なのでメンバでも `static` でもよいが、`init` が複数回呼ばれてもテーブルを作り直さないよう初期化フラグで一度きりにする。盤面サイズに依存しない巨大テーブルなら `static` にすると再生成を避けられる。

#### `State::try_op`

- シグネチャ: `tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const`
- 現在の状態に操作を適用した場合の次状態スコア・ハッシュ値・終了フラグを計算する。
- `const` 関数なので `State` のメンバは変更しない。書き込んでよいのは引数 `action` だけ。score は候補の評価値、hash は次状態を表す値を返す。どちらも差分値ではない。
- 早期打ち切りは Section 1 に従う。次スコアが `threshold` 以上だと確定した時点で `{INF, 0, false}` を返す。INF を返した手は `apply_op` されないので、ロールバック情報を `action` に書く必要はない。通す手についてだけ書く。
- 戻り値 `finished` は、この操作で「最終解として確定してよいゴール状態」に達したか。終了条件が無い問題では常に `false`。`finished` の手は候補に積まれず展開されない。ライブラリは `finished` が初めて出たターンで探索を打ち切り、そのターン内に複数あれば最小スコアのものを最終解にする。それより深いターンは探索しないので、浅い `finished` が最良とは限らない問題では付ける条件に注意。
- ホットパス。詳細は Section 1。

#### `State::apply_op`

- シグネチャ: `void apply_op(const Action &action)`
- `State` をインプレース更新して次の状態へ進める。
- `try_op` を通過した合法な `Action` に対してのみ呼ばれるので、再バリデーションは不要。
- `action` に `nxt_*` を保存しているならそれを使い、保存していないなら操作パラメータから差分計算する。いずれにせよ変更箇所のみ更新する。

#### `State::rollback`

- シグネチャ: `void rollback(const Action &action)`
- `action` の `pre_*` を使って `apply_op` 前の状態に完全に戻す。
- `apply_op` と完全に対称な逆操作にする。
- 盤面全体ではなく差分のみ巻き戻す。

#### `State::get_actions`

- シグネチャ: `template<class Emit> void get_actions(const int turn, const Action &last_action, Emit &&emit) const`
- 現在の状態で実行可能な合法手を生成し、1手ごとに `emit(action)` を呼ぶ。中間の `vector<Action>` を作らないのでバッファ確保とコピーが発生しない。
- `emit(a)` を呼ぶだけで `try_op`、`INF`/`finished` 判定、候補への push まで内部で実行される。`try_op` を自分で呼ぶ必要はない。
- `a` は1個を使い回してよい。push に必要な分は emit 側がコピーし、`try_op` が `a` に書くロールバック情報の扱いも emit 側が行う。
- `emit.threshold()` は現在のビーム最悪スコアを返す。push が進むたびに縮む最新値なので、列挙ループ内の枝刈りに使える。ビームが `beam_width` 個埋まるまでは `INF` を返す。`try_op` 側の早期打ち切りに任せてもよい。
- `last_action` はターン 0 の呼び出しではダミー値。`turn == 0` のときは参照しない。
- 明らかにスコアが悪化する手や、直前操作を単純に打ち消すだけの逆操作は emit しない。
- 良さそうな手を先に emit すると `emit.threshold()` が早く下がり、後続の手の枝刈りや `try_op` の早期打ち切りが効きやすくなる。
- あるターンで生き残った全ノードがスコア有限の手を1つも emit しないと、候補が空になり `assert` 停止する(turn 版の挙動は Section 3)。行き止まりノードは何も emit しなければ枝が消えるだけだが、全ノードが同時に詰む問題では `max_turn` を到達可能な深さに合わせる。`finished=true` はゴール到達の表現に使い、行き止まりには使わない。
- ホットパス。詳細は Section 1。

```cpp
template<class Emit>
void get_actions(const int turn, const Action &last_action, Emit &&emit) const {
    Action a;
    for (char c : "UDLR") {
        if (turn != 0 && is_reverse(c, last_action)) continue; // 逆操作は出さない
        if (!in_bounds(c)) continue;
        a.d = c;
        emit(a);            // try_op + 判定 + push を内部で実行
    }
}
```

#### `State::print`

- シグネチャ: `void print() const`
- デバッグ用に現在の状態を標準エラー出力に表示する。人間が確認できる内容にする。`print` のためだけのメンバを `State` に追加しない。

#### `State::get_state_info`

- `record_history=true` 時の JSON ログ出力用。不要なら `return "{}";` でよい。

#### `search` 関数

0.5 の最小テンプレート内のラッパをそのまま使う。変更不要。

---

### 3. 可変深さ版 `beam_search_turn`

操作で進む論理ターン数が手ごとに異なる問題で使う。各 `Action` が自分の `target_turn`(到達する論理ターン番号)を持ち、その値ごとに別プールへ振り分けられる。同じ呼び出し内で異なる `target_turn` の手を混在 emit してよい。固定深さ問題では `beam_search.cpp` を使う。

#### base 版との差分(これ以外は完全に同一)

| 項目 | base (`beam_search.cpp`) | turn 版 (`beam_search_turn.cpp`) |
|---|---|---|
| include | `beam_search.cpp` | `beam_search_turn.cpp` |
| `Action` 必須メンバ | なし | `int target_turn;`(sentinel `-1`) |
| `try_op` 第2引数 | `ScoreType threshold` | `const vector<ScoreType>& thresholds` |
| `get_actions` 第1引数 | `const int turn` | なし(`last_action` で置換) |
| `emit.threshold` 呼び方 | `emit.threshold()` | `emit.threshold(int target_turn)` |
| `BeamParam::max_turn` | 木の深さ上限 | `target_turn` の最大値 |
| ビーム重複排除粒度 | per-turn | per-`target_turn` |
| `is_adjusting=true` の実効幅 | `beam_width` を超えうる | `beam_width` が上限 |
| `hash_window_turns` | 有効 | 無視される |
| 候補が空になったとき | `assert` 停止 | 停止せず、その時点までの部分解を返す |

クラス(`BeamSearchWithTree`)、`init`/`apply_op`/`rollback`/`print`/`get_state_info`、上表以外の `BeamParam` フィールド、戻り値の意味、Section 1 の高速化指針、Section 2 の対称性・score/hash・finished の扱いは base 版と同じ。

#### `Action::target_turn` の決定

- どのプールに候補を入れるかのキー。重複排除も同じプール内で行われる。
- 通常は `try_op` 内で操作パラメータから計算して `action.target_turn` に書き込む。`get_actions` で先に決めてもよい。
- emit 時点の論理ターン(＝親の `target_turn`)より真に大きく、`max_turn` 以下にする。親以下の値を入れた候補は展開されないまま捨てられ、不正に短い解が返る原因になる。`-1` は root sentinel 専用。
- `target_turn > max_turn` の候補は、emit してもエラーにならず捨てられる。`max_turn` は到達しうる上限以上に設定する。プールは実際に使う `target_turn` の分しか確保されないので、大きめにしてもメモリはほとんど増えない。

#### `try_op` の差分

- `thresholds[action.target_turn]` がそのプールの現最悪スコア。
- 早期打ち切りは『`target_turn` を確定してから `thresholds[t]` と比較』の順。逆だと無駄な差分計算が走る。
- 確定 reject 時に `{INF, 0, false}` を返す・ロールバック情報を書かない、は base と同じ。

#### `get_actions` の差分

- `turn` 引数なし。現在の論理ターンが要るなら `State` のメンバで自前で保持する(`apply_op`/`rollback` で更新)。
- root 判定の慣用は `last_action.target_turn == -1`。base 版の `turn == 0` 相当(`DUMMY_ACTION` に -1 が入っている)。
- 1 回の呼び出しで異なる `target_turn` を持つ手を混在 emit してよい。各 emit ごとに対応プールに振り分けられる。
- `emit.threshold(t)` で `target_turn==t` のプールの最悪スコアが取れる。`target_turn` が emit 前に確定しているならこれで先に枝刈りできる。

#### 最小スケルトン差分(0.5 のテンプレからの diff)

```cpp
#include "titan_cpplib/ahc/beam_search/beam_search_turn.cpp"  // ← turn 版

struct Action {
    int target_turn;                              // 必須メンバ
    // 操作パラメータ + pre_/nxt_* (Section 2)
    Action() : target_turn(-1) {}                 // -1 = root sentinel
    friend ostream& operator<<(ostream& os, const Action&){ return os; }
    string to_string() const { return ""; }
};

class State {
    /* init / apply_op / rollback / print / get_state_info は base と同じ */

    tuple<ScoreType, HashType, bool>
    try_op(Action &action, const vector<ScoreType>& thresholds) const {
        // 1. action.target_turn を計算して書き込む
        // 2. thresholds[action.target_turn] と比較して早期 reject
        // 3. 通った手だけロールバック情報を action に書く
    }

    template<class Emit>
    void get_actions(const Action &last_action, Emit &&emit) const {
        // last_action.target_turn == -1 なら root 呼び出し
        // 必要なら emit.threshold(t) で先に枝刈りしてから emit(a)
    }
};
```

#### 注意点

- `target_turn` のステップ幅は一定でなくてよい。同じ呼び出しで遠い/近い target が混ざってよい(ただし各候補は親の `target_turn` より真に大きいこと)。
- `clear_hash_every_turn=false` は全 `target_turn` 跨ぎで hash dedup する。新規候補は「未登録／既登録より小さい `target_turn`／同じ `target_turn` で低スコア」のときだけ通る。既に大きい `target_turn` のプールに居る同 hash entry は除去されない。hash 自体は base と同じく状態を表すだけでよく、`target_turn` を含める必要はない。
- `max_turn` が小さすぎると、それを超える手が捨てられて探索の幅が狭くなる。最初は大きめにして、ログの `target_turn` 分布を見てから絞るのが安全。
- 戻り値は base と同様、根 → 最良葉(`finished` 優先・最小スコア)までの `Action` 列。長さは finished 到達時のその直前 target_turn まで、または木内の最小 `target_turn` が `max_turn` に達するまで進めた時点の最良パス長になる。
