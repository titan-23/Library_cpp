# beam_param.cpp

## timestamp_meta の引数

- `dt_expand_ms` は候補展開の所要時間で、単位は ms である。
- `dt_update_ms` はソートと木の更新の所要時間で、単位は ms である。
- `tree_size` は更新後の探索木のノード数である。
- `exp_count` は一時的に候補へ登録された件数である。現在は使っていない。
- `applied_w` は展開した葉の数である。
- `delta_target` は target_turn の進行量である。

## recommend_width の考え方

- 展開のあるターンの時間だけをビーム幅に比例させる。
- 展開のないターンは、ビーム幅によらない固定費として扱う。
- 計測が足りない間は -1 を返す。計測が足りるとは、展開のあるメタターンが 1 回以上あり、記録したターンが 1 回以上あることを指す。

計算は次の順に行う。

- 展開のあるターン 1 回の平均時間 `dt_active_obs` と、そのときの平均の幅 `w_obs` を求める。
- 展開のあるターンの割合 `rate` と、展開のないターン 1 回の平均時間 `dt_empty` を求める。
- 残りの 1 メタターンに使える時間 `target_dt` から、展開のないターンの分を差し引く。`target_dt_active = (target_dt - (1 - rate) * dt_empty) / rate` とする。
- 推奨幅は `w_obs * target_dt_active / dt_active_obs * width_safety_factor` とし、1 以上 beam_width 以下に収める。
- 残り時間が 0 以下のときと、`target_dt_active` が 0 以下のときは 1 を返す。
- `dt_active_obs`、`w_obs`、`rate` のどれかが 0 以下のときは beam_width を返す。
