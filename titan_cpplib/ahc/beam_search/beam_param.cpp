#pragma once

#include "titan_cpplib/others/print.cpp"

namespace flying_squirrel { // flying squirrel

struct BeamParam {
    int max_turn, beam_width;
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
    }

    void timestamp(int pool_size, int beam_width, double time) {
        pool_size_sum += pool_size;
        beam_width_sum += beam_width;
        time_sum += time;
        turn_sum++;
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
