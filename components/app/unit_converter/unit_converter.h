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