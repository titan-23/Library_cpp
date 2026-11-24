# 編集距離

`titan_cpplib/string/edit_distance.cpp` を include して使用します。対応する規格は C++17 以上です。
名前空間は `titan23` です。挿入、削除、置換のコストをそれぞれ 1 とする編集距離を扱います。
隣接要素の交換は 1 回の操作として扱いません。

固定した 2 つの列について多数の区間クエリを処理する場合は、
[EditDistanceQuery](how_to_use_edit_distance_query.md) も使用できます。

## API

| 関数 | 戻り値 | 型 |
| --- | --- | --- |
| `edit_distance(s, t)` | 正確な編集距離 | `int` |
| `edit_distance_bounded(s, t, k)` | 距離が `k` 以下なら正確な距離、超えるなら `-1` | `int` |
| `edit_distance_within(s, t, k)` | 距離が `k` 以下かどうか | `bool` |
| `edit_distance_restore(s, t)` | `s` を `t` に変換する最小回数の操作列 | `std::vector<EditOperation>` |
| `edit_distance_restore_bounded(s, t, k)` | 距離が `k` 以下なら最小回数の操作列、超えるなら `std::nullopt` | `std::optional<std::vector<EditOperation>>` |

`k` の型は `int` です。`k` が負の場合の戻り値は次のとおりです。

- `edit_distance_bounded` は `-1`
- `edit_distance_within` は `false`
- `edit_distance_restore_bounded` は `std::nullopt`

`k` が両方の入力の長さ以上の場合も、`edit_distance_bounded` は上限値ではなく正確な距離を返します。
空の入力にも対応しています。

上限付きの復元に成功した場合は、戻り値から `*` で操作列を取り出して使用します。
距離が 0 の場合は空の操作列を保持する `optional` を返すため、`std::nullopt` と区別できます。

```cpp
#include <cassert>
#include <optional>
#include <string>
#include <vector>
#include "titan_cpplib/string/edit_distance.cpp"

int main() {
    const std::string s = "kitten", t = "sitting";
    assert(titan23::edit_distance(s, t) == 3);
    assert(titan23::edit_distance_bounded(s, t, 3) == 3);
    assert(titan23::edit_distance_bounded(s, t, 2) == -1);
    assert(titan23::edit_distance_within(s, t, 3));
    assert(!titan23::edit_distance_within(s, t, 2));
    assert(titan23::edit_distance_restore(s, t).size() == 3);

    const auto ops = titan23::edit_distance_restore_bounded(s, t, 3);
    assert(ops && ops->size() == 3);
    assert(titan23::edit_distance_restore_bounded(s, t, 2) == std::nullopt);
    assert(titan23::edit_distance_restore_bounded(s, t, -1) == std::nullopt);
    const auto same = titan23::edit_distance_restore_bounded(s, s, 0);
    assert(same && same->empty());

    const std::vector<int> a = {1, 2, 3}, b = {1, 4, 3, 5};
    assert(titan23::edit_distance(a, b) == 2);
    assert(titan23::edit_distance_bounded(a, b, 1) == -1);
    assert(titan23::edit_distance_within(a, b, 2));
    assert(titan23::edit_distance_restore(a, b).size() == 2);
    const auto integer_ops = titan23::edit_distance_restore_bounded(a, b, 2);
    assert(integer_ops && integer_ops->size() == 2);
}
```

## 入力できる列

5 つの関数はいずれも `std::string`、`std::string_view`、文字列リテラル、
NUL 終端された `const char*` に対応しています。
`std::vector<int>`、`std::vector<std::string>`、`std::array`、`std::deque`、
`std::u32string` など、`const` な状態で `size()` と添字アクセスに対応する列も渡すことができます。
入力の列はコピーせず、要素を `==` で比較します。等値比較は同値関係であることを前提とします。
要素の大小比較やハッシュは必要ありません。

各入力の長さは `INT_MAX` 未満である必要があります。
条件を満たさない場合は `std::length_error` を送出します。
ただし `k < 0` の場合は長さを確認せず、各関数の失敗値を返します。

`std::string` と `std::string_view` はバイト単位で比較します。
UTF-8 の日本語の 1 文字を 1 要素として扱う場合は、
`std::u32string` などのコードポイント列に変換してから渡します。
Unicode の正規化や結合文字をまとめる処理は行いません。
文字列リテラルと `const char*` は最初の NUL までを使用します。
途中の NUL も含める場合は、長さを明示した `std::string_view` などを渡します。

## 編集操作の添字

```cpp
enum class EditOperationType { Insert, Delete, Replace };
struct EditOperation {
    EditOperationType type;
    int source_index;
    int target_index;
};
```

上の型は名前空間 `titan23` に定義されています。
操作列には列の先頭から末尾に向かう順に操作が格納され、一致した要素に対する操作は含まれません。
操作の個数は編集距離と一致します。最適な操作列が複数ある場合は、
末尾から復元するときに一致、置換、削除、挿入の順に優先して選択します。
`edit_distance_restore_bounded` が成功した場合は、`edit_distance_restore` と同じ操作列を返します。

`source_index` と `target_index` は、編集途中の文字列ではなく、それぞれ元の `s` と `t` に対する 0 始まりの添字です。
`n = s.size()`、`m = t.size()` とすると、各操作の意味は次のとおりです。

| 操作 | `source_index` | `target_index` | 内容 |
| --- | --- | --- | --- |
| `Insert` | 挿入位置となる境界 `0..n` | 要素の添字 `0..m-1` | `t[target_index]` を挿入 |
| `Delete` | 要素の添字 `0..n-1` | 削除時点の境界 `0..m` | `s[source_index]` を削除 |
| `Replace` | 要素の添字 `0..n-1` | 要素の添字 `0..m-1` | `s[source_index]` を `t[target_index]` に置換 |

結果を文字列に反映するには、元の列の読み取り位置を進めながら新しい列を作成します。
次の関数は、操作の間で省略された一致部分もコピーして `t` を復元します。

```cpp
#include <cassert>
#include <string>
#include <string_view>
#include "titan_cpplib/string/edit_distance.cpp"

std::string reconstruct(std::string_view s, std::string_view t) {
    const auto operations = titan23::edit_distance_restore(s, t);
    std::string result;
    int i = 0, j = 0;
    for (const auto& op : operations) {
        while (i < op.source_index) {
            assert(s[i] == t[j]);
            result.push_back(s[i++]);
            ++j;
        }
        assert(i == op.source_index && j == op.target_index);
        switch (op.type) {
        case titan23::EditOperationType::Insert:
            result.push_back(t[j++]);
            break;
        case titan23::EditOperationType::Delete:
            ++i;
            break;
        case titan23::EditOperationType::Replace:
            result.push_back(t[j++]);
            ++i;
            break;
        }
    }
    while (i < (int)s.size()) {
        assert(s[i] == t[j]);
        result.push_back(s[i++]);
        ++j;
    }
    assert(j == (int)t.size() && std::string_view(result) == t);
    return result;
}
```

## 計算量とテスト

入力の長さを `n, m`、`a = min(n, m)`、`b = max(n, m)` とします。
次の表は、長さの取得、添字アクセス、要素の等値比較をそれぞれ `O(1)` とした場合の計算量です。

| 関数 | 時間 | 追加空間 |
| --- | --- | --- |
| `edit_distance` | `O(nm + n + m)` | `O(a + 1)` |
| `edit_distance_bounded` / `edit_distance_within` | `O((a + 1) min(b + 1, 2k + 1))` | `O(min(b + 1, 2k + 1))` |
| `edit_distance_restore` | `O((n + 1)(m + 1))` | DP 用に `O((n + 1)(m + 1))`、戻り値に `O(n + m)` |
| `edit_distance_restore_bounded` | `O((n + 1) min(m + 1, 2k + 1))` | DP 用に `O((n + 1) min(m + 1, 2k + 1))`、戻り値に `O(k + 1)` |

上限付きの関数の計算量は `k >= 0` の場合の値です。距離が上限を超えることが判明した時点で計算を終了します。
`edit_distance_restore_bounded` では `n = |s|`、`m = |t|` とします。この関数は
`|i - j| <= k` の帯だけを保存して復元し、2 つの入力の順序は入れ替えません。
`k = 0` の場合は一致の確認だけを行い、DP 用の表は作成しません。
NUL 終端文字列を渡した場合は、長さの取得に合計 `O(n + m)` が加わります。

テストを実行するには、リポジトリ直下で次のコマンドを実行します。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -I. test/string/edit_distance_test.cpp -o /tmp/edit_distance_test
/tmp/edit_distance_test
g++ -std=c++17 -O2 -Wall -Wextra -I. test/string/edit_distance_within_test.cpp -o /tmp/edit_distance_within_test
/tmp/edit_distance_within_test
```
