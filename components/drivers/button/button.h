#pragma once

#include<stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include "domain.h"

#define BTN_DEBOUNCE            50

#define BTN_SELECT_PIN          1
#define BTN_UP_PIN              2
#define BTN_DOWN_PIN            3
#define BTN_LEFT_PIN            4
#define BTN_RIGHT_PIN           5

void button_init(void);
void button_task(void *arg);


extern QueueHandle_t g_change_sys;
extern QueueHandle_t system_event_queue;