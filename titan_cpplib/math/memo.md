# titan_cpplib/math 概要メモ

`namespace titan23` の数論と有理数のライブラリです。$d(n)$ は $n$ の約数の個数を表します。

---

## [big_int.cpp](./big_int.cpp)

`class BigInt` は符号付きの多倍長整数です。絶対値を基数 $10^8$ の little-endian 配列で保持し、符号は配列とは別に $-1,0,1$ で保持します。
$10^8$ は最大桁数ではなく、配列の 1 要素が 10 進 8 桁を表すことを意味します。値がゼロの場合は符号が 0 に正規化されます。

| API | 機能 |
|---|---|
| `BigInt(integer)` / `BigInt(string_view)` | 組み込み整数または符号付き 10 進文字列から構築 |
| `to_string()` / `to_integral<T>()` | 10 進文字列への変換 / 組み込み整数への変換 |
| `sign()` / `is_zero()` / `is_one()` / `is_neg_one()` / `is_odd()` / `abs()` | 符号や値の判定、絶対値 |
| `+ - * / %`、複合代入、`++ --` | 整数の算術演算 |
| `== != < <= > >=` | 比較 |
| `divmod(a,b)` | 商と剰余を一度に計算 |
| `pow(a,n)` / `gcd(a,b)` / `lcm(a,b)` | 累乗、最大公約数、最小公倍数 |
| `operator<<` / `operator>>` | 10 進表記での入出力 |

乗算では、2 数のサイズとその比に応じて次の方法を自動的に選択します。

- 係数をまとめて正規化する通常の方法
- 任意長の Karatsuba 法
- ACL の 2 つの法による NTT
- サイズの異なる 2 数の積を分割して計算する方法

平方では、通常の方法、Karatsuba 法、NTT のそれぞれで平方専用の処理を使用します。
NTT では各 limb を基数 $10^4$ に分割し、2 つの法による畳み込みの結果を CRT で復元します。

除算では次の方法を自動的に選択します。

- 除数が 1 limb の場合に、逆数の乗算で行う除算
- 商が 1 limb の場合に特化した除算
- Knuth Algorithm D
- Burnikel–Ziegler 法

`gcd` には、上位の limb の値から Euclid の互除法の反復を複数回まとめて計算する Lehmer 法を使用します。
Lehmer 法で値を小さくした後、十分大きな値が残る場合は Boost の `cpp_int` に変換して計算します。
共通因子が大きい場合は小さい商から共通因子を復元し、基数変換のコストを抑えます。
ACL と Boost Multiprecision に依存し、どちらもヘッダのみで利用できます。内部表現と 10 進表記での入出力は基数 $10^8$ のままです。

除算は C++ の符号付き整数と同じく、商を 0 方向に切り捨てます。常に `a == b*q + r` と `abs(r) < abs(b)` が成り立ち、剰余は 0 または被除数 `a` と同じ符号です。ゼロ除算では `domain_error` を送出します。

`pow` の指数には組み込み整数と `BigInt` を指定でき、どちらも二分累乗法で計算します。負の指数では `domain_error` を送出し、`pow(0, 0)` は 1 を返します。

---

## [divisor.cpp](./divisor.cpp)

約数と素因数分解に関する自由関数です。

| 関数 | 機能 | 計算量 |
|---|---|---|
| `get_divisors(ll n)` | $n$ の約数を昇順にすべて列挙 | $O(\sqrt{n})$ |
| `factorization(ll n)` | $n$ を素因数分解し、`{素因数, 指数}` の列を返す | $O(\sqrt{n})$ |
| `divisors_num(ll n)` | $n$ の約数の個数 | $O(\sqrt{n})$ |
| `divisors_num_all(int n)` | $1..n$ の各値の約数の個数 | $O(n \log n)$ |
| `divisors_sum_all(int n)` | $1..n$ の各値の約数の総和 | $O(n \log n)$ |
| `primefactor_num(int n)` | $1..n$ の各値の相異なる素因数の個数 | $O(n \log \log n)$ |
| `get_primelist(int n)` | $n$ 以下の素数列 | $O(n \log \log n)$ |
| `factorization_eratos(int n, primes)` | 素数表を渡して素因数分解 | $O(\sqrt{n} / \log\sqrt{n} + \log n)$ |

---

## [fraction.cpp](./fraction.cpp)

`template<typename T> class Fraction` は有理数を既約分数 $p/q$ で表します。構築時に分母を正に正規化し、約分します。ゼロ除算で生じる $p/0$ は $\pm1/0$ または $0/0$ として保持します。

- 算術: `+ - * / += -= *= /=` は Fraction 同士とスカラ T の両方に対応します。単項 `-` と `abs()` もあります。
- 比較: `< > <= >= == !=` は交差乗算により $O(1)$ で比較します。
- 変換: `explicit operator double / long double`、`operator<<`、`to_string()`
- `inv()` 逆数 / $O(1)$
- `pow(ll k)` $k$ 乗、負の指数にも対応 / $O(\log k)$
- `hash()` と `std::hash<Fraction>` の特殊化があり、unordered コンテナで使用できます。

各算術演算では構築時に約分するため、$O(\log \min(p,q))$ の時間がかかります。

---

## [get_primelist.cpp](./get_primelist.cpp)

素数を列挙する 2 種類の自由関数です。`divisor.cpp` 内の同名関数とは別の実装で、区間篩も含みます。

| 関数 | 機能 | 計算量 |
|---|---|---|
| `get_primelist(int n)` | $n$ 以下の素数列 | 時間 $O(n \log \log n)$ |
| `get_primelist_range(ll l, ll r)` | $[l, r)$ の素数列 | 時間 $O(\sqrt{r} + (r-l) \log \log r)$、空間 $O(\sqrt{r} + (r-l))$ |

---

## [is_primell.cpp](./is_primell.cpp)

`bool is_primell(ll n)` は Miller-Rabin 法で素数を判定する関数です。long long の全範囲で決定的に正しい結果を返します。`math.cpp` の `pow_mod` に依存します。計算量は実質 $O(\log^3 n)$ です。

---

## [math.cpp](./math.cpp)

数論ユーティリティの自由関数です。`namespace titan23` の外で定義されている点に注意してください。

| 関数 | 機能 | 計算量 |
|---|---|---|
| `isqrt(i128 v)` | 整数平方根 $\lfloor\sqrt{v}\rfloor$ | $O(1)$ |
| `solve_quadratic_equation<T>(a,b,c)` | 二次方程式の $2$ つの実数解、判別式が $<0$ なら false | $O(1)$ |
| `pow_mod<T>(a,b,mod)` | $a^b \bmod m$ | $O(\log b)$ |
| `pow(ll a, ll b)` | $a^b$、mod なし、オーバーフローに注意 | $O(\log b)$ |
| `factorial(ll x)` | $x!$、mod なし | $O(x)$ |

---

## [mod_comb.cpp](./mod_comb.cpp)

`template<typename mint> class ModComb` は AtCoder Library の `modint` を前提として二項係数を求めるクラスです。コンストラクタ `ModComb(int n)` で階乗、階乗の逆元、逆元を $O(n)$ で前計算します。

| メソッド | 機能 | 計算量 |
|---|---|---|
| `nPr(n,r)` | 順列の数 | $O(1)$ |
| `nCr(n,r)` | 二項係数 | $O(1)$ |
| `nHr(n,r)` | 重複組合せ | $O(1)$ |
| `fact / factinv / inv (n)` | 各テーブルの参照 | $O(1)$ |

$r<0$ や $n<r$ などの範囲外では $0$ を返します。

---

## [osak.cpp](./osak.cpp)

`class Osa_k` は最小素因数の表を使用して素因数分解を行うクラスです。前計算した最大値 $n$ までの整数を扱うことができます。`run_length_encoding.cpp` に依存します。

- コンストラクタ `Osa_k(int n)`: 前計算の時間 $O(n \log \log n)$、空間 $O(n)$

| メソッド | 機能 | 計算量 |
|---|---|---|
| `p_factorization(int n)` | $n$ の素因数を重複も含めて列挙 | $O(\log n)$ |
| `p_factorization_map(int n)` | `{素因数: 指数}` の map | $O(\log n)$ |
| `get_divisors(int n)` | $n$ の約数を昇順に列挙 | $O(\log n + d(n) \log d(n))$ |

---

## [pollard_rho.cpp](./pollard_rho.cpp)

`class PollardRho` は Pollard の rho 法で大きな整数を素因数分解するクラスです。`math.cpp` と `is_primell.cpp` に依存し、`__int128_t` で計算します。long long の全範囲の合成数を分解できます。

- `factorize(ll n)` → `map<素因数, 指数>` / 期待計算量 $O(n^{1/4} \mathrm{polylog}\, n)$

---

## [prime_factorizer.cpp](./prime_factorizer.cpp)

`class PrimeFactorizer` は $\sqrt{n}$ 以下の素数表を使用して素因数分解を行うクラスです。`osak.cpp` より大きな long long の $n$ に対応します。構築時に渡した $n$ が実質的な上限です。

- コンストラクタ `PrimeFactorizer(ll n)`: 前計算の時間 $O(\sqrt{n} \log \log \sqrt{n})$、空間 $O(\sqrt{n})$

| メソッド | 機能 | 計算量 |
|---|---|---|
| `p_factorization(ll n)` | $n$ の素因数を重複も含めて列挙 | $O(\sqrt{n} / \log\sqrt{n})$ |
| `p_factorization_map(ll n)` | `{素因数: 指数}` の map | $O(\sqrt{n} / \log\sqrt{n})$ |
| `get_divisors(ll n)` | $n$ の約数を昇順に列挙 | $O(\sqrt{n} / \log\sqrt{n} + d(n) \log d(n))$ |

---

## [stern_brocot_tree.cpp](./stern_brocot_tree.cpp)

`template<class T> class SternBrocotTree` は Stern-Brocot 木を扱うクラスです。`Node` は開区間 $(p/q,\ r/s)$ を保持し、その値は $(p+r)/(q+s)$ です。

| メソッド | 機能 | 計算量 |
|---|---|---|
| `Node::den() / num()` | このノードの分母 / 分子 | $O(1)$ |
| `Node::left(d) / right(d)` | 左 / 右の子へ $d$ 段進む | $O(1)$ |
| `Node::range()` | 保持する開区間 | $O(1)$ |
| `get_node(p,q)` | 既約化した $p/q$ に対応するノード | $O(\log(p+q))$ |
| `encode_path(a)` | 根からのパスを `{L/R, 連続回数}` で符号化 | $O(\text{深さ})$ |
| `decode_path(P)` | 符号化したパスからノードを復元 | $O(\|P\|)$ |
| `lca(a,b)` | $2$ ノードの最小共通祖先 | $O(\text{深さ})$ |
| `ancestor(node,k)` | $node$ の深さ $k$ の祖先 | $O(\text{深さ})$ |
| `binary_search(f,d)` | 分母と分子が $\le d$ を満たす範囲で、単調な述語 $f$ の境界の分数を探索 | $O(\log d)$ |

`binary_search` は $f(0/1) \neq f(1/0)$ を前提とします。
