# 固定した 2 列の区間編集距離

`titan_cpplib/string/edit_distance_query.cpp` を include して使う。C++17 以上。
`titan23::EditDistanceQuery` は 2 列を前処理し、任意の 2 区間の編集距離に答える。
挿入・削除・置換は各コスト 1。隣接要素の交換は 1 操作には数えない。

同じ列を使うクエリが多く、上限 `k` や実際の距離 `d` が小さい場合に向く。
前処理なしで比較する場合や編集操作を復元する場合は、
[通常の編集距離関数](how_to_use_edit_distance.md)を使う。

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
    assert(same.distance(0, 3, 4, 7) == 1);  // 同じ文字列内の 2 区間
}
```

すべての添字と上限 `k` は `int`。区間は 0 始まりの半開区間。
`s[l1:r1]` と `t[l2:r2]` を比較し、入力を切り出してコピーする必要はない。

| メソッド | 戻り値 |
| --- | --- |
| `source_size()` / `target_size()` | 構築時の各列の長さ。型は `int` |
| `distance(l1, r1, l2, r2)` | 正確な編集距離。型は `int` |
| `distance_bounded(l1, r1, l2, r2, k)` | 距離が `k` 以下なら正確な距離、超えるなら `-1`。型は `int` |
| `within(l1, r1, l2, r2, k)` | 距離が `k` 以下かを表す `bool` |

`0 <= l1 <= r1 <= source_size()`、`0 <= l2 <= r2 <= target_size()` が必要。
範囲外の区間は `assert` で検査する。空区間にも対応する。
`k < 0` のとき `distance_bounded` は `-1`、`within` は `false`。
上限が大きい場合でも `distance_bounded` は正確な距離を返す。

## 入力と保持する内容

`std::string`、`std::string_view`、文字列リテラル、NUL 終端された `const char*`、
`std::vector<int>` などを渡せる。
一般の列は `const` な状態で `size()` と添字アクセスが使えることが必要。

構築時に要素を座標圧縮するため、要素の `<` による整合した順序が必要。
`!(x < y) && !(y < x)` と `x == y` が一致することを前提とする。
通常の編集距離関数は `==` のみで使えるが、このクラスでは `<` も必要となる。

`std::string` と `std::string_view` はバイト単位で比較する。
文字列リテラルと `const char*` は最初の NUL までを使う。
途中の NUL も含める場合は、長さを明示した `std::string_view` などを渡す。
日本語をコードポイント単位で比べる場合は `std::u32string` などへ変換して渡す。
Unicode の正規化や、結合文字をまとめる処理は行わない。

構築後のクエリは、内部に保持した索引だけを使う。
元の列を変更・破棄しても結果は変わらない。
更新操作はなく、変更後の列を使いたい場合は構築し直す。

## 計算量

`N = |s| + |t| + 1`、比較する区間の編集距離を `d` とする。
添字アクセスと要素の比較を `O(1)` とした計算量。

| 処理 | 時間 | 空間 |
| --- | --- | --- |
| 構築 | `O(N log N)` | `O(N log N)` を保持 |
| `distance_bounded` / `within` | `O((k + 1)^2)` | 作業用に `O(k + 1)` |
| `distance` | `O((d + 1)^2)` | 作業用に `O(d + 1)` |

上限つきの計算量は `k >= 0` の場合。
実際に使う上限は比較区間の長い方の長さまでに抑える。
長さの差だけで上限を超えると分かる場合、空区間の場合、
一致する区間同士の場合などは `O(1)` で答えられる。

前処理では suffix array、LCP 配列、Sparse Table を構築する。
クエリでは一致する部分をまとめて飛ばすため、距離が小さいときは区間長によらず速い。
ハッシュは使わず、衝突による誤判定はない。
距離が大きい場合、`distance` の時間は区間長に対して二次になり得る。

編集回数ごとに各対角線の最遠到達点を持つ方法は、
[Does Preprocessing help in Fast Sequence Comparisons? の第 3 節](https://arxiv.org/pdf/2108.09115)
に説明されている。本実装では LCE を問い合わせ区間の終端で打ち切って使う。

長さは `|s| + |t| + 1 <= INT_MAX` を満たす必要がある。
超える場合は構築時に `std::length_error` を送出する。
特に長い入力では、`O(N log N)` の前処理メモリも確保できるか確認する。
このクラスは距離の取得と判定に対応し、編集操作列の復元は行わない。

## テスト

リポジトリ直下で実行する。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -I. test/string/edit_distance_query_test.cpp -o /tmp/edit_distance_query_test
/tmp/edit_distance_query_test
```
