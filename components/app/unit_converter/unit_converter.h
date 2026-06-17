#pragma once

#include <stdint.h>
#include <stdbool.h>

// Syringe 1mL (Diameter: 4.6mm)
#define SYRINGE_AREA_MM2         16.619025f
// 1 step = 0.0000078125 mm = 0.000000129836 mL
#define ML_PER_STEP              0.000000129836f
#define STEPS_PER_ML             7701955.0f
// => 1 Encoder tick = 12.5 Motor steps
#define STEPS_PER_ENCODER_TICK   12.5f
// 1. Khai báo Bảng số liệu Lookup Table cho Kênh 0

// Thêm vào cuối file unit_converter.h
typedef struct {
    float flow_rate;    // Lưu lượng (mL/h)
    float calib_factor; // Hệ số bù sai số K
} calib_point_t;

static const calib_point_t ch0_calib_table[] = {
    {0.03f, 1.000f}, 
    {0.06f, 0.987f}, 
    {0.60f, 1.000f}, 
    {1.50f, 0.955f}, 
    {3.00f, 0.926f}  
};
#define CH0_TABLE_SIZE (sizeof(ch0_calib_table)/sizeof(ch0_calib_table[0]))

// 2. Khai báo Bảng số liệu Lookup Table cho Kênh 1
static const calib_point_t ch1_calib_table[] = {
    {0.03f, 1.027f},
    {0.06f, 0.955f},
    {0.60f, 1.034f},
    {1.50f, 0.968f},
    {3.00f, 0.926f} 
};
#define CH1_TABLE_SIZE (sizeof(ch1_calib_table)/sizeof(ch1_calib_table[0]))

/**
 * @brief Lấy hệ số K đã nội suy tuyến tính dựa trên dải lưu lượng
 * @param channel_id Kênh 0 hoặc 1
 * @param target_flow Lưu lượng mục tiêu (mL/h)
 * @return Hệ số K (float)
 */
float get_dynamic_calib_factor(int channel_id, float target_flow);

/**
 * @brief total steps of stepper motor for 1 unit of volume
 * @param target_ml Volume need to infuse (ml)
 * @return microstepping (steps)
*/
uint32_t converter_ml_to_steps(float target_ml);

/**
 * @brief Convert flowrate setpoint to max linear velocity
 * @param flow_ml_h Flowrate setpoint (ml/h)
 * @return Max linear velocity (mm/s) is used for Trapezoidal profile
*/
float converter_flow_to_velocity_mms(float flow_ml_h);

/**
 * @brief Convert max velocity to pulse frequency
 * @param velocity_mms Flowrate setpoint (mm/s)
 * @return Pulse frequency (Hz or Steps/sec)
 */
uint32_t converter_velocity_to_freq(float velocity_mm);

/**
 * @brief Calculate volume infused from encoder pulse
 * @param encoder_ticks number of ticks difference to read from AS5600
 * @return Volume infused (ml)
 */
float converter_encoder_to_ml(int32_t encoder_ticks);

/**
 * @brief Calculate error position between theory and feedback encoder
 * @param expected_steps number of steps of MCU has been emitted
 * @param actual_encoder_ticks number of tick read from AS5600
 * @return Error convert to Steps to give PID
 */
int32_t converter_calculate_pid_error(uint32_t expected_steps, int32_t actual_encoder_ticks);

/**
 * @brief Calculate degree from steps
 * @param steps number of steps
 * @return degree
 */
float converter_steps_to_degree(int32_t steps);

float converter_steps_to_ml(int32_t steps);

float converter_degree_to_ml(float degree);

float converter_tick_to_ml(int32_t ticks);