# beam_param.cpp

## timestamp_meta の引数

- `dt_expand_ms` は候補の展開の所要時間で、単位は ms です。
- `dt_update_ms` はソートと木の更新の所要時間で、単位は ms です。
- `tree_size` は更新後の探索木のノード数です。
- `exp_count` は一時的に候補へ登録された件数です。現在は使用していません。
- `applied_w` は展開した葉の数です。
- `delta_target` は target_turn が進んだ量です。

## recommend_width の計算方法

- 展開のあるターンの時間だけがビーム幅に比例するとみなします。
- 展開のないターンは、ビーム幅によらない固定コストとして扱います。
- 計測が足りない間は -1 を返します。計測が足りるとは、展開のあるメタターンが 1 回以上あり、記録したターンが 1 回以上あることを指します。

計算は次の順に行います。

- 展開のあるターン 1 回の平均時間 `dt_active_obs` と、そのときの平均の幅 `w_obs` を求めます。
- 展開のあるターンの割合 `rate` と、展開のないターン 1 回の平均時間 `dt_empty` を求めます。
- 残りの 1 メタターンに使用できる時間 `target_dt` から、展開のないターンの分を差し引きます。`target_dt_active = (target_dt - (1 - rate) * dt_empty) / rate` とします。
- 推奨幅は `w_obs * target_dt_active / dt_active_obs * width_safety_factor` とし、1 以上 beam_width 以下に収めます。
- 残り時間が 0 以下のときと、`target_dt_active` が 0 以下のときは 1 を返します。
- `dt_active_obs`, `w_obs`, `rate` のいずれかが 0 以下のときは beam_width を返します。
