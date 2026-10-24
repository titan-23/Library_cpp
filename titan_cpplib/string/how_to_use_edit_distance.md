# 編集距離

`titan_cpplib/string/edit_distance.cpp` を include して使う。C++17 以上。
名前空間は `titan23`。挿入・削除・置換を各コスト 1 とする編集距離を扱う。
隣接要素の交換は 1 操作には数えない。

固定した 2 列について多数の区間クエリを処理する場合は、
[EditDistanceQuery](how_to_use_edit_distance_query.md)も使える。

## API

| 関数 | 戻り値 |
| --- | --- |
| `edit_distance(s, t)` | 正確な編集距離。型は `int` |
| `edit_distance_bounded(s, t, k)` | 距離が `k` 以下なら正確な距離、超えるなら `-1`。型は `int` |
| `edit_distance_within(s, t, k)` | 距離が `k` 以下かを表す `bool` |
| `edit_distance_restore(s, t)` | `s` から `t` へ変える最小回数の操作列。型は `std::vector<EditOperation>` |
| `edit_distance_restore_bounded(s, t, k)` | 距離が `k` 以下なら最小回数の操作列、超えるなら `std::nullopt`。型は `std::optional<std::vector<EditOperation>>` |

`k` は `int`。負のとき `edit_distance_bounded` は `-1`、
`edit_distance_within` は `false`、`edit_distance_restore_bounded` は `std::nullopt` を返す。
`k` が両入力の長さ以上でも、`edit_distance_bounded` は上限値ではなく正確な距離を返す。
空の入力にも対応する。

上限付きの復元に成功したら、戻り値を `*` で取り出して操作列を使う。
距離 0 のときは空の操作列を持つ `optional` を返すため、`std::nullopt` と区別できる。

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

5 関数とも `std::string`、`std::string_view`、文字列リテラル、
NUL 終端された `const char*` に対応する。
`std::vector<int>`、`std::vector<std::string>`、`std::array`、`std::deque`、
`std::u32string` など、`const` な列から `size()` と添字アクセスを使える列も渡せる。
入力列はコピーせず、要素を `==` で比較する。等値比較は同値関係であることを前提とし、
要素の大小比較やハッシュは不要。

各入力の長さは `INT_MAX` 未満である必要がある。
条件を満たさない場合は `std::length_error` を送出する。
ただし `k < 0` の場合は長さの確認をせず、それぞれの失敗値を返す。

`std::string` と `std::string_view` はバイト単位で比較する。
UTF-8 の日本語を 1 文字として扱いたい場合は、必要に応じて
`std::u32string` などのコードポイント列へ変換してから渡す。
Unicode の正規化や、結合文字をまとめる処理は行わない。
文字列リテラルと `const char*` は最初の NUL までを使う。
途中の NUL も含める場合は、長さを明示した `std::string_view` などを渡す。

## 編集操作の添字

```cpp
enum class EditOperationType { Insert, Delete, Replace };
struct EditOperation {
    EditOperationType type;
    int source_index;
    int target_index;
};
```

上の型は `titan23` 内に定義されている。
操作列は先頭から末尾へ進む順に並び、一致した要素は含まない。
操作数は編集距離に等しい。最適な操作列が複数ある場合は、
末尾からの復元で一致・置換・削除・挿入の順に優先して選ぶ。
`edit_distance_restore_bounded` が成功した場合、`edit_distance_restore` と同じ操作列を返す。

`source_index` は元の `s`、`target_index` は元の `t` に対する 0 始まりの添字。
編集途中の文字列に対する添字ではない。
`n = s.size()`、`m = t.size()` とすると、各操作の意味は次のとおり。

| 操作 | `source_index` | `target_index` | 内容 |
| --- | --- | --- | --- |
| `Insert` | 挿入位置となる境界 `0..n` | 要素の添字 `0..m-1` | `t[target_index]` を挿入 |
| `Delete` | 要素の添字 `0..n-1` | 削除時点の境界 `0..m` | `s[source_index]` を削除 |
| `Replace` | 要素の添字 `0..n-1` | 要素の添字 `0..m-1` | `s[source_index]` を `t[target_index]` に置換 |

結果を文字列へ反映するには、元の列の読み取り位置を進めながら新しい列を作る。
次の関数は、操作間で省略された一致部分もコピーして `t` を復元する。

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

入力の長さを `n, m`、`a = min(n, m)`、`b = max(n, m)` とする。
長さの取得、添字アクセス、要素の等値比較を `O(1)` とした計算量。

| 関数 | 時間 | 追加空間 |
| --- | --- | --- |
| `edit_distance` | `O(nm + n + m)` | `O(a + 1)` |
| `edit_distance_bounded` / `edit_distance_within` | `O((a + 1) min(b + 1, 2k + 1))` | `O(min(b + 1, 2k + 1))` |
| `edit_distance_restore` | `O((n + 1)(m + 1))` | DP 用に `O((n + 1)(m + 1))`、戻り値に `O(n + m)` |
| `edit_distance_restore_bounded` | `O((n + 1) min(m + 1, 2k + 1))` | DP 用に `O((n + 1) min(m + 1, 2k + 1))`、戻り値に `O(k + 1)` |

上限つきの計算量は `k >= 0` の場合。距離が上限を超えると分かれば途中で終了する。
`edit_distance_restore_bounded` は `n = |s|`、`m = |t|` とし、
`|i - j| <= k` の帯だけを保存して復元する。入力の順序は交換しない。
`k = 0` では一致の確認だけを行い、DP 用の表は作らない。
NUL 終端文字列を渡した場合は、長さの取得に合計 `O(n + m)` が加わる。

リポジトリ直下でテストを実行する。

```sh
g++ -std=c++17 -O2 -Wall -Wextra -I. test/string/edit_distance_test.cpp -o /tmp/edit_distance_test
/tmp/edit_distance_test
g++ -std=c++17 -O2 -Wall -Wextra -I. test/string/edit_distance_within_test.cpp -o /tmp/edit_distance_within_test
/tmp/edit_distance_within_test
```
