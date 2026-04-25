#include "button.h"

static const char *TAG = "BUTTON";

#define BUTTON_COUNT 5

QueueHandle_t g_button_queue;
QueueHandle_t g_change_sys; 

static void IRAM_ATTR button_isr(void *arg){
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    
    BaseType_t Woken = pdFALSE;
    xQueueSendFromISR(g_button_queue, &gpio_num, &Woken);


    portYIELD_FROM_ISR(Woken);
}

void button_init(void)
{
    g_button_queue = xQueueCreate(15, sizeof(uint32_t));

    if(g_button_queue == NULL)
    {
        ESP_LOGE(TAG, "Queue create FAILED");
        return;
    }

    ESP_LOGI(TAG, "Queue created");

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,   // nhấn = xuống mức thấp
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
    };

    gpio_install_isr_service(0);

    uint8_t pins[BUTTON_COUNT] = {BTN_SELECT_PIN, BTN_UP_PIN, BTN_DOWN_PIN,BTN_LEFT_PIN, BTN_RIGHT_PIN};

    for(int i = 0; i < BUTTON_COUNT; i++)
    {
        io_conf.pin_bit_mask = (1ULL << pins[i]);
        gpio_config(&io_conf);

        gpio_isr_handler_add(pins[i], button_isr, (void*)(uintptr_t)pins[i]);
    }
    ESP_LOGI(TAG, "Button initialized!");
}
void button_task(void *arg)
{
    system_state_t *g_system_state = (system_state_t *)arg;
    uint32_t gpio_num;
    while(1)
    {
        if(xQueueReceive(g_button_queue, &gpio_num, portMAX_DELAY))
        {
            vTaskDelay(pdMS_TO_TICKS(50));   // debounce

            if(gpio_get_level(gpio_num) == 0) // vẫn đang nhấn
            {
                switch(gpio_num)
                {
                    case BTN_SELECT_PIN:
                    {
                        printf("SELECT pressed\t");
                        //system_event_t ev = EVENT_RUN_TOGGLE;
                       //xQueueSend(g_change_sys, &ev, 0);
                        break;
                    }
                    case BTN_LEFT_PIN:
                    {
                        printf("LEFT pressed\t");
                        //system_event_t ev = EVENT_FLOW_DEC;
                        //xQueueSend(g_change_sys, &ev, 0);
                        break;
                    }
                    case BTN_RIGHT_PIN:
                    {
                        printf("RIGHT pressed\t");
                        //system_event_t ev = EVENT_FLOW_INC;
                        //xQueueSend(g_change_sys, &ev, 0);
                        break;
                    }case BTN_UP_PIN:
                    {
                        printf("UP pressed\t");
                        //system_event_t ev = EVENT_FLOW_INC;
                        //xQueueSend(g_change_sys, &ev, 0);
                        break;
                    }case BTN_DOWN_PIN:
                    {
                        printf("DOWN pressed\t");
                        //system_event_t ev = EVENT_FLOW_INC;
                        //xQueueSend(g_change_sys, &ev, 0);
                        break;
                    }
                }

                while(gpio_get_level(gpio_num) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10)); // Ngủ 10ms rồi kiểm tra lại
                }

                xQueueReset(g_button_queue);
            }
        }
    }
}