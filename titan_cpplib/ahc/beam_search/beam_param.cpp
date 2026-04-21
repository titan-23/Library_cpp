#pragma once

#include "titan_cpplib/others/print.cpp"

namespace flying_squirrel {

struct BeamParam {
    int max_turn, beam_width;
    double time_limit;
    bool is_adjusting;

    // 内部で使用する変数
    int pool_size_sum, beam_width_sum, turn_sum;
    double time_sum;
    int prev_beam_width;
    double ema_turn_time;

    BeamParam() { init(); }

    BeamParam(
        int max_turn,
        int beam_width,
        double time_limit,
        bool is_adjusting=false
    ) {
        init();
        this->max_turn = max_turn;
        this->beam_width = beam_width;
        this->time_limit = time_limit;
        this->is_adjusting = is_adjusting;
        if (is_adjusting) {
            cerr << to_bold("Warning: 動的ビーム幅は試験的です") << endl;
        }
    }

    void init() {
        max_turn = 0;
        beam_width = 0;
        time_limit = 0;
        is_adjusting = false;
        pool_size_sum = 0;
        beam_width_sum = 0;
        turn_sum = 0;
        time_sum = 0;
        prev_beam_width = -1;
        ema_turn_time = 0.0;
    }

    void timestamp(int pool_size, int current_beam_width, double time) {
        pool_size_sum += pool_size;
        beam_width_sum += current_beam_width;
        time_sum += time;
        turn_sum++;

        // 指数移動平均の更新 (直近のデータを20%の重みで反映)
        if (turn_sum == 1) {
            ema_turn_time = time;
        } else {
            ema_turn_time = 0.8 * ema_turn_time + 0.2 * time;
        }
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

        if (remain_turn <= 0 || ema_turn_time <= 0.0) return prev_beam_width;

        // 残り時間から逆算した、1ターンあたりに使用できる目標時間
        double target_turn_time = remain_time / remain_turn;

        // 目標時間と実際の直近のターン時間の比率を計算
        double ratio = target_turn_time / ema_turn_time;

        // 変動率を制限 (急激なビーム幅の増減を防ぐため、0.8倍から1.2倍の範囲に収める)
        ratio = max(0.8, min(1.2, ratio));

        int next_beam_width = prev_beam_width * ratio;

        // 全体のビーム幅の絶対的な下限と上限を設定
        next_beam_width = max(beam_width / 10, min(beam_width * 5, next_beam_width));
        next_beam_width = max(1, next_beam_width);

        prev_beam_width = next_beam_width;
        return next_beam_width;
    }

    void report() const {
        cerr << to_bold("BeamParam-report----------------") << endl;
        if (turn_sum == 0) {
            cerr << "turn_sum = 0" << endl;
        } else {
            cerr << "ave_beam_width=" << (double)beam_width_sum / turn_sum << endl;
        }
        cerr << "--------------------------------" << endl;
    }
};
} // namespace flying_squirrel
