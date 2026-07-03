#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"

typedef enum{
    STEPPER_DIRECTION_CW = 1,
    STEPPER_DIRECTION_CCW = -1
}stepper_dir_t;

typedef struct{
    float acceleration; //steps/s^2
    float max_speed; //steps/s

    // Current state
    volatile long current_pos;
    volatile long target_pos;
    float current_speed; //steps/s

    //Internal variable of David Austin algorithm(AccelStepper)
    float cn; //Time current interval (us)
    float c0; // Time start interval (us)
    long n; //count variable in ramp
    float min_cn; //Thời gian tối thiểu tương ứng với vận tốc tối đa (max_speed)

}trapezoidal_profile_t;

/**
 * @brief Initialize trajectory calculator
 */
void profile_init(trapezoidal_profile_t *p, float accel, float max_speed);

/**
 * @brief Create new position
 */
void profile_move_to(trapezoidal_profile_t *p, long absolute_pos);
/**
 * @brief Calculate time for next step (us)
 * @brief Should be could after one step is run
 * @return micro second need to wait for next step. 0 if coming goal
 */
uint32_t profile_compute_nex_step_interval(trapezoidal_profile_t *p);

/**
 * @brief Check if the stepper is still running
 * @return true if the stepper is still running
 */
bool profile_is_running(trapezoidal_profile_t *p);