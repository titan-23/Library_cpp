### 2次元総和版

- WaveletMatrix2DSum への整理・改名: 実装済み
- range_sum(x1, x2): 実装済み
- sum_lt: 実装済み
- count_sum_lt: 実装済み
- range_count: 実装済み
- kth_y: 実装済み
- sum_k_smallest_y: 実装済み
- sum_k_largest_y: 実装済み
- 重複点: 1登録を1点として数える。同一座標も別々の点として保持する。

### 2次元Min版

range_min は既にあります。未実装なのは以下です。

- WaveletMatrix2DMin への整理・改名: 実装済み
- range_argmin: 実装済み
- range_max: 実装済み
- range_argmax: 実装済み
- WaveletMatrix2DMonoid: 実装済み

### 動的Wavelet Matrix総和版

実装しない。同等用途には `DynamicWaveletTreeSum<T, W>` を使う。

### 動的Wavelet Tree系の追加API

- topk: 実装済み
- range_select: 実装済み
- 公開された reserve(expected_size): 実装済み
- DynamicWaveletTreeSum::add_weight: 実装済み
- set_key という明示的な別名: 実装済み

### その他

- 全WM実装における sigma とビット数定義の統一
- 座標圧縮ラッパー
- OR、AND、min、max、gcdなど総和以外の値順集約・境界探索
- 全クラス間での共通API完全統一
