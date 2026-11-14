# mo.cpp

## 一括処理の関数

- run_light、run、run_range は、追加したクエリを一括処理する。計算量は O(n√q) である。
- 各関数にはラムダ式を渡す。run_light は 3 個、run と run_range は 5 個を受け取る。
- クエリは hilbertorder で求めた順序に並べ替えてから処理する。

## run_range の呼び出し規約

- 現在見ている区間 [nl, nr) を、各クエリ [l, r) に合わせて伸縮させる。
- add_left(i, r) は i を左端に追加する。区間は [i, r) になる。
- add_right(l, i) は i を右端に追加する。区間は [l, i+1) になる。
- del_left(i, r) は i を左端から削除する。区間は [i+1, r) になる。
- del_right(l, i) は i を右端から削除する。区間は [l, i) になる。

## 参考

- https://take44444.github.io/Algorithm-Book/range/mo/main.html
