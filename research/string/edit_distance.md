# edit_distance.cpp

## 入力

- s, t は文字列か、size() と添字アクセスに対応した列です。要素は == で比較します。
- 入力はコピーしません。string と string_view はバイト単位で扱い、C 文字列は終端 NUL の直前までを列とします。
- as_sequence は C 文字列では終端 NUL を除外し、string と string_view では明示された長さをそのまま使用します。その他の列は参照のまま扱い、要素をコピーしません。
- 各入力の長さは INT_MAX 未満である必要があります。この条件を満たさない場合は length_error を送出します。
- edit_distance_bounded と edit_distance_restore_bounded に C 文字列を渡すと、長さの取得に O(|s|+|t|) 時間かかります。

## 編集操作の表し方

- EditOperation は編集前の s, t の添字で編集操作を表します。一致する要素は含めません。
- Insert は s[source_index] の直前に t[target_index] を挿入します。source_index は末尾を指してもかまいません。
- Delete は s[source_index] を削除します。target_index は t の対応する境界を表します。
- Replace は s[source_index] を t[target_index] に置換します。

## 計算量

- edit_distance は n = |s|, m = |t| として O(nm+n+m) 時間、O(min(n,m)+1) 追加空間です。
- edit_distance_bounded は n = min(|s|, |t|), m = max(|s|, |t|), k >= 0 として O((n+1) * min(m+1, 2k+1)) 時間、O(min(m+1, 2k+1)) 追加空間です。
- edit_distance_within は、k >= max(|s|, |t|) の場合は距離を計算せずに true を返します。
- edit_distance_restore は n = |s|, m = |t| として O((n+1)(m+1)) 時間・空間です。戻り値には別に O(n+m) 空間を使用します。
- edit_distance_restore_bounded は n = |s|, m = |t|, k >= 0 として O((n+1) * min(m+1, 2k+1)) 時間・追加空間です。戻り値には別に O(k) 空間を使用します。

## 復元

- edit_distance_restore は一致する要素を省きます。戻り値の size() は編集距離に等しくなります。
- 逆向きの復元では、一致、置換、削除、挿入の順に優先します。
- edit_distance_restore_bounded の入力、操作の添字、同点時の優先順は edit_distance_restore と同じです。
- edit_distance_restore_bounded は、距離が 0 の場合は空の vector を保持する optional を返します。
