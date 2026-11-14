# dynamic_lazy_segment_tree_util.cpp

## 前提

- 第 1 テンプレート引数は値の型で、添字と長さの型は既定で long long である。
- 和を扱う木には、値が 0 でも 1 要素分の長さを含む初期値を渡す。
- mapping(F,S) は ACL と同じ形式である。
- 各型の四則演算、比較、コピーは O(1) とする。
- 和や長さを含む計算結果は、その型の範囲に収める。

## 使用例

```cpp
LazySegAddSum<long long> sums(1000000000LL, {0, 1});
sums.apply(10, 20, -3); auto result = sums.prod(0, 100); // {-30, 100}
LazySegSetMin<long long> minima(1000000000LL, 0);
minima.apply(10, 20, Assign<long long>::set(-5));
LazySegAffineSum<long long> affine(100, {2, 1});
affine.apply(0, 10, {3, 4}); // xを3*x+4に変換
LazySegSetAddStats<long long> stats(100, {2, 2, 2, 1});
stats.apply(10, 20, SetAdd<long long>::set(5));
LazySegAddAPSum<long long> ap(100, {0, 0, 1});
ap.apply(l, r, {x - d*l, d}); // 初項x、公差dを加算
auto total = ap.prod(l, r).eval(l); // APSum.sumは実際の区間和ではない
```

## APSum

- 始点を origin としたときの和を sum+slope*origin で表す。
- 実際の和だけでなく、sum と slope と途中の積も T の範囲に収める。
- dense 版の S_AP.val と異なり、prod(l,r).eval(l) や get(i).eval(i) で実際の値を得る。
- all_prod().eval(0) は全体の和である。初期値 v には {v,0,1} を渡す。
- 点代入も set(i,{v,0,1}) のように 1 要素の長さを渡す。
- max_right(l,pred) では s.eval(l) を、min_left(r,pred) では s.eval(r-s.len) を判定する。

## APAdd

- a+b*i を加算する作用である。i は [0,N) での絶対添字を表す。
