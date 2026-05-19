## ビームサーチ実装ガイド

### 0. ライブラリ概要

`flying_squirrel::BeamSearchWithTree` は 木上の差分更新型ビームサーチです。

ライフサイクルは DFS オイラーツアー型で、各ターンで以下のように木を辿ります。

```
state->init()                                  // 初回1回だけ

for each turn:
    for each node in beam tree (DFS順):
        PRE_ORDER  : apply_op(action)          // 子へ進む
        LEAF       : get_actions(turn, last, emit)   // 合法手を生成し1手ごとに
                     // emit(action) -> try_op + 判定 + 候補push を内部実行
        POST_ORDER : rollback(action)          // 親へ戻る
```

`apply_op` と `rollback` は完全に対称でなければなりません。すなわち、同一の `action` に対して `apply_op → rollback` を呼ぶと状態が完全に元へ復元される必要があります。

情報は `Action` を介して流れます。`get_actions` が手を生成して `emit(action)` を呼ぶと、ライブラリが `try_op` を呼びます。`try_op` は `State` を変更せずスコアとハッシュを計算して `action` に `pre_*` 等を書き込み、生き残った枝で `apply_op` がそれを参照して差分更新し、`rollback` が `pre_*` で差分を巻き戻します。`try_op` の `const` は `State` を変えない意味で、書き込み先は `action` です。各関数の詳細は Section 2。

重要な前提

- `State` オブジェクトは探索を通して 1 つだけ生成され、ディープコピーは発生しません。
- スコアは**最小化**で扱われます。最大化問題はスコアを負にして返してください。
- **最重要の不変条件**。`try_op(action)` が返す score と hash は、その同じ action を `apply_op` した直後に State が保持する score と hash に完全に一致しなければなりません。ビームは `try_op` の値で残す枝を選び、最終解はその枝を `apply_op` で再生して作ります。両者がズレると選んだ枝と出力される解が食い違い、スコアが申告値と合わなくなります。これと `apply_op`/`rollback` の対称性が差分更新の正しさの核です。
- `try_op` が返し `apply_op`/`rollback` が保つ score と hash は、差分ではなくその状態の累積した絶対値です。内部の計算を現在値からの XOR や加算で行うのは構いませんが、返す値と保持する値は常に絶対値にしてください。
- 関数のシグネチャは絶対に変更しないでください。`get_actions` は emit 方式のシグネチャで実装します（Section 2）。

---

### 0.5 セットアップ(最小テンプレート / ここだけで初回統合が完結する)

#### 使うファイル(include 索引)

| include するファイル | 役割 |
|---|---|
| `titan_cpplib/ahc/beam_search/beam_search.cpp` | これを使います。木・差分・インプレース更新版で `State` は1個、コピー無し。`search()` を提供 |
| `beam_param.cpp` | `BeamParam`。`beam_search.cpp` から自動 include されるので個別 include 不要 |

#### 自分で定義すべき記号

ライブラリは供給しません。`State` を置く名前空間内で宣言します。`ScoreType`・`HashType`・`INF` はテンプレート引数として渡すので未定義だとコンパイル不可です。

| 記号 | 例 | 意味 |
|---|---|---|
| `ScoreType` | `using ScoreType = int;`(`long long`/`double`) | スコア型。**最小化**。最大化問題は符号反転 |
| `HashType` | `using HashType = uint64_t;` | Zobrist ハッシュ型 |
| `INF` | `const ScoreType INF = 1e9;` | `try_op` 打ち切り時の返り値＝番兵。**到達しうる任意の実スコアより必ず大きく**、かつ `ScoreType` でオーバーフローしない値。実スコアが `INF` 以上になる設計は不可(候補が黙って捨てられる)。`-INF` に対応しうる必要がある。 |
| `brnd` | `titan23::Random brnd;` | Zobrist 乱数用にユーザーが使う乱数器。`brnd.rand_u64()`。`random.cpp` を include。 |

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

    struct Action { /* 操作パラメータ + pre_*/nxt_* (Section 2) */
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
`flying_squirrel::BeamParam param(int max_turn, int beam_width, double time_limit_ms, bool is_adjusting=false, bool clear_hash_every_turn=true);`

| 引数 | 型/単位 | 既定 | 意味 |
|---|---|---|---|
| `max_turn` | int。ターン＝木の深さ | — | 探索木の最大深さ。これを超えると打ち切り。1手1ターンなら最大手数 |
| `beam_width` | int | — | ビーム幅。`is_adjusting=true` のときは初期値兼基準値で、実効幅はこれを超えても増える。上限ではない |
| `time_limit_ms` | double, ms | — | `is_adjusting=true` 時に実効幅を見積もる時間予算。`Timer::elapsed()` と同単位で1秒1000。**ハードな打ち切りではない。探索ループは時間では止まらず、`max_turn` か `finished` まで必ず走る。`is_adjusting=false` のときは完全に無視される** |
| `is_adjusting` | bool | `false` | `true` で残り時間予算から実効幅を動的に増減。`beam_width` は上限ではなく基準値。実行時間そのものを制御するわけではない点に注意。通常は `false` 推奨 |
| `clear_hash_every_turn` | bool | `true` | 毎ターン重複排除 hash を clear する安全既定。`false` はターン跨ぎ最適化用で、`State` の hash がターン情報を含まないと候補が黙って drop される。理解した上でのみ使用 |

`is_adjusting=false` で使うときは実行時間が `time_limit_ms` で制御されないので、実行時間は `max_turn` と `beam_width` で合わせ込んでください。時間内に収めたいなら `is_adjusting=true` にして `time_limit_ms` に予算を渡しますが、これも幅を調整するだけでループ自体は `max_turn` まで走ります。

#### 戻り値の意味

`search()` は **根 → 最良葉(`finished` 優先・最小スコア)までの `Action` 列**を `apply_op` すべき順序で返す。
解の再生は `for (auto &a : res) apply 相当の処理` で先頭から順に適用するだけ。途中状態の再構築は不要。

**戻り値の長さ**。ライブラリは既定では常に `max_turn` ターンまで進むため、`finished` 状態に到達しなければ戻り値の長さは `max_turn` になります。1手1ターンなら手数がそのまま `max_turn` です。`finished` でより早く打ち切られたときだけ、その時点までの短い列になります。`res.size()` が想定と違うときは、まず `max_turn` の値と `finished` 条件の実装を疑ってください。

---

### 1. 実装上の制約と高速化の指針

Action 構造体のサイズ最小化
- `Action` は毎ターン多数コピー・生成されます。ロールバックに必要な情報のみを持ち、変数の型は必要十分な最小サイズを選んでください。例えば `int` のかわりに `short` や `int8_t` で済むなら積極的に縮めてください。

`score` / `hash` を `Action` に持たせるかの判断
- **持たせる**: `try_op` で計算した値を `action.nxt_*` に保存し、`apply_op` では `score = action.nxt_score` するだけ。実装が単純だが `sizeof(Action)` が増える。
- **持たせない**: `apply_op` 内で操作パラメータから再計算する。`Action` は小さくなるが、`try_op` と `apply_op` で計算が二度走る。
- **判断基準**: 差分計算コストが極めて軽く数命令で済むなら「持たせない」が有利。それ以外は「持たせる」が無難。

スコア計算の早期打ち切り — 最重要
- `try_op` に渡される `threshold` は現在のビーム内の最悪スコアです。ビームが `beam_width` 個埋まるまでは `threshold == INF`(枝刈り不発)なのは正常で、序盤に効かなくても問題ありません。
- 差分計算の途中で次のスコアが `threshold` 以上になることが確定した時点で、即座に `{INF, 0, false}` を返してください。
- この場合その `Action` が `apply_op` されることはないため、`Action` へのロールバック情報の書き込みもスキップしてください。
- スコアを大きく変化させる要因から先に計算し、`threshold` 超を早めに検出してください。

`get_actions` 側でスコアまで計算する選択肢
- `try_op` の差分計算が `get_actions` の合法手生成中に参照する情報をそのまま使えるなら、`get_actions` 内でスコアまで計算して `Action` に保存してしまう方が速いことがあります。`try_op` は保存値を返すだけになり、データ構造への二重アクセスを避けられます。
- この場合でも `emit.threshold()` を使った枝刈りは emit する前に行ってください。

ホットパス `try_op` / `get_actions` でのコピー回避
- ビームサーチ中に最も多く呼ばれる箇所です。
- `State` メンバの一時コピーは禁止です。例えば `auto tmp_board = board;` のような盤面のコピーが該当します。
- スコア・ハッシュの計算はスカラーのローカル変数だけで完結させてください。
- 内部で `vector` を新たに生成・コピーすることも避けてください。ヒープアロケーション全般を禁止します。

Zobrist Hash の設計
- `XOR` 演算による差分更新を実装してください。
- ライブラリは同一ハッシュ値の候補を **「同一状態」とみなし、スコアの良い方のみ残します**。これにより重複した部分木の展開が防がれ、ビーム幅を実効的に増やせます。
- ハッシュ化する対象は「その後の状態遷移やスコア計算に影響を与える要素」のみに限定してください。無関係な情報をハッシュに含めると本来同一視すべき状態が別物として扱われ、ビーム多様性が損なわれます。
- 乱数テーブルは `uint64_t` の xorshift 系乱数で事前に初期化してください。`brnd.rand_u64()` が利用できます。

状態の差分更新
- `State` 内の盤面配列などのディープコピーは禁止します。変更があった箇所のみを計算・更新してください。

`get_actions` での枝刈り
- 悪化手・逆操作を emit しない、良さそうな手を先に emit して `emit.threshold()` を早く締める。詳細は Section 2 の `get_actions`。


---

### 2. 各関数の仕様と実装要件

#### Action 構造体

- 目的: 状態遷移を表す「操作パラメータ」と、`rollback` に必要な「巻き戻し情報」を一体で保持します。
- スケルトン内のメンバ変数は一例です。問題に応じて自由に変更・追加してください。
- `try_op` で上書きされる前の値 `pre_*` と、`apply_op` 後の値 `nxt_*` を持つのが基本パターンです。詳細は Section 1「持たせるか判断」を参照。
- `string to_string() const` は `record_history=true` 時のデバッグ出力用です。中身は空で構いませんが、削除はしないでください。

#### `State::init`

- シグネチャ: `void init()`
- 目的: 探索開始時の初期状態を構築します。`search()` から 1 回だけ呼ばれます。
- 要件:
  - 盤面・スコア・ハッシュ値を初期化してください。
  - 標準入力からの問題データ読み込みはここで行ってください。
  - Zobrist Hash 用の乱数テーブルは一度だけ初期化してください。`State` は探索を通して1個なのでメンバでも `static` でも構いませんが、`init` が複数回呼ばれてもテーブルを作り直さないよう初期化フラグで一度きりにします。盤面サイズに依存しない巨大テーブルなら `static` にしておくと無駄なコピーや再生成を避けられます。

#### `State::try_op`

- シグネチャ: `tuple<ScoreType, HashType, bool> try_op(Action &action, const ScoreType threshold) const`
- 目的: 現在の状態に操作を適用した場合の次状態スコア・ハッシュ値・終了フラグを計算します。
- 要件:
  - `const` 関数なので `State` のメンバは変更しないでください。書き込んでよいのは引数 `action` だけです。スコアとハッシュは差分計算で求めて構いませんが、返すのは絶対値です。
  - 早期打ち切りは Section 1「スコア計算の早期打ち切り」に従います。次スコアが `threshold` 以上だと確定した時点で `{INF, 0, false}` を返してください。INF を返した手は `apply_op` されないので、その手についてはロールバック情報を `action` に書く必要はありません。打ち切らず通す手についてだけ `action` にロールバック情報を書きます。
  - 戻り値 `finished` は、この操作で「最終解として確定してよいゴール状態」に達したかです。終了条件が無い問題では常に `false` を返します。`finished` の手は候補に積まれず展開されません。ライブラリは `finished` が初めて出たターンで探索を打ち切り、そのターン内に複数あれば最小スコアのものを最終解にします。それより深いターンは探索しないので、浅い `finished` が最良とは限らない問題では `finished` を付ける条件に注意してください。
- ⚠️ ホットパスです。詳細は Section 1 を参照。

#### `State::apply_op`

- シグネチャ: `void apply_op(const Action &action)`
- 目的: `State` をインプレース更新して次の状態へ進めます。
- 要件:
  - `try_op` を通過した合法な `Action` に対してのみ呼ばれるため、再バリデーションは不要です。
  - `action` に `nxt_*` を保存しているならそれを使い、保存していないなら操作パラメータから差分計算してください。いずれにせよ変更箇所のみ更新してください。

#### `State::rollback`

- シグネチャ: `void rollback(const Action &action)`
- 目的: `apply_op` 実行前の状態に完全に復元します。
- 要件:
  - `action` に保存された `pre_*` 情報を用いて `apply_op` 前の状態に完全に戻してください。
  - `apply_op` と完全に逆の操作を行うこと。対称性を保ってください。
  - ここでも盤面全体ではなく差分のみを巻き戻してください。

#### `State::get_actions`

- シグネチャ: `template<class Emit> void get_actions(const int turn, const Action &last_action, Emit &&emit) const`
- 目的: 現在の状態で実行可能な合法手を生成し、1手ごとに `emit(action)` を呼びます。中間の `vector<Action>` を作らないのでバッファ確保とコピーが発生しません。
- 要件:
  - 生成した `Action` 変数について `emit(a)` を呼びます。これだけで `try_op`、`INF`/`finished` 判定、候補への push までが内部で実行されるので、`try_op` を自分で呼ぶ必要はありません。
  - `a` は1個を使い回して構いません。push に必要な分は emit 側がコピーし、`try_op` が `a` に書くロールバック情報の扱いも emit 側が行います。
  - `emit.threshold()` は現在のビーム最悪スコアを返します。push が進むたびに縮む最新値なので、列挙ループ内の枝刈りに積極的に使えます。ビームが `beam_width` 個埋まるまでは `INF` を返し枝刈り不発なのは正常です。`try_op` 側の早期打ち切りに任せても構いません。
  - ⚠️ `last_action` はターン 0 の呼び出し時にダミー値です。`turn == 0` のとき `last_action` のメンバを参照すると不正アクセスになるため参照しないでください。
  - 明らかにスコアが悪化する手や、直前操作を単純に打ち消すだけの逆操作は emit しないでください。
  - 良さそうな手を先に emit すると `emit.threshold()` が早く締まり、後続の手や `try_op` の早期打ち切りがよく効きます。Section 1 の早期打ち切りと相乗します。
  - ⚠️ あるターンで生き残った全ノードがスコア有限の手を1つも emit しないと候補が空になり、ライブラリは `candidates==0` を検出して `assert` 停止します。`max_turn` まで常に合法手がある問題では起きません。手が無いだけの行き止まりノードは何も emit しなければ枝が自然に消えるだけで、問題になるのは生き残り全ノードが同時に詰むときだけです。その場合は `max_turn` を到達可能な深さに合わせてください。なお `try_op` の `finished=true` はゴール到達を表すフラグであり、行き止まりの表現には使いません。ゴールに達して `max_turn` より前に探索を正規終了させたいときに使います。
- ⚠️ ホットパスです。詳細は Section 1 を参照。

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
- 目的: デバッグ用に現在の状態を標準エラー出力に表示します。計算量は多少大きくても構わず、人間可読を優先してください。`print` のためだけのメンバを `State` に追加しないこと。

#### `State::get_state_info`

- `string get_state_info() const` は `record_history=true` 時の JSON ログ出力用の互換関数です。`return "{}";` のままにしてください。

#### `search` 関数

0.5 の最小テンプレート内のラッパをそのまま使います。変更不要です。
