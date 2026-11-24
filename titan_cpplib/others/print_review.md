# print.cpp レビュー

`titan_cpplib/others/print.cpp` のデバッグ出力ユーティリティを確認しました。

## 修正すべき点

### 3 要素と 4 要素だけに対応する `tuple` の出力

2 要素や 5 要素以上には対応しておらず、コンパイルできません。可変長テンプレートで一本化すると、重複した定義もなくなります。

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

## 追加を推奨する機能

### 変数名付きデバッグマクロ

使用頻度の高いマクロです。`name = value` の形式で `cerr` に出力します。

```cpp
#define debug(...) (cerr << to_dim(#__VA_ARGS__) << " = ", debug_out(__VA_ARGS__))
inline void debug_out() { cerr << endl; }
template<class H, class... T> void debug_out(const H& h, const T&... t) {
    cerr << h; if (sizeof...(t)) cerr << ", "; debug_out(t...);
}
```

`debug(x, y, v)` を呼び出すと `x, y, v = 3, 5, [1, 2]` のように出力されます。

## 補足

`operator<<` の各オーバーロードは `namespace titan23` の外のグローバル名前空間に定義されています。これは std の型に対して ADL を有効にするための妥当な選択です。`namespace titan23` の中に定義すると、`vector` などの実引数依存探索で見つからなくなります。規約「全コードを `namespace titan23` に」の例外として、この配置は維持して問題ありません。

## 対応の優先度

- 先に修正する項目: `tuple` の可変長化
- 併せて追加する項目: 変数名付きデバッグマクロ
