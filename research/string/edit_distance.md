# edit_distance.cpp

## 入力

- s, t は文字列か、size() と添字アクセスを持つ列である。要素は == で比べる。
- 入力はコピーしない。string と string_view はバイト単位で扱い、C 文字列は終端 NUL までを列とする。
- as_sequence は C 文字列の終端 NUL を除き、string と string_view は明示された長さを保つ。その他の列は参照のまま扱い、要素をコピーしない。
- 各入力の長さは INT_MAX 未満である必要がある。満たさない場合は length_error を送出する。
- edit_distance_bounded と edit_distance_restore_bounded に C 文字列を渡すと、長さの取得に O(|s|+|t|) 時間かかる。

## 編集操作の表し方

- EditOperation は編集前の s, t の添字で編集操作を表す。一致する要素は含めない。
- Insert は s[source_index] の直前に t[target_index] を挿入する。source_index は末尾を指してもよい。
- Delete は s[source_index] を削除する。target_index は t の対応する境界を表す。
- Replace は s[source_index] を t[target_index] に置換する。

## 計算量

- edit_distance は n = |s|, m = |t| として O(nm+n+m) 時間、O(min(n,m)+1) 追加空間である。
- edit_distance_bounded は n = min(|s|, |t|), m = max(|s|, |t|), k >= 0 として O((n+1) * min(m+1, 2k+1)) 時間、O(min(m+1, 2k+1)) 追加空間である。
- edit_distance_within は、k >= max(|s|, |t|) なら距離を計算せずに true を返す。
- edit_distance_restore は n = |s|, m = |t| として O((n+1)(m+1)) 時間・空間である。戻り値には別に O(n+m) 空間を使う。
- edit_distance_restore_bounded は n = |s|, m = |t|, k >= 0 として O((n+1) * min(m+1, 2k+1)) 時間・追加空間である。戻り値には別に O(k) 空間を使う。

## 復元

- edit_distance_restore は一致する要素を省く。戻り値の size() は編集距離に等しい。
- 逆向きの復元では、一致、置換、削除、挿入の順に優先する。
- edit_distance_restore_bounded の入力、操作の添字、同点時の優先順は edit_distance_restore と同じである。
- edit_distance_restore_bounded は、距離 0 のとき空の vector を持つ optional を返す。
