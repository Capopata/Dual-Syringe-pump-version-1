#include "as5600.h"

as5600_t as5600;

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
    conf &= ~(0x03 << 8);

    // set FTH = slow only
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

void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
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

void read_angle_task(void *arg){
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;

    i2c_master_init(&bus, &dev);

    as5600_init(&as5600, dev);

    as5600_config_slow_filter(&as5600);
    as5600_disable_filter(&as5600);
    while(1){
        uint16_t angle;
        uint16_t raw;
        uint8_t status;
        float angle_degree;

        as5600_read_raw_angle(&as5600, &raw);
        as5600_read_angle(&as5600, &angle);
        as5600_read_status(&as5600, &status);
        
        angle_degree = angle*360.0f/4096.0f;

        printf("RAW=%u  ANGLE=%u  STATUS=0x%02X ANGLE(degree)=%.3f\n",
               raw, angle, status, angle_degree);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}