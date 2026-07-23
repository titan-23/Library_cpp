# titan_cpplib/math レビュー

対象は10ファイル(memo.md は対象外)。

- divisor.cpp
- fraction.cpp
- get_primelist.cpp
- is_primell.cpp
- math.cpp
- mod_comb.cpp
- osak.cpp
- pollard_rho.cpp
- prime_factorizer.cpp
- stern_brocot_tree.cpp

重要度は3段階。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる
- **[軽微]** 動作に影響しない指摘

## バグ一覧(要約)

| ファイル | 内容 |
|---|---|
| divisor.cpp / get_primelist.cpp | 同名同シグネチャの get_primelist が両方にあり、同時 include で多重定義 |

## 横断事項

- **[軽微] 非 inline の自由関数をヘッダ相当のファイルに定義**している(divisor、get_primelist、is_primell、math)。複数訳単位で ODR 違反になる。単一ファイル提出なら実害なし。
- **[軽微]** math.cpp と is_primell.cpp のグローバル名(pow_mod、pow、i128、isqrt 等)が `namespace titan23` の外にある(math.cpp 全体が名前空間外)。

## divisor.cpp

- get_divisors / factorization / divisors_num / divisors_num_all / divisors_sum_all / primefactor_num / factorization_eratos とも正しい。
- **[バグ] get_primelist の重複定義**。get_primelist.cpp と同名・同シグネチャの関数がここにもあり、両ファイルを include すると多重定義になる。divisor.cpp 側を削除するか名前を分けるべき。
- **[軽微]** factorization 内の `if (n == 1) break;` は冗長。

## fraction.cpp

- 有理数クラス。符号の正規化(q<0 の反転)、q=0 の ±inf 表現、既約化、pow、hash 特殊化とも正しい。
- **[注意] オーバーフロー**。加減乗除と比較(`p*other.q < other.p*q`)が分子分母の積を取るため、T=long long では値域が √(9e18) 程度に制限される。ライブラリの性質上避けにくいが、コメントに明記がない。
- **[注意]** ±inf(q=0)同士や inf を含む比較は、交差積の計算上、意味が保証されない組合せがある(+inf < +inf が false になるのは偶然正しいが、0*x 項に依存)。inf を使うなら比較の仕様を決めてコメント化した方がよい。
- **[軽微]** p, q が public のため、直接書き換えると既約・符号の不変条件が壊れる。

## get_primelist.cpp

- エラトステネス、区間篩(get_primelist_range)とも正しい。区間篩は l≦1 の境界処理、p 自身を消さない開始位置 `max(2, ceil(l/p))*p` を確認した。sqrt の誤差も +1 の余裕で問題ない。

## is_primell.cpp

- Miller-Rabin のロジックは正しい。判定基底 {2,7,61}(n < 4,759,123,141)と7基底(64bit 全域)の切り替え、`n <= a` の早期 true が誤判定を生まないこと(基底2の最小強擬素数 2047 > 61)も確認した。

## math.cpp

- isqrt は正しい(long double 経由+補正ループ)。pow_mod / pow / factorial も前提の範囲で正しい。
- **[注意] pow_mod<long long> は mod が大きいとオーバーフロー**。`a*a % mod` のため mod が約 3.0e9 を超えると long long で溢れる。int128 でインスタンス化する前提(is_primell の使い方)をコメントに書くべき。
- **[注意] solve_quadratic_equation は未完成に近い**。判別式に `sqrt`(double)を使い、整数 T では `(-b±v)/2` の整数除算で切り捨てる。`v*v == D` の検証もなく、整数解の存在判定にも実数解にも使えない中途半端な仕様。用途を明確にするか作り直しが要る。
- **[軽微]** `long long pow(a, b)` はグローバルで std::pow と同名。整数引数なら完全一致でこちらが選ばれるため動くが、名前汚染の種。b が負のとき無限ループに近い挙動。factorial は x≧21 で silently オーバーフロー。
- **[軽微]** 名前空間外・`<bits/stdc++.h>`・グローバル `using i128`。

## mod_comb.cpp

- 階乗・逆元・逆階乗の前計算、nPr / nCr / nHr とも正しい。`_inv[i] = -_inv[mod%i] * (mod/i)` の定石も正しい。
- **[注意]** n+1 ≧ mod のとき逆元テーブルが破綻する(mod が素数でも i が mod の倍数で inv が 0 になる)。mod より大きい n を渡さない前提の明記か assert が要る。
- **[軽微]** nPr / nCr は上限側の範囲チェックがなく、コンストラクタで確保したサイズを超える n を渡すと範囲外参照になる。

## pollard_rho.cpp

- 構成: Miller-Rabin の証人から因数を取り出し、失敗時に Floyd 循環検出の ρ 法へフォールバックする方式。factorize の分解ループも含めロジックは正しい。
- **[注意] -std=c++20(strict)でコンパイルできない可能性**。内部で `std::gcd` と `std::abs` を `__int128_t` に対して使う。GCC では `-std=gnu++20` なら `__int128` が整数型扱いだが、`-std=c++20` では `is_integral<__int128>` が false になり `std::gcd` の static_assert で落ちる。リポジトリの標準フラグは `-std=c++20` のため要注意。
- **[注意] 実効的な対応範囲は n < 2^63 程度**。内部型は __int128 だが、(1) MR 基底集合は n < 2^64 でのみ決定的、(2) `is_primell(f)` が long long 引数のため f ≧ 2^63 で切り捨てが起きる。クラスコメントに範囲を明記すべき。
- **[軽微]** フォールバックの ρ 法は Brent 加速もバッチ gcd もない素朴版で、大きい素因数どうしの積では遅い。bit_length は __int128 を unsigned long long に切り詰めるが、渡すのは n-1 の最下位ビットなので上記範囲内では問題ない。P200 の試し割りはコメントアウトされた死にコード。

## prime_factorizer.cpp

- √n までの素数表による素因数分解・約数列挙。正しい。
- **[注意]** primelist が `vector<int>` のため、n が約 4.6e18(int 最大値の平方)を超えると素数が int に収まらず壊れる。また n がその規模だと篩のメモリが数百 MB になる。実用範囲(n ≦ 1e18 程度)では問題ない。

## stern_brocot_tree.cpp

- Stern-Brocot 木。get_node の連分数降下、encode_path / decode_path / lca / ancestor を確認した。正しい。binary_search も標準的な「方向ごとに倍加+二分探索」の形で、境界の返し方(node.p/q が左側、node.r/s が右側)も仕様通り。
- **[注意] get_node(0, q) はゼロ除算**。0/1 は木に存在しないため契約外だが、assert がない。p, q ≧ 1 の前提を明記すべき。
- **[軽微]** binary_search の計算量コメント O(log d) は、f の呼び出し回数としては O(log² d) が正確。
