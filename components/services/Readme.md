# Thành phần services - IO Boundary

## 1. Nhiệm vụ
Giao tiếp với hệ thống bên ngoài:
- MQTT
- UI
- Storange(NVS)

## 2. Phạm vi chức năng 
- Nhận input từ user/network
- Gửi output ra ngoài
- Gọi API từ app

## 3. Không bao gồm
- Logic điều khiển
- Truy cập trực tiếp driver
- Xử lý real-time(có thể)

## 4. Đặc điểm thiết kế
- Event - driver
- Chạy trong task riêng