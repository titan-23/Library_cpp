# tree_generator.cpp

## TreeType::Centipede

- ムカデグラフはパスグラフの各頂点に一つずつ頂点を接続した木です。
- centipede はムカデを意味します。この語で検索するとムカデの画像が表示されるため注意が必要です。
- 検索する場合は `centipede graph` が適していると考えられます。

## gen_lobster の定数

- パスの長さ path_len = max(1, (int)sqrt(sqrt(N))) と D = max(1, (int)sqrt(sqrt(sqrt(N-path_len)))) の式には根拠がありません。元のコメントには「謎」と記載されていました。
