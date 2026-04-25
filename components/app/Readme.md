# Thành phần app - business logic

## 1. Nhiệm vụ
Xử lý logic nghiệp vụ:
- Điều khiển máy bơm theo đơn vị (mL/h)
- Quản lý nhiều channel
- Chuyển đổi đơn vị

## 2. Thành phần con
- pump_channel: logic điều khiển cho 1 kênh
- pump_manager: quản lý nhiều kên
- unit_converter: Chuyển đổi đơn vị

## 3. Phạm vi chức năng
- Nhận yêu cầu từ services
- Cập nhật target cho control
- Áp dụng logic sản phẩm

## 4. Không bao gồm 
- Điều khiển trực tiếp hardware
- thuật toán PID
- ISR/Real-time loop