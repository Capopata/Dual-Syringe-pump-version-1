#include "as5600.h"

// as5600_t as5600;

esp_err_t as5600_init(as5600_t *sensor, i2c_master_dev_handle_t dev){
    sensor ->dev = dev;
    return ESP_OK;
}

esp_err_t as5600_read_raw_angle(as5600_t *sensor, uint16_t *angle)
{
    uint8_t reg = AS5600_REG_RAW_ANGLE_H;
    uint8_t data[2];

    esp_err_t err = i2c_master_transmit_receive(
        sensor->dev,
        &reg, 1, 
        data, 2, 
        100
    );

    if(err != ESP_OK) return err;

    *angle = ((data[0]<<8))|data[1];
    
    return ESP_OK;
}

esp_err_t as5600_read_angle(as5600_t *sensor, uint16_t *angle)
{
    uint8_t reg = AS5600_REG_ANGLE_H;
    uint8_t data[2];

    esp_err_t err = i2c_master_transmit_receive(
        sensor->dev,
        &reg, 1, 
        data, 2, 
        100
    );

    if (err != ESP_OK) return err;

    *angle = ((data[0]<<8)) |data[1];

    return ESP_OK;
}

esp_err_t as5600_read_status(as5600_t *sensor, uint8_t *status )
{
    uint8_t reg = AS5600_REG_STATUS;
    uint8_t data[1];

    esp_err_t err = i2c_master_transmit_receive(
        sensor ->dev, 
        &reg, 1, 
        data, 1, 
        100
    );
    if (err!= ESP_OK) return err;
    
    *status = data[0];

    return ESP_OK;
}
/**
 * @brief Cấu hình bộ lọc của cảm biến AS5600 về mức 
 * lọc chậm tối đa (SF = 16x) và tắt bộ lọc nhanh (FTH = slow only), đồng 
 * thời giữ nguyên toàn bộ các cài đặt khác của cảm biến.
 */
esp_err_t as5600_config_slow_filter(as5600_t *sensor)
{
    uint8_t reg = AS5600_REG_CONF_H;
    uint8_t data[2];
    uint8_t data_trans[3];
    uint16_t conf;

    esp_err_t err = i2c_master_transmit_receive(
        sensor->dev,
        &reg, 1,
        data, 2,
        100
    );

    if(err != ESP_OK) return err;

    conf = ((data[0]<<8))|data[1];

    // set SF = 16x time 2.2ms, revolution 0.015
    // Đưa bit 8, 9 về 0
    conf &= ~(0x03 << 8);

    // set FTH = slow only
    // Đưa bit 10, 11, 12 về 0
    conf &= ~(0x07 << 10);

    // write back conf
    data_trans[0] = AS5600_REG_CONF_H;
    data_trans[1]= conf>>8;
    data_trans[2] = conf & 0xFF;

    err = i2c_master_transmit(
        sensor -> dev,
        data_trans,
        3,
        100
    );

    return err;
}

esp_err_t as5600_disable_filter(as5600_t *sensor)
{
    uint8_t reg = AS5600_REG_CONF_H;
    uint8_t data[2];
    uint8_t data_trans[3];
    uint16_t conf;

    esp_err_t err = i2c_master_transmit_receive(
        sensor -> dev,
        &reg, 1,
        data, 2,
        100
    );

    if(err != ESP_OK) return err;

    conf = ((data[0]<<8)) | data[1];

    // Clear
    conf &= ~(0x03 << 8);

    // Set SF = 2x time 0.268ms noise 0.043
    conf |= (0x03 << 8);

    data_trans[0] = AS5600_REG_CONF_H;
    data_trans[1] = conf>>8;
    data_trans[2] = conf & 0xFF;

    return i2c_master_transmit(
        sensor->dev,
        data_trans, 
        3, 
        100
    );

}

void i2c_master_init(i2c_master_bus_handle_t *bus_handle,
     i2c_master_dev_handle_t *dev_handle,
     i2c_port_t port,
     int sda, int scl)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,//chống nhiễu
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AS5600_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}   
/**
 * @brief Tính toán và tích lũy góc quay qua nhiều vòng quay
 * 
 */
void as5600_process_multi_turn(as5600_t *dev, 
                                as5600_logic_t *logic, 
                                float *display_angle, 
                                bool is_running) {
    uint16_t raw_int;
    if (as5600_read_angle(dev, &raw_int) != ESP_OK) return;

    float current_raw = raw_int * 360.0f / 4096.0f;
    logic->last_raw = raw_int; 
    if (is_running) {
        // Lấy điểm Zero khi bắt đầu chạy
        if (!logic->is_started) {
            logic->last_raw_degree = current_raw;
            logic->accumulated_angle = 0.0f;
            logic->is_started = true;
        }

        // Tính Delta (last - current để đảo chiều, giúp ANGLE dương giống Degree_cal)
        float delta = current_raw - logic->last_raw_degree;

        // Xử lý bù vòng khi qua điểm 0/360
        if (delta > 180.0f)  delta -= 360.0f;
        else if (delta < -180.0f) delta += 360.0f;

        logic->accumulated_angle += delta;
        logic->last_raw_degree = current_raw;
    } else {
        // Reset flag khi dừng để lần sau lấy offset mới
        logic->is_started = false;
        logic->accumulated_angle = 0.0f;
    }

    *display_angle = logic->accumulated_angle;
}
