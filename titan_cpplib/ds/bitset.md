# Bitset

`titan23::Bitset<N, Backend>` は、ビット数 `N` をコンパイル時に決める固定長bitsetです。C++20以上で使えます。複数集合を扱う非破壊的な操作は自由関数として提供し、更新操作はメソッドとして使えます。通常の演算子やメソッドも引き続き使えます。

```cpp
#include <cassert>
#include "titan_cpplib/ds/bitset.cpp"

int main() {
    using Bits = titan23::Bitset<4097>;
    Bits a, b;                     // 全ビット0
    a.set(0).set(64);
    a[5] = true;
    b.set(64).set(100);

    assert(a.count() == 3);
    assert(titan23::count_and(a, b) == 1); // (a & b).count()
    assert(a.intersects(b));       // 共通する1ビットがある

    auto common = a & b;
    a.or_shift_left(7);            // 更新前の a に対する a |= a << 7
    assert(a.test(7) && a.test(12) && a.test(71));
    common >>= 64;
    assert(common.test(0));
}
```

## 内部表現とデータの出し入れ

内部は `std::array<std::uint64_t, word_count>` で、動的確保はしません。`word_count` は `N / 64 + (N % 64 != 0)` です。ビット `i` はワード `i / 64` の下位から `i % 64` 番目に対応し、位置 `0` が最下位ビットです。末尾ワードのうち、位置が `N` 以上の余剰ビットは常に0に保ちます。

| API | 内容 |
|---|---|
| `Bitset()` | 全ビット0で構築する |
| `explicit Bitset(std::uint64_t value)` | 下位64ビットへ `value` を入れる。`N < 64` なら切り詰める |
| `Bitset::from_words(words)` | `storage_type` から構築し、余剰ビットを0にする |
| `words()` | 内部配列への `const storage_type&` を返す |
| `size()` / `len()` | ビット数 `N` を返す |
| `word_type` / `storage_type` | `std::uint64_t` / 内部配列の型 |
| `word_count` | 内部配列の要素数 |

```cpp
using Bits = titan23::Bitset<65>;
Bits::storage_type words{0x5, ~Bits::word_type{0}};
auto bits = Bits::from_words(words);
// 位置0、2、64だけが1になる。
assert(bits.count() == 3);
assert(bits.words()[1] == 1);
```

## 操作

集合どうしの演算には、同じ `N` と `Backend` の型を使います。更新操作は自身への参照を返すので、`a.set(1).set(2)` のようにつなげられます。

| API | 内容 |
|---|---|
| `test(i)` / `access(i)` | 位置 `i` の値を `bool` で返す |
| `a[i]` | 読み取り・代入。非const版は参照プロキシで、`a[i].flip()` も使える |
| `set(i, value = true)` / `reset(i)` / `flip(i)` | 1ビットを設定・消去・反転する |
| `set()` / `reset()` / `flip()` | 全ビットを設定・消去・反転する |
| `count()` | 1ビットの個数を返す |
| `any()` / `none()` / `all()` | 1が存在するか／すべて0か／すべて1かを返す |
| `a & b` / `a \| b` / `a ^ b` / `~a` | AND・OR・XOR・全体反転 |
| `a &= b` / `a \|= b` / `a ^= b` | 集合演算で `a` を更新する |
| `a << k` / `a >> k` | 位置の大きい側／小さい側へシフトした値を返す |
| `a <<= k` / `a >>= k` | シフトで `a` を更新する |
| `a == b` / `a != b` | 等値比較 |
| `count_and(b, c, ...)` | 自身も含む共通部分の個数。互換メソッドで、`titan23::count_and(a, b, c, ...)` と同じ |
| `intersects(b)` | `(a & b).any()` を計算し、共通ビットが見つかれば終了する |
| `or_shift_left(k)` | 更新前の値を使って `a \|= a << k` を一時bitsetなしで計算する |
| `or_shift_right(k)` | 更新前の値を使って `a \|= a >> k` を一時bitsetなしで計算する |

1ビットの操作とサイズ取得は `O(1)`、全体の走査・集合演算・シフトは最悪 `O(word_count)` です。

シフトORは部分和DPなどに使えます。以下では各重みを1回ずつ使い、和が10000以下の状態を保持します。

```cpp
titan23::Bitset<10001> reachable;
reachable.set(0);
for (std::size_t weight : {3, 7, 11}) {
    reachable.or_shift_left(weight);
}
assert(reachable.test(10));
```

境界の扱いは次のとおりです。

- `N == 0` では `count() == 0`、`any() == false`、`none() == true`、`all() == true` です。
- シフトで範囲外に出たビットは捨て、空いた位置を0にします。`k >= N` なら通常のシフト結果は全ビット0、シフトORは元の値のままです。`k == 0` は値を変えません。
- 位置を受け取るAPIは `0 <= i < N` が前提です。検査には `assert` を使い、`NDEBUG` を定義すると無効になります。範囲外アクセスで例外を投げる `std::bitset` のAPIとの互換性はありません。

## 複数入力の自由関数

複数の集合を対等な引数として渡せるよう、AND・OR・XORは自由関数でまとめて処理できます。引数は2個以上で、すべて同じ `Bitset<N, Backend>` 型を渡します。個数は可変長引数テンプレートでコンパイル時に決まり、入力は `const` 参照で受け取ります。

| 得たい結果 | AND（共通部分） | OR（和集合） | XOR（奇数個の入力で1の位置） |
|---|---|---|---|
| 所有する `Bitset` | `bit_and(a,b,c,...)` | `bit_or(a,b,c,...)` | `bit_xor(a,b,c,...)` |
| 1の個数 | `count_and(a,b,c,...)` | `count_or(a,b,c,...)` | `count_xor(a,b,c,...)` |
| 1が存在するか | `any_and(a,b,c,...)` | `any_or(a,b,c,...)` | `any_xor(a,b,c,...)` |
| 全ビットが1か | `all_and(a,b,c,...)` | `all_or(a,b,c,...)` | `all_xor(a,b,c,...)` |

すべて `titan23` 名前空間にあります。`bit_and` などは標準ライブラリの関数オブジェクト、`any` は `std::any` と同じ名前なので、呼び出し例では `titan23::` を明記しています。

```cpp
using Bits = titan23::Bitset<4097>;
Bits a, b, c, d;
a.set(0).set(64);
b.set(0).set(100);
c.set(0).set(200);
d.set(0);

auto intersection = titan23::bit_and(a, b, c); // 独立した所有値
assert(intersection.count() == 1);
assert(titan23::count_or(a, b, c, d) == 4);
assert(titan23::count_xor(a, b, c, d) == 3);
assert(titan23::any_and(a, b, c));
assert(titan23::all_or(a, ~a, d));
assert(!titan23::any_xor(a, a));
assert(a.count_and(b, c) == titan23::count_and(a, b, c));
```

`bit_*` は結果を1つだけ構築して返します。入力を後から変更しても結果は変わりません。`count_*`・`any_*`・`all_*` は各ワードで全入力を合成して直接集計するため、一時bitsetを作りません。`any_*` は1を見つけた時点、`all_*` は0を見つけた時点で走査を終えます。`N == 0` のときは個数0、`any_* == false`、`all_* == true` です。

引数が `K` 個、ワード数が `W` のとき、計算量は最悪 `O(KW)` です。AND・OR・XORの選択と入力個数はコンパイル時に固定し、内側ループで演算の種類を判定しません。`count_*` は単体の個数計算と同じ選択を使い、SIMD版でも全入力を合成してから数えます。`bit_*`・`any_*`・`all_*` も、Backendに応じてSIMDで複数ワードをまとめて処理します。

単体の集計にも `titan23::count(a)`、`titan23::any(a)`、`titan23::all(a)`、`titan23::none(a)` を用意しています。これらは `a.count()`、`a.any()`、`a.all()`、`a.none()` と同じ処理です。

`count_or(a,b,c)` と `count(bit_or(a,b,c))` は値が同じですが、後者は所有する結果のbitsetを生成します。一時bitsetを避けたい集計には `count_*`・`any_*`・`all_*` を使います。混在する式を自動的に遅延評価するAPIではありません。

## BackendとSIMD

```cpp
using AutoBits = titan23::Bitset<4096>;
using WordBits = titan23::Bitset<4096, titan23::BitsetBackend::Scalar>;
// AVX2を有効にしたコンパイルターゲットで使う。
using AvxBits = titan23::Bitset<4096, titan23::BitsetBackend::Avx2>;
```

| Backend | 処理の選択 |
|---|---|
| `Auto` | コンパイルターゲットで有効な命令と処理サイズに応じて選ぶ。既定値 |
| `Scalar` | 通常の64bitワードのループを使う。コンパイラによる自動ベクトル化は許す |
| `Avx2` | 下記のSIMD処理を常にAVX2で使い、端数はワード単位で処理する。`__AVX2__` が定義されないターゲットではコンパイルエラー |

SIMD処理には `titan_cpplib/others/simd_int.cpp` の `SimdBits` を使います。`SimdBits` はコンパイル時にAVX2 (4ワード)、SSE2 (2ワード)、逐次の順で命令を選びます。AVX-512のターゲットでもAVX2の256bit命令を使います。

`Auto` と `Avx2` では、次の処理をSIMDで複数ワードずつ行います。

- 集合演算：`&=`・`|=`・`^=`・`&`・`|`・`^`・`bit_*`・`flip()`
- 判定：`any()`・`none()`・`all()`・`intersects()`・`any_*`・`all_*`
- 個数：`count()`・`count_*`
- シフト：`<<=`・`>>=`・`or_shift_left`・`or_shift_right`

`Auto` の個数とシフトには、次の下限があります。集合演算と判定には下限はありません。

- `count`・`count_*`：AVX2では `word_count >= 16` で使います。下記のローカル計測で、小さいサイズのAVX2が遅くなるケースがあったためです。SSE2だけのターゲットでは、POPCNT命令がなければ2ワード以上で使い、POPCNT命令があれば使いません。POPCNT命令がないと `std::popcount` は関数呼び出しになり、POPCNT命令があるとSSE2のビット演算より1ワードずつ数えるほうが速いためです。`__AVX512VPOPCNTDQ__` が定義されている場合は、通常ループからネイティブのベクトルpopcountへの最適化をコンパイラに任せます。
- 通常のシフトとシフトOR：シフト対象のワード数 `word_count - k / 64` が16以上ならSIMDを使います。`k == 0` と `k >= N` は先に処理します。

AVX2の個数計算には、4bitごとのテーブル参照と加算を使います。SSE2では、2bit・4bit・8bitごとの和を順に作るビット演算で数えます。どちらも8本分をバイト単位で足してから64bitにまとめます。シフトは64bit要素ごとのシフトと隣ワードからの繰り上がりを組み合わせます。シフトORでは入力を読み込んでから書き戻し、左は上位側から、右は下位側から処理します。

値を返す `&`・`|`・`^` と `bit_*` は、結果の内部配列を0で初期化せずに構築し、合成結果を直接書き込みます。

`Auto` は実行時のCPUID判定をしません。`-march=x86-64-v3` や `-mavx2` で作ったバイナリは、対応するCPUで実行します。`-march=native` はコンパイルしたマシンのCPUに依存します。

## テスト

リポジトリのルートから実行します。`std::bitset` との比較で、境界サイズ、余剰ビット、左右シフト、融合演算、参照プロキシ、ランダムな更新列を検証します。複数入力では2・3・4・8入力、引数の並べ替え、同じ入力の重複、XORの相殺、入力が変更されないことも検証します。

```sh
g++ -std=c++20 -O2 -I. test/ds/bitset_random.cpp -o /tmp/bitset_test
/tmp/bitset_test

g++ -std=c++20 -O2 -march=x86-64-v3 -I. test/ds/bitset_random.cpp -o /tmp/bitset_test_avx2
/tmp/bitset_test_avx2
```

AVX2を有効にしたビルドでは、`Auto`・`Scalar`・`Avx2` のすべてを検証します。有効にしないビルドでは `Auto` と `Scalar` を検証します。

## ベンチマーク

同じバイナリ内で `std::bitset`、`Scalar`、`Avx2`、`Auto` を比較します。`Avx2` の行はAVX2が有効なビルドだけに出力します。各バイナリは、ほかの重い処理を止めて個別に実行します。

```sh
g++ -std=c++20 -O2 -I. test/ds/benchmark/bitset_benchmark.cpp -o /tmp/bitset_bench
/tmp/bitset_bench --ms 10 --reps 5 > /tmp/bitset_bench.csv

g++ -std=c++20 -O2 -march=x86-64-v3 -I. test/ds/benchmark/bitset_benchmark.cpp -o /tmp/bitset_bench_avx2
/tmp/bitset_bench_avx2 --ms 10 --reps 5 > /tmp/bitset_bench_avx2.csv

g++ -std=c++20 -O2 -march=native -I. test/ds/benchmark/bitset_benchmark.cpp -o /tmp/bitset_bench_native
/tmp/bitset_bench_native --ms 10 --reps 5 > /tmp/bitset_bench_native.csv
```

測定対象は次のとおりです。`--suite basic` を指定すると、単体の `count`、`and_value`、`or_value`、`and_assign`、`or_assign` の5種類に絞れます。

| CSVの演算名 | 計測内容 |
|---|---|
| `count` / `count_and` | 個数の計算。戻り値のchecksumへの加算も計測に含む |
| `and_value` / `or_value` | `out = a & b` / `out = a \| b`。結果の生成・出力への書き込みを含む |
| `and_copy` / `or_copy` | `out = a` に続く `out &= b` / `out \|= b`。入力コピーを含む |
| `and_assign` / `or_assign` | `out &= b` / `out \|= b`。`out = a` の初期化は各サンプルの計測前に行う |
| `or_shift_left_copy` | 入力をコピーしてからシフトOR。シフトORだけの単独計測ではない |

シフト量は0、1、17、63、64、65、`N / 2 + 7`、`N` を比較します。

入力は固定seedから複数用意し、約1/2、約1/8、1ワードに1bitという密度を混ぜています。`*_value` と `*_copy` は毎回元の入力から計算します。`*_assign` は固定した右辺で同じ出力を反復更新するため、初回以降は `a & b` または `a | b` の値を保ちます。これは主にキャッシュ内の更新速度を測るもので、毎回異なる右辺で更新する測定ではありません。乱数生成、構築、結果検証は計測の外で行い、測定結果は `std::bitset` の結果と照合します。

| オプション | 内容 |
|---|---|
| `--bits N` | 128、256、512、1024、2048、4096、4097、65536から指定する。省略時はすべて |
| `--suite basic\|all` | 基本5演算だけ、または全演算を測る。既定値 `all` |
| `--backends std-auto\|all` | 標準とAutoだけ、または全Backendを測る。既定値 `all` |
| `--inputs K` | 入力の組数。既定値64 |
| `--ms T` | 1サンプルの目標測定時間をミリ秒で指定する。既定値10 |
| `--reps R` | サンプル数。既定値5 |
| `--iterations ROUNDS` | 全入力を走査する回数を固定する。指定時は `--ms` による調整を行わない |

CSVの `median_ns` は、1入力に対する1操作あたりの時間の中央値です。既定の入力数では主にキャッシュ内の処理を測ります。`--inputs` を増やすと、より大きい作業領域を使った測定ができます。

結果はCPU、コンパイラとバージョン、最適化・ターゲット指定、入力サイズ、シフト量、キャッシュの状態に依存します。AtCoder実機では未計測です。

## SimdBitsへの置き換えの計測（2026-11-13）

AMD Ryzen Threadripper 3990X (Zen 2)、GCC 12.3.0で、CPU 4に固定して計測しました。フラグは `-std=c++20 -O2` と、既定ビルドでは `-march` なし、nativeでは `-march=native` を加えたものです。入力64組、`--ms 10 --reps 3` を3回ずつ実行した中央値（ns）です。変更前はHEAD (`9634bd5`) の `Auto`、変更後は `Auto` です。チェックサムは全行で一致しました。

変更点は次のとおりです。

- 直接書いていたAVX2の命令を `SimdBits` に置き換え、AVX2がない既定ビルドでもSSE2で処理するようにしました。
- 集合演算と判定もSIMDで処理するようにしました。GCC 12では `a &= b` のループが自動ベクトル化されず、1ワードずつ処理されていました。
- 値を返す演算は、左辺を値渡しでコピーして更新してから返す形で、結果を返すときにもう一度コピーしていました。結果を初期化せずに構築して直接書き込む形にしました。前節までの記録で原因を特定していなかった、値を返すAND/ORが標準より遅い件の原因です。

**既定ビルド (`-O2`)**

| bit数 | 演算 | std | 変更前 | 変更後 | 短縮 |
|---:|---|---:|---:|---:|---:|
| 512 | `a &= b` | 2.5 | 4.5 | 2.4 | 46.0% |
| 512 | `a & b` | 3.9 | 4.1 | 3.0 | 25.0% |
| 512 | `c = a; c &= b` | 3.6 | 5.8 | 3.8 | 35.4% |
| 512 | `a.count()` | 18.9 | 18.9 | 4.9 | 73.9% |
| 512 | `count_and(a, b)` | 21.1 | 17.0 | 5.5 | 67.5% |
| 512 | `c = a; c.or_shift_left(k)` | 12.7 | 5.6 | 5.6 | 0.3% |
| 4097 | `a &= b` | 22.0 | 22.0 | 14.7 | 33.2% |
| 4097 | `a & b` | 67.7 | 99.6 | 35.6 | 64.3% |
| 4097 | `c = a; c &= b` | 42.2 | 41.9 | 37.7 | 10.0% |
| 4097 | `a.count()` | 132.2 | 131.3 | 35.5 | 73.0% |
| 4097 | `count_and(a, b)` | 171.8 | 149.0 | 39.8 | 73.3% |
| 4097 | `c = a; c.or_shift_left(k)` | 121.2 | 57.3 | 40.8 | 28.8% |
| 65536 | `a &= b` | 268.1 | 399.1 | 271.4 | 32.0% |
| 65536 | `a & b` | 697.7 | 767.4 | 444.7 | 42.0% |
| 65536 | `c = a; c &= b` | 1086.4 | 1083.4 | 1069.2 | 1.3% |
| 65536 | `a.count()` | 2231.1 | 2244.5 | 532.3 | 76.3% |
| 65536 | `count_and(a, b)` | 2640.7 | 2508.9 | 600.4 | 76.1% |
| 65536 | `c = a; c.or_shift_left(k)` | 1386.3 | 1237.4 | 899.2 | 27.3% |

**`-O2 -march=native` (AVX2)**

| bit数 | 演算 | std | 変更前 | 変更後 | 短縮 |
|---:|---|---:|---:|---:|---:|
| 512 | `a &= b` | 1.4 | 4.5 | 1.3 | 70.5% |
| 512 | `a & b` | 14.7 | 8.1 | 1.4 | 82.6% |
| 512 | `c = a; c &= b` | 11.1 | 4.7 | 11.1 | -136.0% |
| 4097 | `a &= b` | 22.1 | 32.8 | 11.4 | 65.2% |
| 4097 | `a & b` | 44.3 | 50.7 | 21.0 | 58.5% |
| 4097 | `c = a; c &= b` | 26.3 | 30.3 | 17.5 | 42.2% |
| 65536 | `a &= b` | 198.4 | 512.9 | 212.3 | 58.6% |
| 65536 | `a & b` | 327.7 | 424.8 | 285.6 | 32.8% |
| 65536 | `c = a; c &= b` | 364.7 | 683.2 | 359.9 | 47.3% |

- 既定ビルドでは、POPCNT命令がなく `std::popcount` が関数呼び出しになるため、`count` が約4倍速くなりました。
- nativeの `count`・`count_*`・シフトは、AVX2の実装も下限も変更前と同じです。このベンチマークでは±20%ほどの差が出ましたが、`count` と `count_and` だけを測る小さいプログラムでは、`count` は変更前と同じ時間、`count_and` は4097bitで12%遅く65536bitで10%速い結果でした。同じバイナリ内の `std::bitset` の時間も同程度動くことがあり、コード配置の影響と見ています。`-march=x86-64-v2` (SSE4.2とPOPCNT) の `count` も同様で、単独で測ると変更前と同じ時間でした。
- nativeの512bitの `c = a; c &= b` は遅くなりました。GCC 12は汎用やZen向けの調整で `c = a` のコピーを16byte単位の書き込みにし、直後のAVX2の32byte単位の読み込みでストアからロードへの転送が失敗するためです。標準も同じ11.1nsです。Ice Lake向けの調整では32byte単位でコピーし、この現象は起きません。コピーを挟まない `a &= b` と `a & b` は同じサイズで3〜6倍速くなるので、下限は設けていません。

## ローカル計測（2026-09-13）

以下の速度表は、多入力の自由関数を追加する前の実装の測定記録です。追加後は差分テストと生成コードを検証しており、複数入力の速度はまだ測定していません。

AMD Ryzen AI 9 HX 370、GCC 15.2.0で、CPU 0に固定して計測しました。共通フラグは `-std=gnu++23 -O2 -ftrivial-auto-var-init=zero -Wall -Wextra -Wpedantic -I.`、入力64組、`--ms 10 --reps 5` です。各結果は1操作の中央値（ns）で、倍率は `std / Auto`、大きいほど `Bitset` が高速です。

`-march=x86-64-v3` はAVX2までの命令を対象にしたビルド、`-march=native` はこのCPUのAVX-512 VPOPCNTDQも有効なビルドです。両方とも同じCPU上で動かしており、異なるジャッジCPUの速度を再現したものではありません。

| ターゲット | bit数 | 演算 | std | Auto | 倍率 |
|---|---:|---|---:|---:|---:|
| x86-64-v3 | 4096 | count | 35.55 | 17.81 | 2.00 |
| x86-64-v3 | 4096 | count_and | 48.85 | 21.30 | 2.29 |
| x86-64-v3 | 4096 | or_shift_left_copy(65) | 87.57 | 34.99 | 2.50 |
| x86-64-v3 | 65536 | count_and | 981.68 | 329.79 | 2.98 |
| x86-64-v3 | 65536 | or_shift_left_copy(65) | 1455.55 | 634.81 | 2.29 |
| native | 4096 | count | 8.08 | 9.71 | 0.83 |
| native | 4096 | count_and | 28.88 | 16.23 | 1.78 |
| native | 4096 | or_shift_left_copy(65) | 100.41 | 40.02 | 2.51 |
| native | 65536 | count_and | 450.06 | 334.44 | 1.35 |
| native | 65536 | or_shift_left_copy(65) | 1507.18 | 644.65 | 2.34 |

融合演算では改善が見られますが、単体の `count()` やAND、非常に小さいbitsetでは標準実装が速いケースもあります。特にネイティブのベクトルpopcountが使える場合は標準の `count()` も高速です。

AVX2ターゲットの512bitでは、強制AVX2の `count()` が7.76ns、通常ループが5.21nsでした。1024bitではそれぞれ8.49nsと9.41ns、2048bitでは10.23nsと18.71nsで、16ワードの下限を維持しました。左シフトORでも512bitの強制AVX2は通常ループより遅いケースがあり、同じ下限を使っています。

測定コマンド例：

```sh
g++ -std=gnu++23 -O2 -ftrivial-auto-var-init=zero -Wall -Wextra -Wpedantic -march=x86-64-v3 -I. test/ds/benchmark/bitset_benchmark.cpp -o /tmp/bitset_benchmark_avx2
taskset -c 0 /tmp/bitset_benchmark_avx2 --ms 10 --reps 5 > /tmp/bitset_avx2.csv

g++ -std=gnu++23 -O2 -ftrivial-auto-var-init=zero -Wall -Wextra -Wpedantic -march=native -I. test/ds/benchmark/bitset_benchmark.cpp -o /tmp/bitset_benchmark_native
taskset -c 0 /tmp/bitset_benchmark_native --ms 10 --reps 5 > /tmp/bitset_native.csv
```

## 基本演算の追加計測（2026-09-13）

同じCPU・コンパイラ・共通フラグで、`--suite basic --backends std-auto --ms 20 --reps 7` を指定し、CPU 0に固定して計測しました。入力は64組です。時間は1操作の中央値（ns）で、小さいほど高速です。最後の列は標準の処理時間を100%とした自作の処理時間です。100%未満なら自作が速く、100%を超えると自作が遅い結果です。

`a & b` / `a | b` は値の生成と出力への代入を含みます。`a &= b` / `a |= b` は更新のみで、初期化を含みません。実装は既定の `BitsetBackend::Auto` です。

**`-march=native`**

| bit数 | 演算 | std (ns) | Auto (ns) | 自作の時間（標準100%） |
|---:|---|---:|---:|---:|
| 512 | `a & b` | 3.97 | 3.75 | 94.5% |
| 512 | `a \| b` | 3.47 | 3.58 | 103.1% |
| 512 | `a &= b` | 3.26 | 3.11 | 95.4% |
| 512 | `a \|= b` | 2.96 | 3.12 | 105.4% |
| 512 | `a.count()` | 5.93 | 5.92 | 99.9% |
| 4096 | `a & b` | 31.54 | 34.96 | 110.9% |
| 4096 | `a \| b` | 32.00 | 34.50 | 107.8% |
| 4096 | `a &= b` | 18.13 | 18.40 | 101.5% |
| 4096 | `a \|= b` | 17.41 | 17.61 | 101.1% |
| 4096 | `a.count()` | 8.20 | 8.36 | 101.9% |
| 4097 | `a & b` | 49.88 | 70.91 | 142.1% |
| 4097 | `a \| b` | 51.01 | 70.88 | 138.9% |
| 4097 | `a &= b` | 22.36 | 20.21 | 90.4% |
| 4097 | `a \|= b` | 19.45 | 19.29 | 99.1% |
| 4097 | `a.count()` | 8.14 | 8.50 | 104.4% |
| 65536 | `a & b` | 599.36 | 670.04 | 111.8% |
| 65536 | `a \| b` | 555.79 | 696.21 | 125.3% |
| 65536 | `a &= b` | 337.93 | 355.81 | 105.3% |
| 65536 | `a \|= b` | 388.29 | 337.66 | 87.0% |
| 65536 | `a.count()` | 121.38 | 134.44 | 110.8% |

**`-march=x86-64-v3`**

| bit数 | 演算 | std (ns) | Auto (ns) | 自作の時間（標準100%） |
|---:|---|---:|---:|---:|
| 512 | `a & b` | 2.72 | 2.57 | 94.4% |
| 512 | `a \| b` | 2.98 | 2.55 | 85.6% |
| 512 | `a &= b` | 2.33 | 2.28 | 97.7% |
| 512 | `a \|= b` | 2.28 | 2.23 | 98.1% |
| 512 | `a.count()` | 5.32 | 5.43 | 102.1% |
| 4096 | `a & b` | 32.12 | 32.47 | 101.1% |
| 4096 | `a \| b` | 29.36 | 30.45 | 103.7% |
| 4096 | `a &= b` | 17.27 | 17.33 | 100.3% |
| 4096 | `a \|= b` | 18.48 | 19.31 | 104.5% |
| 4096 | `a.count()` | 36.77 | 20.55 | 55.9% |
| 4097 | `a & b` | 68.41 | 98.14 | 143.4% |
| 4097 | `a \| b` | 69.49 | 117.05 | 168.5% |
| 4097 | `a &= b` | 22.29 | 17.34 | 77.8% |
| 4097 | `a \|= b` | 17.05 | 16.81 | 98.6% |
| 4097 | `a.count()` | 42.02 | 19.71 | 46.9% |
| 65536 | `a & b` | 582.71 | 782.49 | 134.3% |
| 65536 | `a \| b` | 587.42 | 798.62 | 136.0% |
| 65536 | `a &= b` | 299.54 | 306.39 | 102.3% |
| 65536 | `a \|= b` | 292.76 | 301.48 | 103.0% |
| 65536 | `a.count()` | 606.00 | 292.70 | 48.3% |

4096bitの更新AND/ORは両ターゲットとも概ね同速でした。単体countはAVX2ターゲットで自作が速い一方、nativeでは標準もベクトルpopcountを使うため近い速度でした。値を返すAND/ORは自作が遅くなるケースがあり、特に4097bitと65536bitで差が見られます。値返し部分のコピーや一時領域の最適化差は原因の候補ですが、今回の計測では原因の特定までは行っていません。2026-11-13の変更で原因を特定して直しました。「SimdBitsへの置き換えの計測（2026-11-13）」を参照してください。

別途、4096bitの独立した関数をコンパイルして確認すると、nativeの `&=` と `|=` は両実装とも512bitのSIMDループ、`count()` も両実装とも同じ形のベクトルpopcountループになりました。ナノ秒単位の小さい差は、呼び出し側の生成コード・メモリ配置・計測時のCPUの状態にも影響されます。前節の旧ベンチの時間と混ぜず、各行の標準と自作を比較してください。

基本演算だけを測るコマンド（バイナリは上記のコンパイルコマンドで最新版から作成）：

```sh
taskset -c 0 /tmp/bitset_benchmark_native --suite basic --backends std-auto --ms 20 --reps 7 > /tmp/bitset_basic_native.csv
taskset -c 0 /tmp/bitset_benchmark_avx2 --suite basic --backends std-auto --ms 20 --reps 7 > /tmp/bitset_basic_avx2.csv
```
