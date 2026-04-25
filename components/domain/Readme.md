# Thành phần domain - Data model

## 1.Nhiệm vụ
Lưu trữ trạng thái của hệ thống:
- Trạng thái channel
- Giá trị target, current

## 2. Phạm vi chức năng
- Định nghĩa struct dữ liệu
- Là nguồn dữ liệu trung tâm

## 3. Không bao gồm 
- Logic điều khiển
- Gọi driver
- Thuật toán

## 4. Đặc điểm thiết kế
- Plain data (POD)
- Không side-effect
- Dễ serialize (Lưu NVS/gửi MQTT/hiển thị ra màn hình)