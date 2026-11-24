# mcmc.cpp

[mcmc.cpp](../../titan_cpplib/ahc/mcmc.cpp) の BayesianLinearGibbsSampler について説明します。

## モデル

- 観測モデルは y_i ~ N(sum_k(A_{i,k} * X_k), obs_sigma_i^2) です。
- 事前分布は X_k ~ N(prior_mu_k, prior_sigma_k^2) です。
- report() を 1 回呼び出すと、「線形式 coeffs・X が y にほぼ等しい」という一次方程式が 1 本追加されます。
- sigma が小さい観測ほど推定に強く影響します。
- 観測は疎形式で保持します。計算量に現れる nnz は全観測の非零係数の総数です。

## report の引数

- y は観測された値です。
- 疎形式の coeffs は非零の (変数 index, 係数) の列です。
- 密形式の coeffs は各変数に掛ける係数で、長さは K にします。
- sigma は観測ノイズの標準偏差です。

## estimate

- バッチ用で、事前分布から連鎖を開始して推定します。
- iterations はサンプリングの総反復回数で、目安は 5000〜20000 です。
- burn_in は破棄する初期サンプルの数で、目安は iterations の 1〜2 割です。iterations より小さい値にします。
- seed は乱数のシードです。

## estimate_step

- ターンごとの再推定用で、直前の連鎖の状態から続けて推定します。
- 観測を追加してから呼び出します。
- 連鎖が前回の続きから始まるため、burn_in を小さくすることができ、反復回数も少なくて済みます。
- 前回の続きから始まるため、iterations は数百〜数千で足りる場合が多いです。
- burn_in は破棄する初期サンプルの数で、iterations より小さい値にします。
- 初回は事前分布から開始します。seed は初回だけ使用します。
- 連鎖を初期化するには reset_chain() を呼び出します。
