# dycone.cpp インターフェース再設計

対象は `titan_cpplib/ds/dycone.cpp` の `OfflineDynamicConnectivity`。実装は行わず、設計のみをまとめる。計算量の根拠は `review/dycone_complexity.md`、正当性・バグの指摘は `review/ds.md` を参照。

## 現状の問題点

インターフェースに関わるものを整理する。

- **コンストラクタに総クエリ数 q が必要**。更新も参照もすべて時間スロット t を消費するため、利用者は操作総数を事前に正確に数える必要がある。
- **参照系の登録と結果回収が煩雑**。`get_sum` 等は登録時にクエリ番号を返し、答えは `run()` の返す `vector<T>` にまとめて入る。利用者は番号を自前で保持し、型の違う答え（IS_SAME は bool、GET_SIZE は int）まで T に押し込まれた値を読む。
- **名前の衝突**。`namespace titan23` 外にあり、既存の `titan23::OfflineDynamicConnectivity` と同名。expander で両方 include して `using namespace titan23` すると曖昧になる。
- **契約が未定義**。自己ループで構造が壊れる。存在しない辺の削除が `S[0]` を破壊する。多重辺・二重削除は非対応。`run()` の2回呼び出しで壊れる。いずれも assert もコメントもない。
- **規約違反**。`<bits/stdc++.h>`、`#pragma once` なし、メンバ初期化順の不一致。

## 設計方針

- 既存の `titan23::OfflineDynamicConnectivity` / `OfflineDynamicConnectivitySum` と同じ **「next_query() で参照点を予約し、run(コールバック) で答える」二相型**に揃える。ライブラリ内で形式が統一され、乗り換えのコストがなくなる。
- 参照系はクエリ登録ではなく、**コールバック内で直接呼べるメソッド**にする。登録番号・結果ベクタ・型の押し込みが丸ごと消える。1 つの参照点で複数の読み取りを組み合わせることもできる。
- 参照の返り値に正しい型を与える。`same` は bool、`size` / `group_count` は int、`sum` は T。
- q の事前申告を廃止し、内部バッファは動的に伸ばす。
- 契約を明文化し、assert で強制する。

## 新インターフェース

```cpp
namespace titan23 {

// オフライン動的連結性(削除時刻重みの最大全域森)
// 頂点重み T は可換群を要求する(+=, -=, ゼロ値でのデフォルト構築)
template<typename T>
class DyCone {
  public:
    explicit DyCone(int n, unsigned seed = 1321312);
    explicit DyCone(const vector<T> &init, unsigned seed = 1321312);

    // ---- 構築フェーズ: 時系列順に呼ぶ ----
    void add_edge(int u, int v);    // 辺を追加する。u == v は無視
    void delete_edge(int u, int v); // 辺を削除する。現存する辺であること
    void add_point(int u, T x);     // 頂点 u の重みに x を加算する
    void next_query();              // コールバックの呼び出し点を予約する

    // ---- 実行: 1回限り ----
    template<typename F> // void out(int k)
    void run(F &&out);   // k 番目の next_query の時点で out(k) を呼ぶ

    // ---- 参照: run のコールバック内、または run 完了後に呼ぶ ----
    bool same(int u, int v);  // u, v が連結か
    int size(int u);          // u の成分の頂点数
    T sum(int u);             // u の成分の重み和
    int group_count() const;  // 連結成分数
};

} // namespace titan23
```

### 使用例

yosupo judge「Dynamic Graph Vertex Add Component Sum」の場合。

```cpp
titan23::DyCone<long long> dc(a);  // a は初期重み
vector<int> qs;                    // sum クエリの引数を自前で保持する
// 入力を読みながら登録する
//   0 u v -> dc.add_edge(u, v)
//   1 u v -> dc.delete_edge(u, v)
//   2 u x -> dc.add_point(u, x)
//   3 u   -> qs.push_back(u); dc.next_query();
dc.run([&] (int k) {
    cout << dc.sum(qs[k]) << '\n';
});
```

クエリの引数（この例では `qs`）は利用者が保持する。これは既存2クラスと同じ流儀で、オフライン処理では入力を全て読む段階で自然に手元に残る。

## 契約と assert

| 契約 | 強制方法 |
|---|---|
| 更新と next_query は時系列順に登録する | 設計上の前提としてコメントに明記 |
| run は1回だけ呼べる | フェーズフラグ + assert |
| 参照メソッドは run 開始後にのみ呼べる | フェーズフラグ + assert |
| delete_edge の対象は現存する辺 | 内部辞書を find で確認して assert |
| 多重辺は可。削除は後入れ先出しで対応づける | 辞書の値を追加時刻のスタックにする |
| 自己ループは追加・削除とも無視する | メソッド先頭で早期 return |
| 頂点番号は [0, n) | assert |

多重辺の対応づけについて補足する。同一頂点対の辺は端点が同じなので、追加と削除の対応づけをどう選んでも「1本以上生きている期間」は変わらず、連結性は一致する。後入れ先出しは実装が単純なだけで、意味上の選択ではない。

## 内部設計メモ

実装時に合わせて行う変更を列挙する。

- 重み付けは現行踏襲。削除される更新の通し番号を負号つきで辺の重みにする。削除されない辺は番兵値。重み表 S は動的に伸ばす。
- 辺辞書は `unordered_map<long long, vector<int>>`（キーは u * n + v、u < v）。存在しない辺の削除で `operator[]` が空エントリを作る現行バグは、find + assert に置き換えることで消える。
- 参照メソッドはクエリバッファを介さず内部の find 等を直接呼ぶ。GET_* 系の内部ディスパッチと結果ベクタは削除する。
- メンバ変数 `group_count` とメソッド名が衝突するため、メンバは `group_count_` 等に改名する。
- 乱数 seed はコンストラクタ引数にする。省略時は固定値。ハックのあるジャッジでは実行時乱数を渡す（`review/dycone_complexity.md` の注意点参照）。
- 規約対応。`#pragma once`、個別 include、`namespace titan23`、メンバ初期化順の修正。
- `inner_add_edge` の2回目の `sub_del(P[p], p, W[p])` は常に空振りする死にコードなので削除する。
- `inner_add_point` に find と同形の短絡（`sum[P[u]] -= sum[u]` を伴う付け替え）を入れる。計算量解析の隙間（add_point 偏重時の未圧縮経路）が閉じる。
- `disconnect` の再帰は反復に書き換える。深さは期待 O(log n) だが最悪 O(n) になるため。
- `review/ds.md` の dycone への指摘（[注意]・[軽微]）はこの機会に全て解消する。
- テストを `test/dycone/` に追加する。`mk_test.py` でランダムケースを生成し、愚直解（毎回 BFS/DSU 再構築）と照合する。自己ループ・多重辺・存在辺の再追加を含める。

## 代替案と不採用理由

**A. 登録式 + 型別結果ベクタ**。`get_sum(u)` が「sum クエリの通し番号」を返し、`run()` が `{vector<T> sum; vector<int> size; vector<char> same; ...}` の構造体を返す。型の問題は解決するが、利用者が（種別, 添字）の対応を自前管理する点は変わらず、API も大きい。既存2クラスと形式が揃わない。

**B. 現行のまま vector<T> を返す**。型の押し込みと番号管理が残る。

**C. オンライン化**。不可能。クエリ列を先読みして辺の削除時刻を重みにすることが構造の前提であり、計算量証明もオフライン性（敵対者が乱数に適応できないこと）に依存する。

## 既存クラスとの棲み分け

| クラス | 対象 | 計算量 |
|---|---|---|
| `OfflineDynamicConnectivity`（セグ木 + undoable DSU） | 任意の DSU 拡張。可換群にならない集約（max 等）はこちら | O(q log q log n) |
| `DyCone`（本設計） | same / size / group_count / 可換群の成分和 | 期待 O((n + q) log n) |

`DyCone` の集約は差分更新（減算）を使うため可換群に限られる。この制約に収まる用途では `OfflineDynamicConnectivitySum` を完全に置き換えられるので、実装・テスト完了後に置き換えを検討する。

クラス名は `DyCone` を提案する。ファイル名と一致し、既存クラスとの衝突がない。ライブラリの命名規則（記述的な名前）に寄せるなら `OfflineDynamicConnectivityMSF` が候補になる。
