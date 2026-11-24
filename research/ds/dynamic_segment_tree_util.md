# dynamic_segment_tree_util.cpp

## 使用方法

- 別名の第 1 テンプレート引数は値の型で、添字の型は既定で long long です。

```cpp
SegSum<long long> sparse(1000000000LL);          // 初期値は 0
SegSumInit<long long> uniform(1000000000LL, -3); // 初期値は -3
uniform.set(42, 5); auto sum = uniform.prod(0, 100);
```

## 型に関する前提

- 各型の四則演算、比較、ビット演算は O(1) とし、計算結果はその型の範囲に収まるものとします。
- Gcd は非負の値を扱います。計算量の V は max(1,a,b) を表します。
- Xor、Or、And では整数型を使用します。
- Idx の添字と Cnt の個数も Index で保持します。個数の計算結果は Index の範囲に収まるものとします。

## 要素の渡し方

- Cnt の各要素は {値,1} として渡します。
- Idx の各要素は {値,添字} として渡します。
- Idx の Init 版は渡した {値,添字} を反復します。各位置の添字は自動生成しません。

## 返す値

- SegAffine の prod(l,r).eval(x) は f_{r-1}(...f_l(x)...) を返します。
- SegMaxSubSum の prod(l,r).ans は、空区間を許す最大部分配列和を返します。

## pow と計算量

- pow の n は非負です。
- Affine 以外の pow は O(1) です。
- Affine の pow は一般の環でも使用できる二分累乗で計算するため O(logn) です。
- SegAffineInit の set、prod、max_right、min_left は O(log²N)、all_prod は O(logN) です。
