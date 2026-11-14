# dynamic_segment_tree_util.cpp

## 使い方

- 別名の第1テンプレート引数は値の型で、添字の型は既定で long long である。
- 例を示す。

```cpp
SegSum<long long> sparse(1000000000LL);          // 初期値は0
SegSumInit<long long> uniform(1000000000LL, -3); // 初期値は-3
uniform.set(42, 5); auto sum = uniform.prod(0, 100);
```

## 型への前提

- 各型の四則演算、比較、ビット演算は O(1) とし、計算結果はその型の範囲に収める。
- Gcd は非負値を扱う。計算量の V は max(1,a,b) を表す。
- Xor、Or、And は整数型を使う。
- Idx の添字と Cnt の個数も Index で保持する。個数の計算結果は Index の範囲に収める。

## 要素の渡し方

- Cnt の各要素は {値,1} として渡す。
- Idx の各要素は {値,添字} として渡す。
- Idx の Init 版は、渡した {値,添字} を反復する。各位置の添字は自動生成しない。

## 返す値

- SegAffine の prod(l,r).eval(x) は f_{r-1}(...f_l(x)...) を返す。
- SegMaxSubSum の prod(l,r).ans は、空区間を許す最大部分配列和を返す。

## pow と計算量

- pow の n は非負である。
- Affine 以外の反復集約は O(1) である。
- Affine の反復集約は、一般の環でも使える二分累乗で O(logn) である。
- SegAffineInit の set、prod、max_right、min_left は O(log²N)、all_prod は O(logN) である。
