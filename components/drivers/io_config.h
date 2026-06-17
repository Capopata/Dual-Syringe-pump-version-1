#pragma once
//==================STEPPER MOTOR 0/1 PIN=======================//
#define CH0_STEP_PIN            25
#define CH0_DIR_PIN             2
#define CH0_EN_PIN              27

#define CH1_STEP_PIN            14
#define CH1_DIR_PIN             13
#define CH1_EN_PIN              17

// //=================BUTTON PIN ========================//
// #define BTN_SELECT_PIN          1
// #define BTN_UP_PIN              0
// #define BTN_DOWN_PIN            4
// #define BTN_LEFT_PIN            3
// #define BTN_RIGHT_PIN           2

//=================TFT PIN===========================//
#define LCD_HOST                SPI2_HOST
#define PIN_NUM_SCLK            18
#define PIN_NUM_MOSI            23
#define PIN_NUM_MISO            19
#define PIN_NUM_LCD_DC          16
#define PIN_NUM_LCD_CS          5
#define PIN_NUM_LCD_RST         -1
#define PIN_NUM_BK_LIGHT        12

//================UART PIN===========================//
#define SHARED_UART_PORT        UART_NUM_1
#define SHARED_TX_PIN           15
#define SHARED_RX_PIN           26
#define CMD_UART_PORT       UART_NUM_0
#define CMD_UART_BAUD       115200

//===============Channel direction==================//
#define CH0_INFUSE              1
#define CH0_WITHDRAW            0
#define CH1_INFUSE              1
#define CH1_WITHDRAW            0

#define infuse                  0

#define PUMP_CH1_NODE           0 
#define PUMP_CH2_NODE           1 