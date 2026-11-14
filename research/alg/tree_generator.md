# tree_generator.cpp

## TreeType::Centipede

- ムカデグラフは、一直線の木の各頂点に一つずつ頂点を接続した木である。
- centipede はムカデの意味である。この語で検索するとムカデの画像が出るので注意する。
- 検索するなら `centipede graph` がよさそうである。

## gen_lobster の定数

- パスの長さ path_len = max(1, (int)sqrt(sqrt(N))) と、D = max(1, (int)sqrt(sqrt(sqrt(N-path_len)))) の式には根拠がない。元のコメントでは「謎」と書かれていた。
