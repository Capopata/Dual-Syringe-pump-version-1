#include <tmc2209.h>

static void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // install driver
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}
static esp_err_t uart_write(const uint8_t *msg, size_t len ){
    
    if(msg == NULL || len == 0){
        return ESP_ERR_INVALID_ARG;
    }

    int tx_bytes = uart_write_bytes(UART_NUM_1, msg, len);
    ESP_LOGI("TMC_UART", "Wrote %d bytes", tx_bytes);

    if(tx_bytes != len){
        return ESP_FAIL;
    }

    return ESP_OK;
}

// checking both read and write access by calculate CRC
static uint8_t TMC2209_CalculateCRC(uint8_t *datagram, uint8_t datagramLength){
    uint8_t crc = 0;

    for(uint8_t i = 0; i < datagramLength; i++ ){
        uint8_t currentByte = datagram[i];

        for(uint8_t j = 0; j<8; j++){
            if((crc >>7) ^(currentByte & 0x01)){
                crc = (crc << 1) ^ 0x07;
            }else{
                crc = (crc <<1);
            }
            currentByte >>= 1;
        }
    }

    return crc;
}
// [Giữ nguyên các hàm uart_init, uart_write, TMC2209_CalculateCRC, TMC2209_WriteRegister của bạn ở đây]

// Hàm Đọc dữ liệu từ TMC2209
uint32_t TMC2209_ReadRegister(uint8_t node_addr, uint8_t reg_addr) {
    uint8_t req[4];
    
    // Khởi tạo Read Request Datagram (4 bytes)
    req[0] = TMC2209_SYNC_BYTE; // 0x05
    req[1] = node_addr;
    req[2] = reg_addr & 0x7F;   // Đảm bảo bit 7 = 0 để IC hiểu đây là lệnh Read
    req[3] = TMC2209_CalculateCRC(req, 3);

    // Xóa sạch buffer RX trước khi gửi để tránh rác
    uart_flush_input(UART_NUM_1);
    
    // Gửi yêu cầu
    uart_write(req, 4);

    /* * XỬ LÝ 1-WIRE UART ECHO:
     * Vì TX nối với RX qua trở 1k, ESP32 sẽ nhận lại đúng 4 byte vừa gửi.
     * Sau đó TMC2209 mới gửi trả 8 byte Reply.
     * Tổng số byte cần đọc: 4 + 8 = 12 byte.
     */
    uint8_t rx_buf[16];
    int len = uart_read_bytes(UART_NUM_1, rx_buf, 12, 100 / portTICK_PERIOD_MS);
    ESP_LOGI("TMC2209_DEBUG", "Số byte nhận được (len) = %d", len);
    if (len >= 8) {
        // Trượt qua buffer để tìm Header của gói Reply (0x05, 0xFF, reg_addr)
        
        for (int i = 0; i <= len - 8; i++) {
            if (rx_buf[i] == 0x05 && rx_buf[i+1] == 0xFF && rx_buf[i+2] == reg_addr) {
                
                // Kiểm tra mã CRC của gói Reply
                uint8_t crc = TMC2209_CalculateCRC(&rx_buf[i], 7);
                if (crc == rx_buf[i+7]) {
                    // Ghép 4 byte dữ liệu (MSB first)
                    uint32_t data = (rx_buf[i+3] << 24) | 
                                    (rx_buf[i+4] << 16) | 
                                    (rx_buf[i+5] <<  8) | 
                                     rx_buf[i+6];
                    return data;
                } else {
                    ESP_LOGE("TMC2209", "Lỗi: Sai mã CRC phản hồi!");
                    return 0; 
                }
            }
        }
    }
    ESP_LOGE("TMC2209", "Lỗi: Không nhận được phản hồi hoặc Timeout");
    return 0;
}

// Hàm theo dõi dữ liệu để Tuning cơ cấu
void TMC2209_TuneStallGuard(uint8_t node_addr) {
    // 1. Đọc tải cơ khí hiện tại
    uint32_t sg_result = TMC2209_ReadRegister(node_addr, TMC2209_REG_SG_RESULT);
    
    // 2. Đọc trạng thái lỗi
    uint32_t drv_status = TMC2209_ReadRegister(node_addr, TMC2209_REG_DRV_STATUS);
    
    // Bóc tách các cờ quan trọng từ DRV_STATUS
    uint8_t stst = (drv_status >> 31) & 0x01; // Động cơ đang đứng yên
    uint8_t otpw = (drv_status >> 0) & 0x01;  // Cảnh báo quá nhiệt (Pre-warning)
    uint8_t ola  = (drv_status >> 6) & 0x01;  // Hở mạch cuộn A
    uint8_t olb  = (drv_status >> 7) & 0x01;  // Hở mạch cuộn B
    
    ESP_LOGI("TMC_TUNE", "SG_RESULT: %lu | Đứng yên: %d | Quá nhiệt: %d | Hở mạch: A=%d, B=%d", 
             (unsigned long)sg_result, stst, otpw, ola, olb);
}
//Package data and sent by UART
void TMC2209_WriteRegister(uint8_t node_addr, uint8_t reg_addr, uint32_t data){
    uint8_t msg[8];

    msg[0] = TMC2209_SYNC_BYTE;
    msg[1] = node_addr;
    msg[2] = reg_addr |TMC2209_WRITE_BIT;

    msg[3] = (uint8_t)((data>>24) & 0xFF);
    msg[4] = (uint8_t)((data>>16) & 0xFF);
    msg[5] = (uint8_t)((data>>8 ) & 0xFF);
    msg[6] = (uint8_t)(data & 0xFF);

    msg[7] = TMC2209_CalculateCRC(msg, 7);

    uart_write(msg, sizeof(msg));
}

void TMC_Init(uint8_t node_addr){
    uart_init();
    
    // 1. Cấu hình chung (Bật UART, Chỉnh vi bước, Nội trở)
    TMC2209_WriteRegister(node_addr, TMC2209_REG_GCONF, TMC2209_VAL_GCONF);

    // 2. Cấu hình dòng điện (Dòng chạy, Dòng giữ, Thời gian trễ)
    TMC2209_WriteRegister(node_addr, TMC2209_REG_IHOLD_IRUN, TMC2209_VAL_IHOLD_IRUN);
    TMC2209_WriteRegister(node_addr, TMC2209_REG_TPOWERDOWN, TMC2209_VAL_TPOWERDOWN);

    // 3. Cấu hình Vi bước và Chopper (Độ êm)
    TMC2209_WriteRegister(node_addr, TMC2209_REG_CHOPCONF, TMC2209_VAL_CHOPCONF);

    // 4. CẤU HÌNH LỰC KÉO TỐC ĐỘ THẤP (Ép bù biên độ PWM) - Thêm dòng này
    TMC2209_WriteRegister(node_addr, TMC2209_REG_PWMCONF, TMC2209_VAL_PWMCONF);

    // 5. Cấu hình StallGuard (Phát hiện kẹt)
    TMC2209_WriteRegister(node_addr, TMC2209_REG_TCOOLTHRS, TMC2209_VAL_TCOOLTHRS);
    TMC2209_WriteRegister(node_addr, TMC2209_REG_SGTHRS, TMC2209_VAL_SGTHRS);

    // 6. Cấp xung nội bộ (Chỉ bỏ comment nếu KHÔNG DÙNG chân STEP của ESP32 nữa)
    // TMC2209_WriteRegister(node_addr, TMC2209_REG_VACTUAL, TMC2209_VAL_VACTUAL);
}