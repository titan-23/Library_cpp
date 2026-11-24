# dynamic_lazy_segment_tree_util.cpp

## 前提

- 第 1 テンプレート引数は値の型で、添字と長さの型は既定で long long です。
- 和を扱う木には、値が 0 でも 1 要素分の長さを含む初期値を渡します。
- mapping(F,S) は ACL と同じ形式です。
- 各型の四則演算、比較、コピーは O(1) とします。
- 和や長さを含む計算結果は、その型の範囲に収まるものとします。

## 使用例

```cpp
LazySegAddSum<long long> sums(1000000000LL, {0, 1});
sums.apply(10, 20, -3); auto result = sums.prod(0, 100); // {-30, 100}
LazySegSetMin<long long> minima(1000000000LL, 0);
minima.apply(10, 20, Assign<long long>::set(-5));
LazySegAffineSum<long long> affine(100, {2, 1});
affine.apply(0, 10, {3, 4}); // x を 3*x+4 に変換
LazySegSetAddStats<long long> stats(100, {2, 2, 2, 1});
stats.apply(10, 20, SetAdd<long long>::set(5));
LazySegAddAPSum<long long> ap(100, {0, 0, 1});
ap.apply(l, r, {x - d*l, d}); // 初項 x、公差 d を加算
auto total = ap.prod(l, r).eval(l); // APSum.sum は実際の区間和ではない
```

## APSum

- 始点を origin とした場合の和を sum+slope*origin で表します。
- 実際の和だけでなく、sum、slope、途中の積も T の範囲に収まる必要があります。
- dense 版の S_AP.val と異なり、実際の値は prod(l,r).eval(l) や get(i).eval(i) で取得します。
- all_prod().eval(0) は全体の和です。初期値 v には {v,0,1} を渡します。
- 点代入でも set(i,{v,0,1}) のように 1 要素の長さを渡します。
- max_right(l,pred) では s.eval(l) を、min_left(r,pred) では s.eval(r-s.len) を判定します。

## APAdd

- a+b*i を加算する作用です。i は [0,N) での絶対位置を表します。
