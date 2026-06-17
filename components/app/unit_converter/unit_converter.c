#include "unit_converter.h"
#include <math.h>
static float freq_error_accumulator = 0.0f;
// Thêm vào file unit_converter.c
// 3. Hàm nội suy tuyến tính cơ bản
static float interpolate_calib(const calib_point_t *table, int size, float flow) {
    if (flow <= table[0].flow_rate) return table[0].calib_factor;
    if (flow >= table[size-1].flow_rate) return table[size-1].calib_factor;

    for (int i = 0; i < size - 1; i++) {
        if (flow >= table[i].flow_rate && flow <= table[i+1].flow_rate) {
            float f1 = table[i].flow_rate;
            float k1 = table[i].calib_factor;
            float f2 = table[i+1].flow_rate;
            float k2 = table[i+1].calib_factor;
            
            return k1 + (flow - f1) * (k2 - k1) / (f2 - f1);
        }
    }
    return 1.0f; 
}

// 4. Hàm giao tiếp gọi từ bên ngoài
float get_dynamic_calib_factor(int channel_id, float target_flow) {
    if (channel_id == 0) {
        return interpolate_calib(ch0_calib_table, CH0_TABLE_SIZE, target_flow);
    } else if (channel_id == 1) {
        return interpolate_calib(ch1_calib_table, CH1_TABLE_SIZE, target_flow);
    }
    return 1.0f;
}
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

float converter_steps_to_degree(int32_t steps){
    float steps_per_rev = 51200.0f;

    return ((float)steps *360.0f)/(steps_per_rev);

}

//  Chuyển đổi số bước (steps) lý thuyết sang thể tích (mL)
float converter_steps_to_ml(int32_t steps) {
    return (float)steps * ML_PER_STEP;
}

//  Chuyển đổi góc đo được từ cảm biến (degree) sang thể tích (mL)
float converter_degree_to_ml(float degree) {
    // 1 vòng = 360 độ = 51200 steps
    // Số bước tương ứng với góc quay = degree * (51200.0f / 360.0f)
    float steps_from_degree = degree * 142.222222f; 
    return steps_from_degree * ML_PER_STEP;
}

//  (Mở rộng) Nếu bạn muốn tính thẳng từ giá trị raw tick (0-4095) của AS5600
float converter_tick_to_ml(int32_t ticks) {
    return (float)ticks * STEPS_PER_ENCODER_TICK * ML_PER_STEP;
}