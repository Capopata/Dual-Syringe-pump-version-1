#include <tmc2209.h>
static bool is_uart_initialized = false;

esp_err_t uart_init(uart_port_t port, int tx_pin, int rx_pin) {
    // 1. CHỈ cài đặt driver nếu cổng UART này CHƯA được khởi tạo
    if (!uart_is_driver_installed(port)) {
        esp_err_t err = uart_driver_install(port, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
        if (err != ESP_OK) {
            return err; // Hoặc ESP_ERROR_CHECK(err) tùy cách bạn quản lý lỗi
        }
    } else {
        // Cổng đã được cài đặt driver trước đó, chỉ cần xóa bớt dữ liệu rác trong buffer (nếu có)
        uart_flush(port);
    }

    // 2. Các bước cấu hình Parameter và Pin bên dưới giữ nguyên (hoặc bọc lại nếu cần)
    uart_config_t uart_config = {
        .baud_rate = 115200, // Thay bằng baudrate thực tế của bạn
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    return ESP_OK;
}
void cmd_uart_init(void) {
    uart_config_t cfg = {
        .baud_rate  = CMD_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(CMD_UART_PORT, &cfg);
    // UART0: TX=1, RX=3 — mặc định của ESP32, không cần set lại
    uart_driver_install(CMD_UART_PORT, 1024, 0, 0, NULL, 0);
}
void uart_loopback_test(uart_port_t port, int tx_pin, int rx_pin) {
    // Nối vật lý TX_PIN với RX_PIN bằng dây jumper trước khi test này
    uint8_t tx_data[] = {0xAA, 0x55, 0x01, 0x02};
    uint8_t rx_data[4] = {0};
    
    uart_flush_input(port);
    uart_write_bytes(port, tx_data, 4);
    uart_wait_tx_done(port, pdMS_TO_TICKS(10));
    
    int len = uart_read_bytes(port, rx_data, 4, pdMS_TO_TICKS(50));
    
    if (len == 4 && memcmp(tx_data, rx_data, 4) == 0) {
        ESP_LOGI("LOOPBACK", "OK — UART port=%d TX=%d RX=%d hoạt động tốt", port, tx_pin, rx_pin);
    } else {
        ESP_LOGE("LOOPBACK", "FAIL — nhận %d bytes", len);
        ESP_LOG_BUFFER_HEX("LOOPBACK", rx_data, len);
    }
}
static esp_err_t uart_write(uart_port_t port,const uint8_t *msg, size_t len ){
    
    if(msg == NULL || len == 0){
        return ESP_ERR_INVALID_ARG;
    }

    int tx_bytes = uart_write_bytes(port, msg, len);
    ESP_LOGI("TMC_UART", "Wrote %d bytes", tx_bytes);

    return (tx_bytes == (int)len) ? ESP_OK : ESP_FAIL;
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
uint32_t TMC2209_ReadRegister(uart_port_t port, uint8_t node_addr, uint8_t reg_addr) {
    uint8_t req[4];
    req[0] = TMC2209_SYNC_BYTE;
    req[1] = node_addr;
    req[2] = reg_addr & 0x7F;
    req[3] = TMC2209_CalculateCRC(req, 3);

    uart_flush_input(port);
    uart_write_bytes(port, req, 4);
    uart_wait_tx_done(port, pdMS_TO_TICKS(10));

    // ✅ Chỉ đọc 4 echo trước, sau đó đọc 8 response riêng
    uint8_t echo_buf[4];
    uart_read_bytes(port, echo_buf, 4, pdMS_TO_TICKS(20));  // discard echo

    uint8_t resp[8] = {0};
    int len = uart_read_bytes(port, resp, 8, pdMS_TO_TICKS(100));

    ESP_LOGI("TMC2209_DEBUG", "Node %d | resp_len=%d", node_addr, len);

    if (len < 8) {
        ESP_LOGE("TMC2209", "Read failed: chỉ nhận %d/8 bytes response", len);
        return 0;
    }

    if (resp[0] != 0x05 || resp[1] != 0xFF) {
        ESP_LOGE("TMC2209", "Header sai: 0x%02X 0x%02X", resp[0], resp[1]);
        return 0;
    }

    uint8_t crc = TMC2209_CalculateCRC(resp, 7);
    if (crc != resp[7]) {
        ESP_LOGE("TMC2209", "CRC sai: got 0x%02X, exp 0x%02X", resp[7], crc);
        return 0;
    }

    return ((uint32_t)resp[3] << 24) |
           ((uint32_t)resp[4] << 16) |
           ((uint32_t)resp[5] <<  8) |
            (uint32_t)resp[6];
}

// Hàm theo dõi dữ liệu để Tuning cơ cấu
void TMC2209_TuneStallGuard(uart_port_t port, uint8_t node_addr) {
    uint32_t sg_result  = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_SG_RESULT);
    uint32_t drv_status = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_DRV_STATUS);
    
    uint8_t stst = (drv_status >> 31) & 0x01;
    uint8_t otpw = (drv_status >> 0) & 0x01; 
    uint8_t ola  = (drv_status >> 6) & 0x01; 
    uint8_t olb  = (drv_status >> 7) & 0x01; 
    
    ESP_LOGI("TMC_TUNE", "Node %d | SG_RESULT: %lu | Đứng yên: %d | Quá nhiệt: %d | Hở mạch: A=%d, B=%d", 
             node_addr, (unsigned long)sg_result, stst, otpw, ola, olb);
}
//Package data and sent by UART
void TMC2209_WriteRegister(uart_port_t port, uint8_t node_addr,
                            uint8_t reg_addr, uint32_t data) {
    uint8_t msg[8];
    msg[0] = TMC2209_SYNC_BYTE;
    msg[1] = node_addr;
    msg[2] = reg_addr | TMC2209_WRITE_BIT;
    msg[3] = (data >> 24) & 0xFF;
    msg[4] = (data >> 16) & 0xFF;
    msg[5] = (data >>  8) & 0xFF;
    msg[6] =  data        & 0xFF;
    msg[7] = TMC2209_CalculateCRC(msg, 7);

    uart_flush_input(port);              // dọn rác trước khi gửi
    uart_write_bytes(port, msg, 8);
    uart_wait_tx_done(port, pdMS_TO_TICKS(10));

    // ✅ Discard 8 bytes echo
    uint8_t echo_buf[8];
    uart_read_bytes(port, echo_buf, 8, pdMS_TO_TICKS(20));
}

void check_chip_alive(uart_port_t port, uint8_t node_addr) {
    // Đọc giá trị IFCNT hiện tại
    uint32_t count1 = TMC2209_ReadRegister(port, node_addr, 0x02);
    
    // Thử ghi một giá trị vào thanh ghi không gây hại (ví dụ SGTHRS)
    TMC2209_WriteRegister(port, node_addr, 0x40, 100); 
    
    // Đọc lại IFCNT
    uint32_t count2 = TMC2209_ReadRegister(port, node_addr, 0x02);

    if (count2 > count1) {
        ESP_LOGI("CHECK", "Chip alive! IFCNT increased from %lu to %lu", count1, count2);
    } else {
        ESP_LOGE("CHECK", "Chip is not responding or Write failed!");
    }
}
void TMC2209_Check_DRV_STATUS(uart_port_t port, uint8_t node_addr) {
    // Đọc thanh ghi DRV_STATUS
    uint32_t drv_status = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_DRV_STATUS);
    
    // Nếu không đọc được (bằng 0 do lỗi UART), thoát luôn
    if (drv_status == 0) return; 

    // ---- BÓC TÁCH CÁC BIT LỖI (Dựa theo Datasheet TMC2209) ----
    uint8_t otpw  = (drv_status >> 0) & 0x01; // Overtemperature pre-warning (Sắp quá nhiệt)
    uint8_t ot    = (drv_status >> 1) & 0x01; // Overtemperature (Quá nhiệt, chip tự ngắt)
    uint8_t s2ga  = (drv_status >> 2) & 0x01; // Short to ground phase A (Chập mass cuộn A)
    uint8_t s2gb  = (drv_status >> 3) & 0x01; // Short to ground phase B (Chập mass cuộn B)
    uint8_t s2vsa = (drv_status >> 4) & 0x01; // Short to supply phase A (Chập nguồn cuộn A)
    uint8_t s2vsb = (drv_status >> 5) & 0x01; // Short to supply phase B (Chập nguồn cuộn B)
    uint8_t ola   = (drv_status >> 6) & 0x01; // Open load phase A (Hở mạch cuộn A)
    uint8_t olb   = (drv_status >> 7) & 0x01; // Open load phase B (Hở mạch cuộn B)
    uint8_t stst  = (drv_status >> 31) & 0x01; // Standstill (Động cơ đang đứng yên)

    // Lấy thông tin dòng điện hiện tại đang cấp cho motor (Bit 16 đến 20)
    uint8_t cs_actual = (drv_status >> 16) & 0x1F;

    ESP_LOGI("TMC_DIAG", "--- KẾT QUẢ QUÉT DRV_STATUS (0x%08lX) ---", (unsigned long)drv_status);
    ESP_LOGI("TMC_DIAG", "Dòng điện (CS_ACTUAL): %d/31 | Đứng yên: %s", cs_actual, stst ? "CÓ" : "KHÔNG");

    // ---- KIỂM TRA VÀ IN RA LỖI TỪNG HẠNG MỤC ----
    bool has_error = false;

    // Lỗi nghiêm trọng (Chập mạch) -> Thường do đấu sai dây motor
    if (s2ga || s2gb || s2vsa || s2vsb) {
        ESP_LOGE("TMC_ERROR", "PHÁT HIỆN CHẬP MẠCH (SHORT CIRCUIT)! RÚT ĐIỆN NGAY!");
        if (s2ga) ESP_LOGE("TMC_ERROR", "- Cuộn A bị chập xuống GND");
        if (s2gb) ESP_LOGE("TMC_ERROR", "- Cuộn B bị chập xuống GND");
        if (s2vsa) ESP_LOGE("TMC_ERROR", "- Cuộn A bị chập lên nguồn VM");
        if (s2vsb) ESP_LOGE("TMC_ERROR", "- Cuộn B bị chập lên nguồn VM");
        has_error = true;
    }

    // Lỗi hở mạch (Chưa cắm motor hoặc đứt cáp)
    if (ola || olb) {
        ESP_LOGW("TMC_WARN", "PHÁT HIỆN HỞ MẠCH (OPEN LOAD):");
        if (ola) ESP_LOGW("TMC_WARN", "- Cuộn A chưa cắm hoặc đứt dây");
        if (olb) ESP_LOGW("TMC_WARN", "- Cuộn B chưa cắm hoặc đứt dây");
        ESP_LOGW("TMC_WARN", "(Lưu ý: Nếu motor đang quay nhanh quá hoặc cấp dòng thấp quá cũng có thể báo hở mạch ảo)");
        has_error = true;
    }

    // Lỗi nhiệt độ
    if (ot) {
        ESP_LOGE("TMC_ERROR", "QUÁ NHIỆT (OVER TEMP): Driver đã tự ngắt để bảo vệ!");
        has_error = true;
    } else if (otpw) {
        ESP_LOGW("TMC_WARN", "CẢNH BÁO NHIỆT (PRE-WARNING): Driver đang rất nóng, sắp ngắt!");
        has_error = true;
    }

    if (!has_error) {
        ESP_LOGI("TMC_DIAG", "=> Trạng thái hoàn hảo, không có lỗi gì!");
    }
    ESP_LOGI("TMC_DIAG", "------------------------------------------");
}
void TMC2209_Full_Report(uart_port_t port, uint8_t node_addr) {
    uint32_t gstat = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_GSTAT);
    uint32_t drv_status = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_DRV_STATUS);

    printf("\n--- [TMC2209 TOTAL DIAGNOSTIC - NODE %d] ---\n", node_addr);

    // 1. KIỂM TRA TRẠNG THÁI HỆ THỐNG (GSTAT)
    printf("[Hệ thống]: ");
    if (gstat == 0) {
        printf("OK\n");
    } else {
        if (gstat & 0x01) printf("! Reset hệ thống (vừa bật nguồn) ");
        if (gstat & 0x02) printf("! Lỗi Driver (Cần kiểm tra DRV_STATUS) ");
        if (gstat & 0x04) printf("! Sụt áp mạch nạp (UV_CP) ");
        printf("\n");
    }

    // 2. KIỂM TRA NHIỆT ĐỘ (DRV_STATUS)
    printf("[Nhiệt độ]: ");
    uint8_t otpw = (drv_status >> 0) & 0x01;
    uint8_t ot   = (drv_status >> 1) & 0x01;
    if (ot) printf("NGUY HIỂM: QUÁ NHIỆT (SHUTDOWN)!\n");
    else if (otpw) printf("CẢNH BÁO: ĐANG NÓNG DẦN\n");
    else {
        // Kiểm tra các ngưỡng nhiệt độ cụ thể
        if ((drv_status >> 11) & 0x01) printf("> 157°C\n");
        else if ((drv_status >> 10) & 0x01) printf("> 150°C\n");
        else if ((drv_status >> 9) & 0x01) printf("> 143°C\n");
        else if ((drv_status >> 8) & 0x01) printf("> 120°C\n");
        else printf("Bình thường (< 120°C)\n");
    }

    // 3. KIỂM TRA MẠCH ĐỘNG CƠ (DRV_STATUS)
    printf("[Động cơ]: ");
    uint8_t ola = (drv_status >> 6) & 0x01;
    uint8_t olb = (drv_status >> 7) & 0x01;
    uint8_t s2ga = (drv_status >> 2) & 0x01;
    uint8_t s2gb = (drv_status >> 3) & 0x01;

    if (s2ga || s2gb) printf("CẢNH BÁO: CHẬP MASS (Short to GND)!\n");
    else if (ola && olb) printf("CHƯA CẮM MOTOR (Hoặc đứt cả 2 cuộn)\n");
    else if (ola || olb) printf("HỞ MẠCH: %s\n", ola ? "Cuộn A" : "Cuộn B");
    else printf("Đã kết nối, sẵn sàng\n");

    // 4. TRẠNG THÁI VẬN HÀNH
    printf("[Vận hành]: ");
    uint8_t stst = (drv_status >> 31) & 0x01;
    uint8_t stealth = (drv_status >> 30) & 0x01;
    uint8_t cs_actual = (drv_status >> 16) & 0x1F;
    printf("%s | Chế độ: %s | Dòng thực tế: %d/31\n", 
            stst ? "Đang đứng yên" : "Đang quay",
            stealth ? "StealthChop" : "SpreadCycle",
            cs_actual);

    printf("-------------------------------------------\n\n");
}

void test_tmc_override(uart_port_t port, uint8_t node_addr) {
    ESP_LOGI("TEST", "--- Bat dau ghi de cau hinh de kiem tra ---");

    // 1. Ghi de GCONF: 
    // Gia tri cu cua ban la 0xC7 (nhi phân: ...11000111)
    // Gia tri moi: 0xC6 (nhi phân: ...11000110) -> Bit 0 = 0 (bo qua VREF pin)
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_GCONF, 0x000000C6);
    
    // 2. Thiet lap dong dien qua UART (IHOLD_IRUN)
    // IRUN (bit 16-20): 16 (khoang 50% dong max)
    // IHOLD (bit 0-4): 8 (dong giu khi dung yen)
    // IHOLDDELAY (bit 8-11): 1
    uint32_t current_val = (1 << 16) | (16 << 8) | (8); 
    // Hoac dung gia tri hex thu cong cho chac chan: 
    // 0x00011008 -> IRUN=16, IHOLD=8
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_IHOLD_IRUN, 0x00011008);

    ESP_LOGI("TEST", "Da ghi de GCONF va IHOLD_IRUN");
}

void TMC_Init(uart_port_t port, uint8_t node_addr) {

    // Nạp cấu hình dựa theo địa chỉ Node
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_GCONF, TMC2209_VAL_GCONF);
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_IHOLD_IRUN, TMC2209_VAL_IHOLD_IRUN);
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_TPOWERDOWN, TMC2209_VAL_TPOWERDOWN);
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_CHOPCONF, TMC2209_VAL_CHOPCONF);
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_PWMCONF, TMC2209_VAL_PWMCONF);
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_TCOOLTHRS, TMC2209_VAL_TCOOLTHRS);
    TMC2209_WriteRegister(port, node_addr, TMC2209_REG_SGTHRS, TMC2209_VAL_SGTHRS);
    
//     vTaskDelay(pdMS_TO_TICKS(10));
//     uint32_t gconf     = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_GCONF);
//     uint32_t chopconf  = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_CHOPCONF);
//     uint32_t ihold     = TMC2209_ReadRegister(port, node_addr, TMC2209_REG_IHOLD_IRUN);

//     ESP_LOGI("TMC", "Node %d | GCONF=0x%08lX (exp=0x%08X)", 
//              node_addr, gconf, TMC2209_VAL_GCONF);
//     ESP_LOGI("TMC", "Node %d | CHOPCONF=0x%08lX (exp=0x%08X)", 
//              node_addr, chopconf, TMC2209_VAL_CHOPCONF);
//     ESP_LOGI("TMC", "Node %d | IHOLD_IRUN=0x%08lX (exp=0x%08X)", 
//              node_addr, ihold, TMC2209_VAL_IHOLD_IRUN);

//     if (gconf != TMC2209_VAL_GCONF || chopconf != TMC2209_VAL_CHOPCONF) {
//         ESP_LOGE("TMC", "Node %d CONFIG MISMATCH — retry!", node_addr);
//     }
// }
}