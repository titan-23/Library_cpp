# titan23::Kmeans ライブラリ仕様

## 1. 概要
指定された距離関数と重心計算関数を用いて、K-means法および各クラスタのサイズを指定可能な制約付きK-means法（最小費用流を利用）を実行するテンプレートクラスです。

## 2. テンプレート引数
クラスのインスタンス化には以下の4つのパラメータが必要です。
- `DistType`: 距離を表す数値型（例: `double`, `long long`）
- `ElmType`: 要素（座標やデータ点）の型（例: `pair<double, double>`）
- `dist`: 2つの `ElmType` 間の距離を計算して `DistType` を返す関数ポインタ
- `mean`: `ElmType` の配列から重心を計算して `ElmType` を返す関数ポインタ

## 3. コンストラクタ
- `Kmeans()`
  デフォルトコンストラクタです。
- `Kmeans(const int k, const int max_iter)`
  クラスタ数 `k` と最大反復回数 `max_iter` を指定して初期化します。

## 4. メンバ関数
すべての `fit` 系関数は、`pair<vector<int>, vector<ElmType>>` を返します。
戻り値の `first` は各データ点が属するクラスタのID（ラベル）の配列、`second` は計算された各クラスタの中心座標の配列です。

### 4.1 通常の K-means
- `fit(const vector<ElmType> &X, const vector<ElmType> &init_centers)`
  データ点 `X` と、事前に指定した初期クラスタ中心 `init_centers` を用いてクラスタリングを行います。
- `fit(const vector<ElmType> &X)`
  データ点 `X` のみを受け取ります。内部で距離に応じた確率的選択（K-means++相当の手法）により初期中心を決定し、クラスタリングを行います。

### 4.2 クラスタサイズ制約付き K-means (Min-Cost Flow 適用)
各クラスタに割り当てるデータ点の数を厳密に指定する場合に使用します。内部で `atcoder::mincostflow` を利用します。
- `fit_flow(const vector<ElmType> &X, const vector<ElmType> &init_centers, const vector<int> &target_sizes, double cost_scale = 1.0)`
  データ点 `X`、初期中心 `init_centers` に加え、各クラスタの目標サイズ `target_sizes` を指定します。`target_sizes` の合計はデータ点数と一致する必要があります。`cost_scale` は距離を整数のフローコストに変換する際の倍率です。
- `fit_flow(const vector<ElmType> &X, const vector<int> &target_sizes, double cost_scale = 1.0)`
  初期中心を自動で決定した後、サイズ制約付きクラスタリングを行います。

## 5. 使用例
```cpp
#include <iostream>
#include <vector>
#include <cmath>
#include "kmeans.cpp"

using namespace std;
using ElmType = pair<double, double>;

// 距離関数
double calc_dist(const ElmType& a, const ElmType& b) {
    double dx = a.first - b.first;
    double dy = a.second - b.second;
    return dx * dx + dy * dy;
}

// 重心計算関数
ElmType calc_mean(const vector<ElmType>& pts) {
    if (pts.empty()) return {0.0, 0.0};
    double sx = 0, sy = 0;
    for (const auto& p : pts) {
        sx += p.first;
        sy += p.second;
    }
    double n = pts.size();
    return {sx / n, sy / n};
}

int main() {
    vector<ElmType> data = {
        {0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0},
        {10.0, 10.0}, {11.0, 11.0}, {10.0, 11.0}
    };
    int K = 2;
    int max_iter = 20;
    // インスタンス化
    titan23::Kmeans<double, ElmType, calc_dist, calc_mean> kmeans(K, max_iter);
    // 通常のK-means実行（初期値自動決定）
    auto [labels, centers] = kmeans.fit(data);
    for (int i = 0; i < data.size(); ++i) {
        cout << "Point " << i << " -> Cluster " << labels[i] << "\n";
    }
    return 0;
}
```
