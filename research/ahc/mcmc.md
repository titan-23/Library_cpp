# mcmc.cpp

[mcmc.cpp](../../titan_cpplib/ahc/mcmc.cpp) の BayesianLinearGibbsSampler のコメントから移した説明。

## モデル

- 観測モデルは y_i ~ N(sum_k(A_{i,k} * X_k), obs_sigma_i^2) である。
- 事前分布は X_k ~ N(prior_mu_k, prior_sigma_k^2) である。
- report() を 1 回呼ぶと、「線形式 coeffs・X が y にほぼ等しい」という一次方程式が 1 本加わる。
- sigma が小さい観測ほど強く信頼する。
- 観測は疎形式で持つ。計算量に出てくる nnz は全観測の非零係数の総数である。

## report の引数

- y は観測された値である。
- 疎形式の coeffs は、非零の (変数index, 係数) の列である。
- 密形式の coeffs は各変数に掛ける係数で、長さは K にする。
- sigma は観測ノイズの標準偏差である。

## estimate

- バッチ用で、事前分布から連鎖を始めて推定する。
- iterations はサンプリングの総反復回数で、目安は 5000〜20000 である。
- burn_in は捨てる初期サンプルの数で、目安は iterations の 1〜2 割である。iterations より小さくする。
- seed は乱数の種である。

## estimate_step

- ターンごとの再推定用で、直前の連鎖状態から続けて推定する。
- 観測を追加してから呼ぶ。
- 連鎖が前回の続きから始まるので、burn_in を小さくでき、反復も少なくて済む。
- iterations は、前回の続きから始まるため数百〜数千で足りることが多い。
- burn_in は捨てる初期サンプルの数で、iterations より小さくする。
- 初回は事前分布から始める。seed は初回だけ使う。
- 連鎖を初期化するには reset_chain() を呼ぶ。
