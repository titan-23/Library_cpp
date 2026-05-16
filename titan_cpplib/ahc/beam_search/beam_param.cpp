#pragma once

#include "titan_cpplib/others/print.cpp"

namespace flying_squirrel { // flying squirrel

struct BeamParam {
    int max_turn, beam_width;
    // time_limit は ms 単位 (Timer::elapsed() と単位を揃える)。1 秒なら 1000。
    double time_limit;
    bool is_adjusting;
    // 毎ターン Candidates 内の hash dict を clear するか。
    // true : 各ターンの beam 内重複排除のみ行う安全な既定動作。
    // false: clear のオーバーヘッドを省くが、State 側の hash がターン情報を含まないと
    //        ターン跨ぎの stale entry により候補が黙って drop される。設計を理解した上で使うこと。
    bool clear_hash_every_turn;

    // 内部で使用する変数
    int pool_size_sum, beam_width_sum, turn_sum;
    double time_sum;
    int prev_beam_width;

    // 動的ビーム幅の推移 report 用。timestamp / timestamp_meta が
    // 毎ターン (メタターン) 末尾に実効幅を push する。探索挙動には不使用。
    vector<int> width_hist;

    // target_turn 進行速度の実測 (beam_search_turn.cpp のマルチターン用)
    // 1 メタターンで進む target_turn の期待値 = target_step_sum / target_step_count
    long long target_step_sum;
    long long target_step_count;

    // beam_search_turn.cpp の global seen_hash の初期キャパシティのヒント (0 で自動)
    int seen_hash_capacity_hint;

    // ---- 累積コスト計測 (beam_search_turn.cpp 用) ----
    // 全時間は ms 単位 (param.time_limit と一致)。
    //
    // dt は active メタターン (applied_w > 0) と empty メタターン
    // (applied_w = 0) で構造的に違う:
    //   - empty : dt = tree 走査のみ。W に依存しない
    //   - active: dt = W に比例する成分が支配
    // 単一の EMA / 平均で混ぜると bimodal な分布を平均してしまい、
    // empty が多いほど平均が下振れ、W を絞るタイミングが遅れる。
    // したがって active / empty を別々に累積で記録し、線形スケーリングは
    // active 成分にだけ適用する。詳細は recommend_width 参照。
    //
    //   time_active_sum / count_active : active メタターン累積
    //   time_empty (= time_sum - time_active_sum) は副次的に求まる
    //   count_empty (= turn_sum - count_active) も同様
    //   beam_width_sum は applied_w の累積 (= active 時のみ加算される)
    //
    // 補助 EMA (現状 recommend_width 内では未使用、観測用):
    //   ema_active_rate : EMA( applied_w > 0 ? 1 : 0 )
    //   ema_step        : EMA( delta_target )    残メタターン推定用
    double time_active_sum;
    long long count_active;
    double ema_active_rate;
    double ema_step;
    int    meta_sample_count;

    // EMA 平滑化係数 (0 < alpha <= 1, 大きいほど直近重視)
    double ema_alpha_rate;
    double ema_alpha_step;

    // 動的調整の安全率。予測 1 メタターン時間が予算を超過しないように、
    // 推奨幅をこの倍率だけ下振れさせる。
    double width_safety_factor;

    // 計測安定化のため、最初の数メタターンは固定幅で動かす。
    // EMA に十分なサンプルが入ってから動的調整を始める。
    int calibration_meta_count;

    BeamParam() { init(); }

    BeamParam(
        int max_turn,
        int beam_width,
        double time_limit,
        bool is_adjusting=false,
        bool clear_hash_every_turn=true
    ) {
        init();
        this->max_turn = max_turn;
        this->beam_width = beam_width;
        this->time_limit = time_limit;
        this->is_adjusting = is_adjusting;
        this->clear_hash_every_turn = clear_hash_every_turn;
    }

    void init() {
        max_turn = 0;
        beam_width = 0;
        time_limit = 0;
        is_adjusting = false;
        clear_hash_every_turn = true;
        pool_size_sum = 0;
        beam_width_sum = 0;
        turn_sum = 0;
        time_sum = 0;
        prev_beam_width = -1;
        width_hist.clear();
        target_step_sum = 0;
        target_step_count = 0;
        seen_hash_capacity_hint = 0;

        time_active_sum = 0.0;
        count_active    = 0;
        ema_active_rate = -1.0;
        ema_step        = -1.0;
        meta_sample_count = 0;
        ema_alpha_rate = 0.20;
        ema_alpha_step = 0.30;
        width_safety_factor    = 0.90;
        calibration_meta_count = 3;
    }

    static double ema_update(double cur, double x, double alpha) {
        return (cur < 0.0) ? x : (alpha * x + (1.0 - alpha) * cur);
    }

    void timestamp(int pool_size, int beam_width, double time) {
        pool_size_sum += pool_size;
        beam_width_sum += beam_width;
        time_sum += time;
        turn_sum++;
        width_hist.push_back(beam_width);
    }

    // beam_search_turn.cpp 用: 1 メタターン分の計測サンプルを EMA に流し、
    // 互換のため累積指標 (turn_sum / time_sum 等) も同時に更新する。
    //   dt_expand_ms : get_next_beam の所要時間 [ms]
    //   dt_update_ms : sort + update_tree の所要時間 [ms]
    //   tree_size    : update_tree 後の tree.size()
    //   exp_count    : current_new_candidates.size() (採用された展開数)
    //   applied_w    : このメタターン中に展開した leaf 数
    //                  (= target_turn == 現メタターン だった tree 内 leaf 数)
    //   delta_target : 1 メタターンで進んだ target_turn の量 (>=0)
    //
    // dt_expand_ms / dt_update_ms は分けて取っているが、現状の累積モデルでは
    // 合算 dt のみを使う。分割は将来の精度向上のため signature だけ残す。
    void timestamp_meta(double dt_expand_ms, double dt_update_ms,
                        int tree_size, int exp_count,
                        int applied_w, int delta_target) {
        double dt_ms = dt_expand_ms + dt_update_ms;

        // active / empty を分けて累積する。
        // empty は dt が W に依存しないので別計上したい。
        if (applied_w > 0) {
            time_active_sum += dt_ms;
            count_active++;
        }
        // 補助 EMA (観測用)
        ema_active_rate = ema_update(ema_active_rate,
                                     (applied_w > 0 ? 1.0 : 0.0),
                                     ema_alpha_rate);
        // ema_step は zero meta-turn も含めた平均にする
        // (0 を入れずに更新すると進捗ゼロの実態を反映できず remain_meta を過小評価する)
        ema_step = ema_update(ema_step, (double)max(0, delta_target), ema_alpha_step);
        meta_sample_count++;

        // 互換のため既存の累積も更新する。
        (void)exp_count;
        pool_size_sum  += tree_size;
        beam_width_sum += applied_w;  // applied_w==0 のとき加算されないので、結果として active 時の W 累積
        time_sum       += dt_ms;
        turn_sum++;
        width_hist.push_back(applied_w);
    }

    // beam_search_turn.cpp 用: target_turn の進行量を記録する (累積のみ)。
    // EMA 更新は timestamp_meta 側で行う。
    void note_target_step(int step) {
        target_step_sum += step;
        target_step_count++;
    }

    // モデルが予測に使える状態か。active を 1 回以上観測している必要がある。
    bool cost_model_ready() const {
        return count_active > 0 && turn_sum > 0;
    }

    // 残り remain_meta メタターンを remain_time_ms 以内に収める W を返す。
    // モデル未初期化なら -1。
    //
    // active メタターン (dt_active = a + b*W に近い) と empty メタターン
    // (dt_empty = const) を分けて扱う:
    //   rate            = count_active / turn_sum
    //   dt_empty        = (time_sum - time_active_sum) / (turn_sum - count_active)
    //   dt_active_obs   = time_active_sum / count_active        (W=w_obs での実測)
    //   w_obs           = beam_width_sum / count_active         (active 時の平均 W)
    //
    //   target_dt        = remain_time / remain_meta
    //   target_dt_active = (target_dt - (1-rate) * dt_empty) / rate
    //   W*               = w_obs * target_dt_active / dt_active_obs
    //
    // dt_active が W に厳密に比例する仮定 (overhead 無視) なので多少
    // バイアスはあるが、recommend_width は毎メタターン何度も呼ばれるので
    // 反復で自己補正される (W を下げれば dt_active 累積が下がり、次の
    // 推奨も整合する)。
    int recommend_width(double remain_time_ms, int remain_meta) const {
        if (!cost_model_ready())       return -1;
        if (remain_time_ms  <= 0.0)    return 1;
        if (remain_meta < 1) remain_meta = 1;

        double dt_active_obs = time_active_sum / (double)count_active;
        if (dt_active_obs <= 0.0) return beam_width;
        double w_obs = (double)beam_width_sum / (double)count_active;
        if (w_obs <= 0.0) return beam_width;

        double rate = (double)count_active / (double)turn_sum;
        if (rate <= 0.0) return beam_width;

        double dt_empty = 0.0;
        if ((long long)turn_sum > count_active) {
            dt_empty = (time_sum - time_active_sum)
                     / (double)((long long)turn_sum - count_active);
            if (dt_empty < 0.0) dt_empty = 0.0;
        }

        double target_dt = remain_time_ms / (double)remain_meta;
        double target_dt_active = (target_dt - (1.0 - rate) * dt_empty) / rate;
        if (target_dt_active <= 0.0) return 1;

        double W_d = w_obs * target_dt_active / dt_active_obs * width_safety_factor;
        int W = (int)W_d;
        if (W < 1) W = 1;
        if (W > beam_width) W = beam_width;
        return W;
    }

    int get_beam_width(int remain_turn, int now_pool_size, double remain_time) {
        // return beam_width;
        if (!is_adjusting || turn_sum <= 10) {
            return beam_width;
        }
        // 10回ごとに更新してみる
        if (turn_sum % 10 != 0 && prev_beam_width != -1) {
            return prev_beam_width;
        }
        if (remain_turn <= 0) return beam_width;
        int ave_beam_width = (double)beam_width_sum / turn_sum;
        double can_use_time = (double)remain_time / remain_turn;
        double pred_one_time = (double)time_sum / beam_width_sum;
        int pred_width = max(1.0, can_use_time / pred_one_time);
        int beam_width = (pred_width*2 + ave_beam_width) / 3;
        prev_beam_width = beam_width;
        return beam_width;
    }

    // @deprecated `beam_log::end_banner` で同等の情報が出力される。
    // 個別に呼ぶことも引き続き可能。
    void report() const {
        cerr << to_bold("BeamParam-report----------------") << endl;
        if (turn_sum == 0) {
            cerr << "turn_sum = 0" << endl;
        } else {
            cerr << "ave_beam_width=" << (double)beam_width_sum / turn_sum << endl;
        }
        cerr << "--------------------------------" << endl;
    }

    // 平均ビーム幅 (turn_sum == 0 の時は 0)
    double ave_width() const {
        return turn_sum > 0 ? (double)beam_width_sum / turn_sum : 0.0;
    }
};

BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting, bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}
} // namespace flying_squirrel
