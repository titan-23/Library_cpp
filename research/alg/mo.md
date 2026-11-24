# mo.cpp

## 一括処理の関数

- run_light、run、run_range は、追加したクエリを一括処理します。計算量は O(n√q) です。
- 各関数にはラムダ式を渡します。run_light は 3 個、run と run_range は 5 個を受け取ります。
- クエリは hilbertorder で求めた順序に並べ替えてから処理します。

## run_range の呼び出し規約

- 現在の区間 [nl, nr) を、各クエリ [l, r) に合わせて伸縮させます。
- add_left(i, r) は i を左端に追加します。追加後の区間は [i, r) です。
- add_right(l, i) は i を右端に追加します。追加後の区間は [l, i+1) です。
- del_left(i, r) は i を左端から削除します。削除後の区間は [i+1, r) です。
- del_right(l, i) は i を右端から削除します。削除後の区間は [l, i) です。

## 参考

- https://take44444.github.io/Algorithm-Book/range/mo/main.html
