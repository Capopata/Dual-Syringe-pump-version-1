#include "unit_converter.h"
#include <math.h>
static float freq_error_accumulator = 0.0f;

uint32_t converter_ml_to_steps(float target_ml) {
    if (target_ml <= 0.0f) return 0;
    // Chuyển đổi và làm tròn thành số nguyên
    return (uint32_t)(target_ml * STEPS_PER_ML);
}

float converter_flow_to_velocity_mms(float flow_ml_h) {
    if (flow_ml_h <= 0.0f) return 0.0f;
    
    // 1. Đổi mL/h sang mm^3/s (1 mL = 1000 mm^3, 1 giờ = 3600 giây)
    // Q(mm^3/s) = flow_ml_h * (1000.0 / 3600.0) = flow_ml_h / 3.6
    float flow_mm3_s = flow_ml_h / 3.6f;
    
    // 2. V(mm/s) = Q(mm^3/s) / A_p(mm^2)
    float velocity_mms = flow_mm3_s / SYRINGE_AREA_MM2;
    
    return velocity_mms;
}

//Xử lý bù sai số cộng dồn (Error Accumulator) + chuyển từ vận tốc sang tần số xung
uint32_t converter_velocity_to_freq(float velocity_mms) {
    if (velocity_mms <= 0.0f) return 0;
    
    // Tần số thực tế cần phát
    float exact_freq = velocity_mms * 128000.0f; 
    
    // Cộng dồn sai số từ chu kỳ trước
    exact_freq += freq_error_accumulator;
    
    // Phần nguyên đưa vào phần cứng
    uint32_t hardware_freq = (uint32_t)exact_freq;
    
    // Lưu lại phần dư cho chu kỳ sau
    freq_error_accumulator = exact_freq - (float)hardware_freq;
    
    return hardware_freq;
}

float converter_encoder_to_ml(int32_t encoder_ticks) {
    // 1. Đổi tick của encoder ra số bước động cơ tương đương
    float equivalent_steps = (float)encoder_ticks * STEPS_PER_ENCODER_TICK;
    
    // 2. Đổi số bước ra thể tích
    return equivalent_steps * ML_PER_STEP;
}

int32_t converter_calculate_pid_error(uint32_t expected_steps, int32_t actual_encoder_ticks) {
    // Quy đổi encoder về hệ quy chiếu "bước" để so sánh
    int32_t actual_steps = (int32_t)((float)actual_encoder_ticks * STEPS_PER_ENCODER_TICK);
    
    // Tính sai số
    // Nếu error > 0: Motor đang chạy chậm hơn lý thuyết -> PID cần bù dương
    // Nếu error < 0: Motor chạy lố (hiếm khi xảy ra có tải) -> PID bù âm
    return (int32_t)expected_steps - actual_steps;
}