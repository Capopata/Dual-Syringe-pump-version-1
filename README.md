# Firmware Vi Bơm Đa Kênh (Dual Syringe Pump) ESP-IDF

Kho lưu trữ này chứa mã nguồn firmware dựa trên framework ESP-IDF cho hệ thống **Vi Bơm Đa Kênh** độ chính xác cao. Hệ thống điều khiển hai kênh bơm tiêm hoạt động độc lập hoặc phối hợp, được truyền động bằng động cơ bước kết hợp điều khiển vòng kín PID thông qua cảm biến góc quay từ trường và điều khiển bằng tập lệnh thông qua cổng nối tiếp UART.

---

## Mục lục
1. [Các tính năng chính](#1-các-tính-năng-chính)
2. [Kiến trúc phần cứng & Sơ đồ chân](#2-kiến-trúc-phần-cứng--sơ-đồ-chân)
3. [Cấu trúc phần mềm](#3-cấu-trúc-phần-mềm)
4. [Quy đổi đơn vị](#4-quy-đổi-đơn-vị)
5. [Giao thức truyền thông UART](#5-giao-thức-truyền-thông-uart)

---

## 1. Các tính năng chính

- **Điều khiển 2 kênh**: Điều khiển độc lập hoặc phối hợp nhịp nhàng giữa hai kênh bơm tiêm (`Kênh 0` & `Kênh 1`).
- **Điều khiển vòng kín**: Kết hợp giữa thuật toán sinh quỹ đạo chuyển động **Ramp hình thang** (chạy vòng hở) với bộ điều khiển phản hồi vòng kín **PID** sử dụng **Cảm biến góc từ trường AS5600** để tự động hiệu chỉnh sai số bước cơ khí theo thời gian thực.
- **Chế độ hoạt động đa dạng**:
  - `Independent` (Độc lập): Các kênh chạy độc lập với thể tích mục tiêu và tốc độ lưu lượng được cài đặt riêng biệt.
  - `Simultaneous` (Đồng thời): Cả hai kênh bắt đầu và dừng lại cùng lúc; lỗi ở một kênh sẽ dừng cả hai kênh để đảm bảo an toàn.
  - `Sequential` (Liên tiếp): Truyền dịch liên tục, Kênh 1 tự động chạy ngay sau khi Kênh 0 hoàn thành nhiệm vụ (`PUMP_DONE`).
  - `Homing` (Về gốc): Quy trình chạy tìm giới hạn điểm gốc vật lý để hiệu chuẩn hành trình cơ khí.

---

## 2. Kiến trúc phần cứng & Sơ đồ chân

Hệ thống phần cứng được phát triển dựa trên vi điều khiển **ESP32**. Sơ đồ chân của các thiết bị ngoại vi được cấu hình trong tệp [io_config.h](file:///d:/pc/Documents/Micropump/Newversion/idf/Dual_Syringe_pump_idf/components/drivers/io_config.h):

### Driver động cơ bước (TMC2209)
Mỗi kênh được điều khiển bằng một IC Driver TMC2209 thông qua các chân Step/Dir và được cấu hình dòng điện, vi bước qua bus UART dùng chung.
- **Sơ đồ chân động cơ Kênh 0**:
  - `STEP`: GPIO 25
  - `DIR`: GPIO 2
  - `EN`: GPIO 27
- **Sơ đồ chân động cơ Kênh 1**:
  - `STEP`: GPIO 14
  - `DIR`: GPIO 13
  - `EN`: GPIO 17
- **Giao tiếp UART dùng chung (TMC2209 Communication)**:
  - `UART Port`: UART1
  - `TX Pin`: GPIO 15
  - `RX Pin`: GPIO 26
  - Địa chỉ Node ID: Kênh 0 là Node `0`, Kênh 1 là Node `1`

### Cảm biến góc quay từ trường (AS5600)
Cảm biến AS5600 đọc góc quay của trục động cơ hoặc trục vít me để giám sát và phản hồi tốc độ thực tế của pít-tông.
- **AS5600 Kênh 0**: Kết nối bus I2C 0 (`SDA` = GPIO 21, `SCL` = GPIO 22)
- **AS5600 Kênh 1**: Kết nối bus I2C 1 (`SDA` = GPIO 32, `SCL` = GPIO 33)

### Cấu hình cổng UART máy chủ (Host UART)
- **UART Port**: UART0 (Cổng Debug mặc định và nhận lệnh điều khiển)
- **Baudrate**: 115200
- **TX/RX**: Chân mặc định của ESP32 (GPIO 1 / GPIO 3)

---

## 3. Cấu trúc phần mềm

Chương trình được phát triển trên nền tảng framework **ESP-IDF v5.x**:

- **`main/main.c`**: Điểm khởi chạy chương trình. Khởi tạo trạng thái ban đầu của hệ thống và bắt đầu task quản lý bơm trung tâm.
- **`components/domain`**:
  - `domain.c/h`: Định nghĩa các cấu trúc dữ liệu chung (`system_state_t`) và hàm truy cập trạng thái hệ thống toàn cục.
- **`components/app`**:
  - `pump_channel`: Điều khiển phát xung động cơ bước cấp thấp và cấu hình timer.
  - `pump_manager`: Điều phối logic chuyển trạng thái giữa các chế độ bơm và phân tích cú pháp (parsing) tập lệnh nhận từ UART.
  - `unit_converter`: Tập hợp các công thức quy đổi cơ học vật lý và đơn vị đo.
- **`components/drivers`**:
  - `as5600`: Trình điều khiển cảm biến góc từ trường đọc qua I2C và xử lý quay nhiều vòng.
  - `tmc2209`: Trình cấu hình thanh ghi và tham số chạy của IC driver động cơ bước qua UART.
- **`components/motion`**:
  - `trapezoidal_profile`: Sinh quỹ đạo vận tốc hình thang cho động cơ.
  - `PID`: Bộ điều khiển bù sai lệch của chu kỳ xung động cơ hiện tại so với setpoint.

---

## 4. Quy đổi đơn vị

Tất cả các hằng số quy đổi đơn vị được định nghĩa trong tệp [unit_converter.h](file:///d:/pc/Documents/Micropump/Newversion/idf/Dual_Syringe_pump_idf/components/app/unit_converter/unit_converter.h).

### Quy đổi từ Thể tích sang Số bước xung
Các hằng số hiệu chuẩn trong firmware được cấu hình tối ưu cho **Xi-lanh dung tích 1 mL** có đường kính trong là **4.6 mm** (Diện tích tiết diện = `16.619025 mm²`).
- **Chế độ vi bước (Microstepping)**: 256 vi bước/bước (tương đương `51200` xung cho 1 vòng quay $360^\circ$).
- **Công thức quy đổi Thể tích - Xung**:
  $$\text{Số bước xung trên 1 mL} = 7701955.0$$
  $$\text{Dung tích trên mỗi bước xung} \approx 0.000000129836 \text{ mL}$$
  $$\text{Số bước xung trên mỗi tick Encoder} = 12.5$$
- **Tốc độ di chuyển cơ khí sang Tần số phát xung**:
  $$\text{Tần số (Hz)} = \text{Vận tốc (mm/s)} \times 128000.0$$

---

## 5. Giao thức truyền thông UART

ESP32 giao tiếp với máy tính hoặc HMI thông qua cổng UART0 bằng các gói dữ liệu dạng chuỗi JSON kết thúc bằng ký tự xuống dòng `\n`.

### 1. Tập lệnh điều khiển (Gửi từ Máy chủ tới ESP32)

- **Chạy chế độ Độc lập (`INDEP`):**
  ```json
  {"cmd": "START", "mode": "INDEP", "ch": 0, "flow": 0.5, "vol": 0.2}
  ```
- **Chạy chế độ Đồng thời (`SIMUL`):**
  ```json
  {"cmd": "START", "mode": "SIMUL", "flow": 1.0, "vol": 0.5}
  ```
- **Chạy chế độ Liên tiếp (`SEQ`):**
  ```json
  {"cmd": "START", "mode": "SEQ", "ch0_flow": 0.5, "ch0_vol": 0.2, "ch1_flow": 1.0, "ch1_vol": 0.5}
  ```
- **Dừng bơm (Stop):**
  - Dừng riêng một kênh:
    ```json
    {"cmd": "STOP", "ch": 0}
    ```
  - Dừng khẩn cấp toàn bộ các kênh ngay lập tức:
    ```json
    {"cmd": "STOP"}
    ```
- **Chạy về gốc (Homing):**
  - Về gốc cho một kênh riêng biệt:
    ```json
    {"cmd": "HOME", "ch": 1}
    ```
  - Về gốc cho cả hai kênh:
    ```json
    {"cmd": "HOME"}
    ```

### 2. Gói tin giám sát (ESP32 gửi lên Máy chủ)
Định kỳ mỗi 1 giây, ESP32 sẽ tự động gửi gói tin JSON cập nhật thông số chi tiết của từng kênh bơm đang hoạt động:
```json
{"ch":0,"algo":"TRAP+PID","vol_infused":0.05214,"vol_target":0.20000,"flow_measure":0.49812,"flow_setpoint":0.50000,"time_run":12.0,"state":1,"steps":401524,"kp":1.20,"ki":0.05,"kd":0.10}
```

