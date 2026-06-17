#ifndef TMC2209_H
#define TMC2209_H

#include <stdint.h>
#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "io_config.h"

// node_addr luôn = 0x00 vì mỗi driver độc quyền trên UART của nó
#define TMC_NODE_ADDR       0x00
#define RX_BUF_SIZE             512


// define UART basic parameter
#define TMC2209_WRITE_BIT       0x80
#define TMC2209_SYNC_BYTE       0x05

#define TMC2209_REG_DRV_STATUS 0x6F
#define TMC2209_REG_GSTAT      0x01
#define TMC2209_REG_DRV_STATUS 0x6F

/* MAIN REGISTER ADDRESS OF TMC2209 */
/* Global configuration */
#define TMC2209_REG_GCONF       0x00
/* Control current: Run current, hold current */
#define TMC2209_REG_IHOLD_IRUN  0x10
/* Waiting time before reduce current */
#define TMC2209_REG_TPOWERDOWN  0x11
/* Low threshold velocity */
#define TMC2209_REG_TCOOLTHRS   0x14
/* Internal step pulse generator */
#define TMC2209_REG_VACTUAL     0x22
/* Sensitivity threshold for detecting jamming */
#define TMC2209_REG_SGTHRS      0x40
/* Chopper configuration and microstep resolution */
#define TMC2209_REG_CHOPCONF    0x6C
/* StealthChop PWM configuration (Bổ sung để ép lực kéo ở tốc độ thấp) */
#define TMC2209_REG_PWMCONF     0x70

/* READ-ONLY REGISTERS FOR TUNING AND DIAGNOSTICS */
#define TMC2209_REG_TSTEP       0x12 // Actual measured time between two 1/256 microsteps
#define TMC2209_REG_SG_RESULT   0x41 // StallGuard result (0-510). Lower value = higher load
#define TMC2209_REG_MSCNT       0x6A // Microstep counter (0-1023)
#define TMC2209_REG_DRV_STATUS  0x6F // Driver status flags (Errors, temp, stall)

/* CONFIG GLOBAL REGISTER */
// Use UART, allow set microstep via UART, dùng Internal Rsense
#define TMC2209_VAL_GCONF       0x000000C3 

// MRES = 0000 (256 microsteps), INTPOL = 1, TOFF = 3
#define TMC2209_VAL_CHOPCONF    0x10000053 

/* CẤU HÌNH STEALTHCHOP Ở TỐC ĐỘ SIÊU THẤP (Thắng ma sát tĩnh) */
// Tắt pwm_autoscale (bit 18 = 0), nạp cứng PWM_OFS (bit 7..0) = 100 (0x64)
#define TMC2209_VAL_PWMCONF     0xC40C0096

// time = 1s
#define TMC2209_VAL_TPOWERDOWN  0x00000014
// Vận tốc quay nội bộ siêu chậm
#define TMC2209_VAL_VACTUAL     5000 
// Turn on StallGuard
#define TMC2209_VAL_TCOOLTHRS   0x000FFFFF
// Threshold detect step touch/stuck 0-255
#define TMC2209_VAL_SGTHRS      50 

/* CALCULATE CURRENT AND STANDSTILL */
// Nâng IHOLD lên 60% của IRUN để tránh mất lực khi vừa khởi động
// IRUN (bit 12..8) = 25 (0x19)
// IHOLD (bit 4..0) = 15 (0x0F)
// IHOLDDELAY (bit 19..16) = 1
#define TMC2209_VAL_IHOLD_IRUN  0x0001190F




/*HEADER FUNCTION*/
void uart_loopback_test(uart_port_t port, int tx_pin, int rx_pin);
uint32_t TMC2209_ReadRegister(uart_port_t port, uint8_t node_addr, uint8_t reg_addr);
void TMC2209_WriteRegister(uart_port_t port,uint8_t node_addr,
                            uint8_t reg_addr, uint32_t data);
void TMC2209_TuneStallGuard(uart_port_t port, uint8_t node_addr);
void TMC_Init(uart_port_t port, uint8_t node_addr);
esp_err_t uart_init(uart_port_t port, int tx_pin, int rx_pin);
void check_chip_alive(uart_port_t port, uint8_t node_addr);
void TMC2209_Check_DRV_STATUS(uart_port_t port, uint8_t node_addr);
void TMC2209_Full_Report(uart_port_t port, uint8_t node_addr);
void test_tmc_override(uart_port_t port, uint8_t node_addr);
#endif