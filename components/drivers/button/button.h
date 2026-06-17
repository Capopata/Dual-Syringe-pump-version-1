#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "domain.h"
#include "tft.h"

void button_init(void);
void button_task(void *arg);
