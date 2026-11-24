# 固定した 2 つの列の区間の編集距離

`titan_cpplib/string/edit_distance_query.cpp` を include して使用します。対応する規格は C++17 以上です。
`titan23::EditDistanceQuery` は 2 つの列を前処理し、任意の 2 つの区間の編集距離を求めます。
挿入、削除、置換のコストはそれぞれ 1 です。隣接要素の交換は 1 回の操作として扱いません。

同じ列に対するクエリが多く、上限 `k` や実際の距離 `d` が小さい場合に適しています。
前処理をせずに比較する場合や編集操作を復元する場合は、
[通常の編集距離関数](how_to_use_edit_distance.md)を使用します。

## 構築とクエリ

```cpp
#include <cassert>
#include <string>
#include <vector>
#include "titan_cpplib/string/edit_distance_query.cpp"

int main() {
    titan23::EditDistanceQuery query("--kitten++", "!!sitting??");
    assert(query.source_size() == 10);
    assert(query.target_size() == 11);

    // s[2:8] = "kitten", t[2:9] = "sitting" を比較する
    assert(query.distance(2, 8, 2, 9) == 3);
    assert(query.distance_bounded(2, 8, 2, 9, 3) == 3);
    assert(query.distance_bounded(2, 8, 2, 9, 2) == -1);
    assert(query.within(2, 8, 2, 9, 3));
    assert(!query.within(2, 8, 2, 9, 2));
    assert(query.distance(3, 6, 3, 6) == 0);  // "itt" 同士
    assert(query.distance(0, 0, 2, 9) == 7);  // 空区間と "sitting"

    std::vector<int> a = {9, 1, 2, 3, 9}, b = {8, 1, 4, 3, 5, 8};
    titan23::EditDistanceQuery integers(a, b);
    assert(integers.distance(1, 4, 1, 5) == 2);
    a[2] = 4;  // 構築済みの索引は元の内容を保持する
    assert(integers.distance(1, 4, 1, 5) == 2);

    const std::string text = "abcxabd";
    titan23::EditDistanceQuery same(text, text);
    assert(same.distance(0, 3, 4, 7) == 1);  // 同じ文字列内の 2 つの区間
}
```

すべての添字と上限 `k` の型は `int` です。区間は 0 始まりの半開区間です。
クエリは `s[l1:r1]` と `t[l2:r2]` を比較します。入力を切り出してコピーする必要はありません。

| メソッド | 戻り値 | 型 |
| --- | --- | --- |
| `source_size()` / `target_size()` | 構築時の各列の長さ | `int` |
| `distance(l1, r1, l2, r2)` | 正確な編集距離 | `int` |
| `distance_bounded(l1, r1, l2, r2, k)` | 距離が `k` 以下なら正確な距離、超えるなら `-1` | `int` |
| `within(l1, r1, l2, r2, k)` | 距離が `k` 以下かどうか | `bool` |

区間は `0 <= l1 <= r1 <= source_size()` と `0 <= l2 <= r2 <= target_size()` を満たす必要があります。
範囲外の区間は `assert` で検出します。空区間にも対応しています。
`k < 0` の場合、`distance_bounded` は `-1` を、`within` は `false` を返します。
上限が大きい場合でも `distance_bounded` は正確な距離を返します。

## 入力と保持する内容

`std::string`、`std::string_view`、文字列リテラル、NUL 終端された `const char*`、
`std::vector<int>` などを渡すことができます。
一般の列は `const` な状態で `size()` と添字アクセスに対応している必要があります。

構築時に要素を座標圧縮するため、要素には `<` による整合した順序が必要です。
`!(x < y) && !(y < x)` と `x == y` が一致することを前提とします。
通常の編集距離関数は `==` だけで使用できますが、このクラスでは `<` も必要です。

`std::string` と `std::string_view` はバイト単位で比較します。
文字列リテラルと `const char*` は最初の NUL までを使用します。
途中の NUL も含める場合は、長さを明示した `std::string_view` などを渡します。
日本語をコードポイント単位で比較する場合は、`std::u32string` などに変換して渡します。
Unicode の正規化や結合文字をまとめる処理は行いません。

構築後のクエリは内部に保持した索引だけを使用します。
元の列を変更または破棄しても結果は変化しません。
更新操作には対応していないため、変更後の列を使用する場合は再構築します。

## 計算量

`N = |s| + |t| + 1` とし、比較する区間の編集距離を `d` とします。
次の表は、添字アクセスと要素の比較をそれぞれ `O(1)` とした場合の計算量です。

| 処理 | 時間 | 空間 |
| --- | --- | --- |
| 構築 | `O(N log N)` | `O(N log N)` を保持 |
| `distance_bounded` / `within` | `O((k + 1)^2)` | 作業用に `O(k + 1)` |
| `distance` | `O((d + 1)^2)` | 作業用に `O(d + 1)` |

上限付きのメソッドの計算量は `k >= 0` の場合の値です。
実際に使用する上限は、比較する区間のうち長い方の長さ以下に抑えます。
次のような場合は `O(1)` で結果を返します。

- 長さの差だけで上限を超えることが判明する場合
- 空区間の場合
- 一致する区間同士の場合

前処理では接尾辞配列、LCP 配列、Sparse Table を構築します。
クエリでは一致する部分をまとめてスキップするため、距離が小さい場合は区間長によらず高速です。
ハッシュを使用しないため、衝突による誤判定は発生しません。
距離が大きい場合は、`distance` の計算時間が区間長の 2 乗のオーダーになり得ます。

編集回数ごとに各対角線の最遠到達点を保持する方法は、
[Does Preprocessing help in Fast Sequence Comparisons? の第 3 節](https://arxiv.org/pdf/2108.09115)
で説明されています。本実装では LCE をクエリの区間の終端で打ち切って使用します。

長さは `|s| + |t| + 1 <= INT_MAX` を満たす必要があります。
満たさない場合は構築時に `std::length_error` を送出します。
特に長い入力では、前処理で使用する `O(N log N)` のメモリも確保できるかを確認する必要があります。
このクラスは距離の取得と判定に対応しており、編集操作列の復元は行いません。

## テスト

リポジトリ直下で次のコマンドを実行します。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -I. test/string/edit_distance_query_test.cpp -o /tmp/edit_distance_query_test
/tmp/edit_distance_query_test
```
