/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/beam_search/beam_param.cpp
#pragma once

#include "titan_cpplib/others/print.cpp"

namespace flying_squirrel {

/// @brief ビームサーチの設定と動的ビーム幅の計測値を管理する
struct BeamParam {
    int max_turn, beam_width;
    /// @brief 制限時間 [ms]
    double time_limit;
    bool is_adjusting;

    /// @brief ターンごとに重複判定用ハッシュを破棄するか
    /// false にする場合、State のハッシュにターン情報が含まれないと、別ターンの候補も重複とみなされる
    bool clear_hash_every_turn;

    /// @brief 固定深さ版と naive 版で、古いハッシュを破棄する間隔 0 なら破棄しない
    /// 保持範囲は最後の破棄から 1〜K ターン分で、厳密な直近 K ターンではない
    /// @note 可変ターン版では使用しない
    int hash_window_turns;

    // 通常ターンとメタターンで共用する累積値
    int pool_size_sum, beam_width_sum, turn_sum;
    double time_sum;
    int prev_beam_width;

    // 各ターンの実効幅 ログ出力にのみ使用する
    vector<int> width_hist;

    // beam_search_turn.cpp で使う target_turn 進行量の累積値
    long long target_step_sum;
    long long target_step_count;

    // beam_search_turn.cpp の global seen_hash に与える初期容量のヒント 0 なら自動
    int seen_hash_capacity_hint;

    // 展開のあるメタターンの累積時間と回数 時間は ms 単位
    double time_active_sum;
    long long count_active;

    // 展開の有無と target_turn 進行量の EMA
    double ema_active_rate;
    double ema_step;
    int meta_sample_count;

    // EMA の平滑化係数 大きいほど直近の値を重視する
    double ema_alpha_rate;
    double ema_alpha_step;

    // 推奨幅に掛ける安全率
    double width_safety_factor;

    // 動的調整を始めるまでのメタターン数
    int calibration_meta_count;

    BeamParam() { init(); }

    BeamParam(int max_turn, int beam_width, double time_limit, bool is_adjusting=false,
              bool clear_hash_every_turn=true, int hash_window_turns=0) {
        init();
        this->max_turn = max_turn;
        this->beam_width = beam_width;
        this->time_limit = time_limit;
        this->is_adjusting = is_adjusting;
        this->clear_hash_every_turn = clear_hash_every_turn;
        this->hash_window_turns = hash_window_turns;
    }

    void init() {
        max_turn = 0;
        beam_width = 0;
        time_limit = 0;
        is_adjusting = false;
        clear_hash_every_turn = true;
        hash_window_turns = 0;
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

    /// @brief 1 メタターンの計測値を記録し、進行量の EMA を更新する
    /// @param dt_expand_ms 候補展開の所要時間 [ms]
    /// @param dt_update_ms ソートとツリー更新の所要時間 [ms]
    /// @param tree_size 更新後の探索木のノード数
    /// @param exp_count 一時的に候補へ登録された件数 現在は未使用
    /// @param applied_w 展開した葉の数
    /// @param delta_target target_turn の進行量
    void timestamp_meta(double dt_expand_ms, double dt_update_ms, int tree_size, int exp_count, int applied_w,
                        int delta_target) {
        double dt_ms = dt_expand_ms + dt_update_ms;

        // 展開のないターンはビーム幅に依存しないため、分けて計測する
        if (applied_w > 0) {
            time_active_sum += dt_ms;
            count_active++;
        }
        ema_active_rate = ema_update(ema_active_rate, (applied_w > 0 ? 1.0 : 0.0), ema_alpha_rate);
        // 進行しなかったターンも残りターン数の推定に含める
        ema_step = ema_update(ema_step, (double)max(0, delta_target), ema_alpha_step);
        meta_sample_count++;

        (void)exp_count;
        pool_size_sum  += tree_size;
        beam_width_sum += applied_w;
        time_sum       += dt_ms;
        turn_sum++;
        width_hist.push_back(applied_w);
    }

    /// @brief target_turn の進行量を累積する
    void note_target_step(int step) {
        target_step_sum += step;
        target_step_count++;
    }

    /// @brief 展開コストを予測できるかを返す
    bool cost_model_ready() const {
        return count_active > 0 && turn_sum > 0;
    }

    /// @brief 残り時間から推奨ビーム幅を求める
    /// @details 展開のあるターンの時間だけをビーム幅に比例させ、展開のないターンは固定費として扱う
    /// @return 計測不足の場合は -1
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
            dt_empty = (time_sum - time_active_sum) / (double)((long long)turn_sum - count_active);
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

    /// @brief 通常のビームサーチで次ターンの幅を求める
    int get_beam_width(int remain_turn, int now_pool_size, double remain_time) {
        if (!is_adjusting || turn_sum <= 10) {
            return beam_width;
        }
        // 幅の更新は 10 ターンごとに行う
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

    /// @brief 累積した平均ビーム幅を標準エラーに出力する
    /// @deprecated beam_log::end_banner で同等の情報を出力できる
    void report() const {
        cerr << to_bold("BeamParam-report----------------") << endl;
        if (turn_sum == 0) {
            cerr << "turn_sum = 0" << endl;
        } else {
            cerr << "ave_beam_width=" << (double)beam_width_sum / turn_sum << endl;
        }
        cerr << "--------------------------------" << endl;
    }

    /// @brief 累積値から平均ビーム幅を求める
    double ave_width() const {
        return turn_sum > 0 ? (double)beam_width_sum / turn_sum : 0.0;
    }
};

BeamParam gen_param(int max_turn, int beam_width) {
    return {max_turn, beam_width, -1};
}

BeamParam gen_param(int max_turn, int beam_width, double time_limit, bool is_adjusting,
                    bool clear_hash_every_turn=true) {
    return {max_turn, beam_width, time_limit, is_adjusting, clear_hash_every_turn};
}
} // namespace flying_squirrel
