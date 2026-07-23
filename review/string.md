# titan_cpplib/string レビュー

全9ファイル精査済み。

重要度は3段階。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる
- **[軽微]** 動作に影響しない指摘

このディレクトリは ds に比べて健全で、明確なバグは hash_string.cpp の正規化 off-by-one のみ。

## aho_corasick.cpp

- 構築(BFS による fail 計算、goto 補完、cnt の fail 連鎖累積)は正しい。BFS 順により fail 先が先に処理される点も確認した。
- **[注意] build() 後の add_string は不正**。build() で child が goto 関数(欠損遷移を fail 先で埋める)に書き換えられるため、その後の add_string は既存遷移をパターン木の辺と誤認して壊れる。「全パターン追加後に一度だけ build」という契約をコメントに明記した方がよい。
- **[注意]** 文字が `[B, B+26)` の範囲外だと child の範囲外アクセスになる。assert がない。
- **[軽微]** trie 隣接リストは add_string のたびに重複辺を積み、build() で unique する。共有プレフィックスが多いと一時メモリが総文字数比例で膨らむ。
- **[軽微]** メンバ宣言順(node, trie, failtree, root)と初期化リスト順が不一致(-Wreorder)。`get(v)` が Node を値渡しで返す。

## dynamic_aho_corasick.cpp

- 二進分割(binary grouping)による動的化。ブロックのマージが二進カウンタとして正しく動くこと、pos と neg の差分で削除を表現する設計を確認した。追加は償却 O(|p| log q)、search は O(|s| log q)。
- **[注意]** delete_string は存在しないパターンを渡すとカウントが負に食い込む。コメントに前提の記載はあるが、assert 等の検出手段はない。
- **[軽微]** search 内の「TODO b.ahoを使ってクエリに答える」コメントは実装済みで古い。
- **[軽微]** Block のマージで文字列集合を毎回コピー・再構築する(この手法の性質上避けられない)。search の計算量コメントがない。

## eer_tree.cpp

- 回文木(EerTree)。add の get_upper、suffix_link の決定、suffix_link_dep による回文数カウント、get_freq の伝播順(suffix_link が常に小さい添字を指す)を確認した。正しい。「aa」のように空ノード経由で長さ2の回文を作る経路も机上で追跡した。
- **[注意] ノード添字の規約が2種類混在**。get_suff / get_range_from_idx / enumerate_suffix は内部番号(番兵2個を含む)を使い、idx_to_range / idx_to_string / get_freq / count_unique_palindromes は外部番号(0 始まり、内部番号-2)を使う。get_range_from_idx と idx_to_range は同じ計算で規約だけ違うため、取り違えやすい。関数コメントでの明示か、片方への統一を推奨する。

## hash_string.cpp

- Mersenne prime (2^61-1) ロールハッシュ。mul の折り畳み、acc 方式と seg 方式の切り替え、`4*MOD +` による符号なし引き算の保護は正しい。
- **[バグ] op() の正規化が off-by-one**。`if (u > HashStringBase::MOD) u -= MOD;` は `>=` が正しい。和がちょうど MOD のとき 0 に正規化されず MOD のまま残り、同じハッシュ値が 0 と MOD の2通りの表現を持つ。セグ木経路(set 使用後)の get 同士の比較で、まれに等しいハッシュを不一致と誤判定する。発生確率は極小だが理論上のバグ。unite() と mod() は `>=` で正しい。
- **[注意] コンストラクタの extend 条件が off-by-one**。`if (n > hsb->get_cap()) extend(...)` は、cap がちょうど n のとき extend せず、get(0, r) の `invb[n-r]`(r=0 で invb[n])が範囲外参照になる。必要なのは添字 n までなので、条件は `n+1 > get_cap()` 相当が正しい。HashStringBase(n) で作った直後(cap=n+1)は問題にならない。
- **[注意]** 文字を `c-'a'+1` で数値化しており、英小文字以外は壊れる。前提のコメントがない。
- **[軽微]** set を使わない場合も seg を常に構築する(O(n) の無駄)。メンバ宣言順と初期化リスト順の不一致(-Wreorder)。get_lcp は set 使用後 O(n log²n) になる。

## suffix_automaton.cpp

- 標準的な suffix automaton の構築。clone 処理(len 調整、遷移の付け替え、link の付け替え)は定石通り正しい。`a.push_back(a[q])` は同一 vector 要素の push_back だが、規格上安全な操作である。
- **[軽微]** len と link と遷移のみの骨組みで、出現回数・部分文字列数等のユーティリティはない(用途側で書く前提なら可)。'a' 基準26文字固定。

## trie.cpp

- add / count / count_prefix / erase / erase_prefix / s_prefix の整合を確認した。count は「そのノードを通過して先へ延びる文字列数」、stop_count は「そのノードで終わる文字列数」で一貫しており、erase_prefix の切断も最終文字の判定で必ず実行される。正しい。
- **[注意] 空文字列まわりの縁が不整合**。
  - contains_prefix_inv は、格納済みの空文字列を検出しない(先頭文字の child 判定が root の stop_count 判定より先に走る)。s="" のときも常に false。
  - erase_prefix("") は、空文字列が格納されていると全消去の分岐に入らず、何も消さずに個数だけ返す。
  - size() は root の count を返すため空文字列を数えない。
  - 空文字列を扱わないなら実害はない。扱わない前提をコメントに書くか、境界を直すかのどちらかにするとよい。
- **[軽微]** erase はノードの参照を切るだけで領域を再利用しない。print() の色付き出力は環境依存(ANSI エスケープ)。
