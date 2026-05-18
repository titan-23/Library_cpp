# ビームサーチ高速化メモ：残タスク

beam_fast.cpp の Action プール化（世代ブロック arena ＋ ActionId ハンドル化）
および確定接頭辞バルク解放は実装済み。

解放基準: ある世代の処理後 `L = turn - max(lca_dist)`（その世代の全葉の
global-LCA 深さ）。次世代以降の DFS が触れる最小深さは L で単調非減少なので、
深さ < L は二度と参照されない。L 未満のブロックを解放し、確定一本道の
Action は解放直前に trace から result_prefix へ退避。メモリは生存窓×W に収まり、
最終解 = result_prefix ＋ 未確定サフィックス再構築で挙動完全不変設計。

旧 cd 連動解放の不具合（ツアー構築後に cd が進み確定済み深さを再走→解放済み
deref で SEGV）は、解放基準を「実際の DFS 最小到達深さ L」に直結させて解消。

未検証: record_history JSON の A/B（beam_search.cpp とノード集合・status の
一致、または旧版とのバイト一致）で要確認。

---

## 残（任意・低優先）

- **tour/trace の 8B 末尾 trim**: メモリは既に生存窓×W に収まる。さらに
  `tour/trace/leaf` を確定接頭辞ぶん詰めて再番号付けすれば残る 8B×窓ぶんも
  削れるが、Euler インデックス全体の前提が変わり高リスク・低価値。見送り。
- memcpy バルクコピー／解放を入れるなら
  `if constexpr (is_trivially_copyable_v<Action>)` で内部分岐し、非 POD は
  ムーブで1個ずつフォールバック。POD のときだけ追加で速い形にする。
