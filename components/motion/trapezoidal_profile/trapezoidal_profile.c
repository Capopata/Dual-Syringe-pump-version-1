#include "trapezoidal_profile.h"
#include <math.h>

void profile_init(trapezoidal_profile_t *p, float accel, float max_speed){
    p->acceleration = accel;
    p->max_speed = max_speed;
    p->current_pos = 0;
    p->target_pos = 0;
    p->current_speed = 0.0;
    p->n = 0;
    p->cn = 0.0;

    // calculate c0: c0 = 0.676 * sqrt(2/accel) * 10^6
    // This is time of first step form start (v = 0)
    p->c0 = 0.676f *sqrt(2.0/accel) *1000000.0;

    //Calculat min_cn based on max speed
    p->min_cn = 1000000.0/max_speed;

}

void profile_move_to(trapezoidal_profile_t *p, long absolute_pos){
    if(p->target_pos != absolute_pos){
        p->target_pos = absolute_pos;
        // if motor stand still, reset n to start acceleration
        if(p->current_speed == 0.0){
            p->n = 0;
        }
    }
}

uint32_t profile_compute_nex_step_interval(trapezoidal_profile_t *p) {
    long distance_to = p->target_pos - p->current_pos;

    if (distance_to == 0) {
        p->current_speed = 0.0f;
        p->cn = 0.0f;
        p->n = 0;
        return 0;
    }

    // KIỂM TRA AN TOÀN: Tránh chia cho 0 nếu acceleration chưa được nạp
    if (p->acceleration <= 0.01f) {
        return 2000; // Trả về đại 1 giá trị an toàn (2000us) để không crash
    }

    // 1. Tính toán số bước cần để dừng: steps = v^2 / 2a
    // Dùng 2.0f để ép tính toán float 32-bit (Cực kỳ quan trọng)
    long steps_to_stop = (long)((p->current_speed * p->current_speed) / (2.0f * p->acceleration));

    // 2. Logic kiểm soát Ramp (Gia tốc / Giảm tốc)
    if (distance_to > 0) { // Đi thuận
        if (p->n > 0 && steps_to_stop >= distance_to) {
            p->n = -steps_to_stop;
        }
    } else { // Đi nghịch
        if (p->n > 0 && steps_to_stop >= -distance_to) {
            p->n = -steps_to_stop;
        }
    }

    // Thuật toán David Austin - Tính time interval(cn) cho bước tiếp theo
    if (p->n == 0) {
        p->cn = p->c0; 
    } else {
        float denominator = (4.0f * p->n) + 1.0f;
        
        // Bảo vệ mẫu số không được bằng 0
        if (denominator != 0.0f) {
            p->cn = p->cn - ((2.0f * p->cn) / denominator);
        }

        // Giới hạn tốc độ tối đa (min_cn)
        if (p->cn < p->min_cn) {
            p->cn = p->min_cn;
        }
    }

    p->n++;
    float micro_sec = 1000000.0f;
    p->current_speed = micro_sec / p->cn;

    return (uint32_t)p->cn;
}

bool profile_is_running(trapezoidal_profile_t *p){
    return (p->current_speed != 0.0 || p->target_pos != p->current_pos);
}
