# print.cpp レビュー

`titan_cpplib/others/print.cpp` のデバッグ出力ユーティリティを確認した。修正すべき点と追加推奨をまとめる。

## 修正すべき点

### `tuple` の対応が3要素と4要素のみ

2要素や5要素以上は未対応でコンパイルできない。可変長テンプレートで一本化すると重複も消える。

```cpp
template<class Tuple, size_t... I>
void print_tuple(ostream& os, const Tuple& t, index_sequence<I...>) {
    os << "(";
    ((os << (I ? ", " : "") << get<I>(t)), ...);
    os << ")";
}
template<class... Ts>
ostream& operator<<(ostream& os, const tuple<Ts...>& t) {
    print_tuple(os, t, index_sequence_for<Ts...>{});
    return os;
}
```

## 追加推奨の機能

### 変数名付きデバッグマクロ

使用頻度が高い。`name = value` を `cerr` に出す。

```cpp
#define debug(...) (cerr << to_dim(#__VA_ARGS__) << " = ", debug_out(__VA_ARGS__))
inline void debug_out() { cerr << endl; }
template<class H, class... T> void debug_out(const H& h, const T&... t) {
    cerr << h; if (sizeof...(t)) cerr << ", "; debug_out(t...);
}
```

`debug(x, y, v)` で `x, y, v = 3, 5, [1, 2]` のように出る。

## 補足

`operator<<` の各オーバーロードを `namespace titan23` の外、グローバルに置いている。これは std 型に対する ADL を効かせるための妥当な選択。`namespace titan23` に入れると `vector` などの実引数依存探索で見つからなくなる。規約「全コードを `namespace titan23` に」の例外として、この配置は維持してよい。

## 対応の優先度

- 先に直す。`tuple` の可変長化。
- 併せて入れる。変数名付きデバッグマクロ。
