# Thành phần drivers - Hardware Abstraction Layer(HAL)


## 1. Nhiệm vụ 
Wrap toàn bộ phần cứng: 
- Driver TMC2209
- Encoder AS5600
- Button
- TFT display
- RMT/GPIO/SPI/I2C

Cung cấp API mức thấp phục vụ giao tiếp (đọc/ghi) với tầng Hardware.

## 2. Phạm vi chức năng
- Điều khiển chân GPIO
- Gửi/nhận dữ liệu qua bus (I2C, SPI, UART)

## 3. Không bao gồm
- Thuật toán điều khiển (PID, motion profile)
- Logic điều khiển
- Chuyển đổi đơn vị 
- Trạng thái của hệ thống
- Các giao tiếp ngoài (mqtt/wifi)

## Đặc điểm thiết kế
- Stateless hoặc minimal state
- có thể chạy trong ISR(IRAM-safe nếu cần)
- API đơn giản
