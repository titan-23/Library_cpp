# titan_cpplib/math 概要メモ

`namespace titan23` の数論・有理数ライブラリ。$d(n)$ は $n$ の約数の個数。

---

## [divisor.cpp](./divisor.cpp)

約数・素因数分解まわりの自由関数群。

| 関数 | 機能 | 計算量 |
|---|---|---|
| `get_divisors(ll n)` | $n$ の約数を昇順に全列挙 | $O(\sqrt{n})$ |
| `factorization(ll n)` | $n$ を素因数分解、`{素因数, 指数}` の列 | $O(\sqrt{n})$ |
| `divisors_num(ll n)` | $n$ の約数の個数 | $O(\sqrt{n})$ |
| `divisors_num_all(int n)` | $1..n$ 各値の約数個数 | $O(n \log n)$ |
| `divisors_sum_all(int n)` | $1..n$ 各値の約数総和 | $O(n \log n)$ |
| `primefactor_num(int n)` | $1..n$ 各値の相異なる素因数の種類数 | $O(n \log \log n)$ |
| `get_primelist(int n)` | $n$ 以下の素数列 | $O(n \log \log n)$ |
| `factorization_eratos(int n, primes)` | 素数表を渡して素因数分解 | $O(\sqrt{n} / \log\sqrt{n} + \log n)$ |

---

## [fraction.cpp](./fraction.cpp)

`template<typename T> class Fraction` — 既約有理数 $p/q$。構築時に分母を正へ正規化し約分する。ゼロ除算は $p/0$ を $\pm1/0$、$0/0$ として保持。

- 算術: `+ - * / += -= *= /=`。Fraction同士とスカラ T の両方に対応。単項 `-`、`abs()`
- 比較: `< > <= >= == !=`、交差乗算で $O(1)$
- 変換: `explicit operator double / long double`、`operator<<`、`to_string()`
- `inv()` 逆数 / $O(1)$
- `pow(ll k)` $k$ 乗、負指数可 / $O(\log k)$
- `hash()` と `std::hash<Fraction>` 特殊化。unordered コンテナで使用可

各算術演算は構築時に約分するため $O(\log \min(p,q))$ かかる。

---

## [get_primelist.cpp](./get_primelist.cpp)

素数列挙2種の自由関数。`divisor.cpp` 内の同名関数とは別実装で、区間篩を持つ。

| 関数 | 機能 | 計算量 |
|---|---|---|
| `get_primelist(int n)` | $n$ 以下の素数列 | 時間 $O(n \log \log n)$ |
| `get_primelist_range(ll l, ll r)` | $[l, r)$ の素数列 | 時間 $O(\sqrt{r} + (r-l) \log \log r)$、空間 $O(\sqrt{r} + (r-l))$ |

---

## [is_primell.cpp](./is_primell.cpp)

`bool is_primell(ll n)` — Miller-Rabin 素数判定。long long 全域で決定的に正しい。`math.cpp` の `pow_mod` に依存。計算量は実質 $O(\log^3 n)$。

---

## [math.cpp](./math.cpp)

数論ユーティリティの自由関数群。`namespace titan23` の外にある点に注意。

| 関数 | 機能 | 計算量 |
|---|---|---|
| `isqrt(i128 v)` | 整数平方根 $\lfloor\sqrt{v}\rfloor$ | $O(1)$ |
| `solve_quadratic_equation<T>(a,b,c)` | 二次方程式の $2$ 実数解、判別式 $<0$ は false | $O(1)$ |
| `pow_mod<T>(a,b,mod)` | $a^b \bmod m$ | $O(\log b)$ |
| `pow(ll a, ll b)` | $a^b$、mod なし。オーバーフロー注意 | $O(\log b)$ |
| `factorial(ll x)` | $x!$、mod なし | $O(x)$ |

---

## [mod_comb.cpp](./mod_comb.cpp)

`template<typename mint> class ModComb` — AtCoder Library `modint` 前提の二項係数。コンストラクタ `ModComb(int n)` で階乗・階乗逆元・逆元を $O(n)$ で前計算。

| メソッド | 機能 | 計算量 |
|---|---|---|
| `nPr(n,r)` | 順列数 | $O(1)$ |
| `nCr(n,r)` | 二項係数 | $O(1)$ |
| `nHr(n,r)` | 重複組合せ | $O(1)$ |
| `fact / factinv / inv (n)` | 各テーブル参照 | $O(1)$ |

$r<0$ や $n<r$ などの範囲外は $0$ を返す。

---

## [osak.cpp](./osak.cpp)

`class Osa_k` — 最小素因数テーブルによる素因数分解。前計算した最大値 $n$ までの整数を扱える。`run_length_encoding.cpp` に依存。

- コンストラクタ `Osa_k(int n)`: 前計算 時間 $O(n \log \log n)$、空間 $O(n)$

| メソッド | 機能 | 計算量 |
|---|---|---|
| `p_factorization(int n)` | $n$ の素因数を重複ありで列挙 | $O(\log n)$ |
| `p_factorization_map(int n)` | `{素因数: 指数}` の map | $O(\log n)$ |
| `get_divisors(int n)` | $n$ の約数を昇順列挙 | $O(\log n + d(n) \log d(n))$ |

---

## [pollard_rho.cpp](./pollard_rho.cpp)

`class PollardRho` — Pollard's rho による大きな整数の素因数分解。`math.cpp`、`is_primell.cpp` に依存し `__int128_t` で計算。long long 全域の合成数を分解できる。

- `factorize(ll n)` → `map<素因数, 指数>` / 期待 $O(n^{1/4} \mathrm{polylog}\, n)$

---

## [prime_factorizer.cpp](./prime_factorizer.cpp)

`class PrimeFactorizer` — $\sqrt{n}$ 以下の素数表を使う素因数分解。`osak.cpp` より大きな long long の $n$ に対応。構築時に渡した $n$ が実質の上限。

- コンストラクタ `PrimeFactorizer(ll n)`: 前計算 時間 $O(\sqrt{n} \log \log \sqrt{n})$、空間 $O(\sqrt{n})$

| メソッド | 機能 | 計算量 |
|---|---|---|
| `p_factorization(ll n)` | $n$ の素因数を重複ありで列挙 | $O(\sqrt{n} / \log\sqrt{n})$ |
| `p_factorization_map(ll n)` | `{素因数: 指数}` の map | $O(\sqrt{n} / \log\sqrt{n})$ |
| `get_divisors(ll n)` | $n$ の約数を昇順列挙 | $O(\sqrt{n} / \log\sqrt{n} + d(n) \log d(n))$ |

---

## [stern_brocot_tree.cpp](./stern_brocot_tree.cpp)

`template<class T> class SternBrocotTree` — Stern-Brocot 木。`Node` は開区間 $(p/q,\ r/s)$ を保持し、値は $(p+r)/(q+s)$。

| メソッド | 機能 | 計算量 |
|---|---|---|
| `Node::den() / num()` | このノードの分母 / 分子 | $O(1)$ |
| `Node::left(d) / right(d)` | 左 / 右の子へ $d$ 段進む | $O(1)$ |
| `Node::range()` | 保持する開区間 | $O(1)$ |
| `get_node(p,q)` | 既約化した $p/q$ に対応するノード | $O(\log(p+q))$ |
| `encode_path(a)` | 根からのパスを `{L/R, 連続回数}` で符号化 | $O(\text{深さ})$ |
| `decode_path(P)` | パス符号からノード復元 | $O(\|P\|)$ |
| `lca(a,b)` | $2$ ノードの最小共通祖先 | $O(\text{深さ})$ |
| `ancestor(node,k)` | $node$ の深さ $k$ の祖先 | $O(\text{深さ})$ |
| `binary_search(f,d)` | 分母分子 $\le d$ で単調述語 $f$ の境界分数を探索 | $O(\log d)$ |

`binary_search` は $f(0/1) \neq f(1/0)$ が前提。
