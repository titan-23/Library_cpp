# dycone.cpp インターフェース再設計

対象は `titan_cpplib/graph/dycone.cpp` の `OfflineDynamicConnectivity` です。この文書では実装は行わず、設計だけを記載します。計算量の根拠は `review/dycone_complexity.md`、正当性とバグの指摘は `review/ds.md` を参照してください。

## 現状の問題点

インターフェースに関する問題点は次のとおりです。

- **コンストラクタに総クエリ数 q が必要**: 更新も参照もすべて時間スロット t を消費するため、利用者は操作の総数を事前に正確に求める必要があります。
- **参照系の登録と結果の取得が煩雑**: `get_sum` などは登録時にクエリ番号を返し、答えは `run()` が返す `vector<T>` にまとめて格納されます。利用者は番号を自分で保持し、型の異なる答えも T に変換された値として参照する必要があります。本来の型は IS_SAME が bool、GET_SIZE が int です。
- **名前の衝突**: このクラスは `namespace titan23` の外にあり、既存の `titan23::OfflineDynamicConnectivity` と同名です。expander で両方を include して `using namespace titan23` を宣言すると、名前の参照が曖昧になります。
- **契約が未定義**: 自己ループを追加すると内部の構造が不正な状態になります。存在しない辺を削除すると `S[0]` が誤って更新されます。多重辺と二重の削除には対応していません。`run()` を 2 回呼び出すと不正な状態になります。いずれについても assert もコメントもありません。
- **規約違反**: 該当するのは `<bits/stdc++.h>` の使用、`#pragma once` の欠落、メンバの初期化順の不一致です。

## 設計方針

- 既存の `titan23::OfflineDynamicConnectivity`, `OfflineDynamicConnectivitySum` と同じ **「next_query() で参照する時点を予約し、run(コールバック) で答える」二段階の形式**にそろえます。ライブラリ内で形式が統一され、クラスを切り替えるときのコストがなくなります。
- 参照系はクエリの登録ではなく、**コールバック内で直接呼び出すことができるメソッド**にします。登録番号、結果のベクタ、T への型の変換がすべて不要になります。予約した 1 つの時点で複数の参照を組み合わせることもできます。
- 参照の返り値は正しい型にします。`same` は bool、`size` と `group_count` は int、`sum` は T です。
- q の事前の指定を廃止し、内部バッファは動的に拡張します。
- 契約を明文化し、assert で強制します。

## 新インターフェース

```cpp
namespace titan23 {

// オフライン動的連結性 (削除時刻を重みとする最大全域森)
// 頂点の重みの型 T は可換群である必要がある (+=, -=, ゼロ値でのデフォルト構築)
template<typename T>
class DyCone {
  public:
    explicit DyCone(int n, unsigned seed = 1321312);
    explicit DyCone(const vector<T> &init, unsigned seed = 1321312);

    // ---- 構築フェーズ: 時系列順に呼び出す ----
    void add_edge(int u, int v);    // 辺を追加する / u == v の場合は何もしない
    void delete_edge(int u, int v); // 辺を削除する / 現存する辺に限る
    void add_point(int u, T x);     // 頂点 u の重みに x を加算する
    void next_query();              // コールバックを呼び出す時点を予約する

    // ---- 実行: 1 回限り ----
    template<typename F> // void out(int k)
    void run(F &&out);   // k 番目の next_query の時点で out(k) を呼び出す

    // ---- 参照: run のコールバック内、または run の完了後に呼び出す ----
    bool same(int u, int v);  // u, v が連結かどうか
    int size(int u);          // u の成分の頂点数
    T sum(int u);             // u の成分の重みの総和
    int group_count() const;  // 連結成分数
};

} // namespace titan23
```

### 使用例

yosupo judge の「Dynamic Graph Vertex Add Component Sum」を解く場合の例です。

```cpp
titan23::DyCone<long long> dc(a);  // a は初期の重み
vector<int> qs;                    // sum クエリの引数を利用側で保持する
// 入力を読み込みながら登録する
//   0 u v -> dc.add_edge(u, v)
//   1 u v -> dc.delete_edge(u, v)
//   2 u x -> dc.add_point(u, x)
//   3 u   -> qs.push_back(u); dc.next_query();
dc.run([&] (int k) {
    cout << dc.sum(qs[k]) << '\n';
});
```

クエリの引数 (この例では `qs`) は利用者が保持します。これは既存の 2 クラスと同じ方式です。オフライン処理では入力をすべて読み込むため、その段階で引数をそのまま保持できます。

## 契約と assert

| 契約 | 強制方法 |
|---|---|
| 更新と next_query は時系列順に登録する | 設計上の前提としてコメントに明記 |
| run は 1 回だけ呼び出すことができる | フェーズフラグ + assert |
| 参照メソッドは run の開始後にだけ呼び出すことができる | フェーズフラグ + assert |
| delete_edge の対象は現存する辺 | 内部の辞書を find で確認して assert |
| 多重辺を許可し、削除は後入れ先出しで対応づける | 辞書の値を追加時刻のスタックにする |
| 自己ループの追加と削除はスキップする | メソッドの先頭で早期 return |
| 頂点番号は [0, n) | assert |

多重辺では同じ頂点対の辺の端点が同じであるため、追加と削除の対応づけをどのように選んでも「1 本以上の辺が存在する期間」は変化せず、連結性は一致します。後入れ先出しは実装が単純であるために選んだものであり、意味に影響する選択ではありません。

## 内部設計メモ

実装時に合わせて行う変更は次のとおりです。

- 重み付けは現行の方式を引き継ぎます。辺が削除される更新の通し番号に負号を付けた値を、その辺の重みにします。削除されない辺の重みは番兵の値にします。重みの表 S は動的に拡張します。
- 辺の辞書は `unordered_map<long long, vector<int>>` にします。キーは u * n + v で、u < v とします。存在しない辺の削除で `operator[]` が空のエントリを作成する現行のバグは、find + assert に置き換えることで解消されます。
- 参照メソッドはクエリのバッファを介さずに内部の find などを直接呼び出します。GET_* 系の内部のディスパッチと結果のベクタは削除します。
- メンバ変数 `group_count` とメソッド名が衝突するため、メンバ変数は `group_count_` などに改名します。
- 乱数のシードはコンストラクタの引数にします。省略時は固定値です。ハックのあるジャッジでは実行時に生成した乱数を渡します (`review/dycone_complexity.md` の注意点を参照)。
- 規約に対応します。対象は `#pragma once`、個別の include、`namespace titan23`、メンバの初期化順の修正です。
- `inner_add_edge` の 2 回目の `sub_del(P[p], p, W[p])` は、常に対象を見つけずに終了するデッドコードであるため削除します。
- `inner_add_point` に find と同じ形の短絡を追加します。この短絡は `sum[P[u]] -= sum[u]` を伴う付け替えです。これにより、add_point に偏ったクエリ列で経路が圧縮されないという計算量解析上の問題が解消されます。
- `disconnect` の再帰は反復に変更します。深さは期待 O(log n) ですが、最悪の場合は O(n) になるためです。
- `review/ds.md` の dycone への指摘 ([注意], [軽微]) は今回の変更ですべて解消します。
- テストを `test/graph/dycone/` に追加します。`mk_test.py` でランダムなケースを生成し、毎回 BFS/DSU の再構築を行う愚直解と照合します。ケースには自己ループ、多重辺、存在する辺の再追加を含めます。

## 代替案と不採用理由

**A. 登録式 + 型別の結果ベクタ**: `get_sum(u)` が「sum クエリの通し番号」を返し、`run()` が `{vector<T> sum; vector<int> size; vector<char> same; ...}` の構造体を返します。型の問題は解決しますが、利用者が (種別, 添字) の対応を自分で管理する点は現行と同じであり、API も大きくなります。また、既存の 2 クラスと形式がそろいません。

**B. 現行のまま vector<T> を返す**: T への型の変換と番号の管理が残ります。

**C. オンライン化**: 実現できません。クエリ列を先読みして辺の削除時刻を重みにすることがこのデータ構造の前提です。計算量の証明も、敵対者が乱数に適応できないというオフライン性に依存しています。

## 既存クラスとの使い分け

| クラス | 対象 | 計算量 |
|---|---|---|
| `OfflineDynamicConnectivity` (セグ木 + undoable DSU) | 任意の DSU 拡張、可換群にならない集約 (max など) | O(q log q log n) |
| `DyCone` (本設計) | same / size / group_count / 可換群の成分和 | 期待 O((n + q) log n) |

`DyCone` の集約は減算による差分更新を使用するため、可換群に限られます。この制約を満たす用途では `OfflineDynamicConnectivitySum` を完全に置き換えることができるため、実装とテストの完了後に置き換えを検討します。

クラス名は `DyCone` を提案します。ファイル名と一致し、既存クラスと衝突しません。記述的な名前にするというライブラリの命名規則に合わせる場合は `OfflineDynamicConnectivityMSF` が候補になります。
