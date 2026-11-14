# eer_tree.cpp

## 計算量

- 空間は O(n)、時間は O(nlogσ) である。
- 子は `map<char, int>` で持つ。26 文字の `array<int, 26>` で持つ版はコメントとして残してある。
- add の計算量は償却 O(1) と見込んでいるが、確かめていない。元のコメントは「償却O(1)のはず?」だった。
- enumerate_suffix と get_freq の計算量は元のコメントでも O(?) のままで、確かめていない。

## 頂点番号と回文番号

- 頂点 0 は長さ -1 の根、頂点 1 は長さ 0 の根である。頂点 0 の par は自身を指す。
- 回文番号は頂点番号から 2 を引いた値である。get_freq、idx_to_range、idx_to_string は回文番号を受け取る。
- get_suff と enumerate_suffix は頂点番号を返す。get_range_from_idx は頂点番号を受け取る。
- suff[i] は s[:i+1] の最長回文接尾辞に対応する頂点番号である。この回文は s[i] を含んで終わる。

## 参考

- https://math314.hateblo.jp/entry/2016/12/19/005919
- https://mojashi.hatenablog.com/entry/2017/07/17/155520
- https://arxiv.org/abs/1506.04862
