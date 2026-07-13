# titan_cpplib/others レビュー

対象は io.cpp、print.cpp、util.cpp の3ファイル。grid_design.md と print_review.md はドキュメントのため対象外とした。

重要度は3段階。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる
- **[軽微]** 動作に影響しない指摘

## io.cpp

- read_int64 / read_char / read_string の字句処理自体は正しい。
- **[注意] include するだけで標準入力を先読みする**。グローバル変数 `scanner` のコンストラクタが静的初期化時に `do_read()` を実行し、stdin から最大 1MB を自前バッファへ読み込む。Scanner を使わないファイルでも io.cpp を include した時点で cin / scanf との併用が壊れる。使う側の注意として明記するか、初回読み込みを遅延させた方が安全である。
- **[注意] EOF 後の読み込みでバッファのゴミを返す**。EOF 到達後(n_written=0)にさらに next_char を呼ぶと n_read だけが進み、`n_read == n_written` の等値判定をすり抜けて `buffer[n_read]` の古い内容を返す。入力より多くのトークンを読もうとしたとき、失敗せず無言でゴミを解析する。判定を `n_read >= n_written` にすれば防げる。
- **[軽微]** グローバル変数 `scanner` と非 inline の自由関数 `read_int64()` をヘッダ相当のファイルに定義している。複数訳単位で ODR 違反になる(expander 経由の単一ファイル運用なら実害なし)。
- **[軽微]** read_int64 は `+` 符号を受け付けない。buffer の要素が signed char のまま int へ渡るため、非 ASCII バイトで `isspace` に負値(EOF 以外)を渡す UB になり得る(競プロ入力では実害なし)。

## print.cpp

- 前方宣言を先にまとめて置くことで、ネストしたコンテナ(vector<pair> 等)の相互参照が解決される設計。各 operator<< の実装は正しい。
- **[注意] 非テンプレート関数が inline でない**。to_red 系、spacefill(string)、zfill、bin、to_string_int128/uint128、__int128 系の operator<< は、#pragma once があっても複数訳単位から include すると ODR 違反(多重定義)になる。単一ファイル提出では問題ないが、`inline` を付けておくのが安全である。PRINT_RED 等は static なので問題ない。
- **[軽微]** `operator<<(ostream&, __int128_t)` は最小値(-2^127)で `x = -x` が符号あふれ(UB)。
- **[軽微]** unordered_set / unordered_map は set / map に詰め替えて出力するため、要素に operator< が必要になる(ソート出力の意図なら仕様)。stack / priority_queue はコピーして全 pop するため O(n log n) だがデバッグ用途なので許容。
- **[軽微]** これらの operator<< はグローバル名前空間にあるため、titan23 内のテンプレートから使う場合は「定義より前に print.cpp が include されている」ことが前提になる(ADL では見つからない)。現状ライブラリ各所の include 順はこの前提を満たしている。

## util.cpp

- 4関数(discard_vec / remove_vec / contains_vec / index)のロジックは正しい。いずれも O(n)。
- **[軽微]** `namespace titan23` の外にある。特に `index` は POSIX の同名関数(strings.h)とグローバル名前空間で衝突し得る名前で、`using namespace std` と組み合わせた際の曖昧さの種になる。titan23 に入れるか名前を変えるのが安全である。
