# Biểu đồ Sequence và Phân tích tương tác giữa các Tác vụ (Tasks) - Đã thêm hộp kích hoạt (Activation Boxes)

Tài liệu này mô tả chi tiết biểu đồ sequence của hệ thống điều khiển bơm tiêm kép (**Dual Syringe Pump**). Bản cập nhật này bổ sung các hộp kích hoạt màu chữ nhật đứng (Activation Boxes) trên đường sống (lifeline) của mỗi tác vụ để biểu thị rõ thời điểm tác vụ thức dậy và xử lý công việc.

---

## 1. Cơ chế Điều phối Kênh của `pump_manager_task`

Theo thiết kế hệ thống và sửa đổi mới nhất:
1.  **Độc lập kiểm tra chế độ**: Tác vụ `pump_manager_task` (`MGR`) chịu trách nhiệm duy nhất trong việc kiểm tra chế độ hoạt động (`op_mode`) và điều phối trạng thái hệ thống. Tác vụ tính toán bước (`CALC`) chỉ thực hiện nhiệm vụ điều khiển động cơ của riêng nó và cập nhật trạng thái kênh của nó thành `PUMP_DONE` trong bộ nhớ chung.
2.  **Đọc và ghi bộ nhớ chung (Shared Memory)**: `MGR` tự động kiểm tra trạng thái của các kênh và chế độ hoạt động thông qua cấu trúc `system_state_t` (sử dụng `sys_state_mutex`). Nó tự cập nhật lại dữ liệu (ví dụ: chuyển đổi trạng thái hệ thống chạy/dừng) mà không cần giao tiếp trực tiếp với tác vụ tính toán.
3.  **Tương tác phần cứng**: Nếu chế độ hoạt động yêu cầu kích hoạt kênh tiếp theo (như chế độ chạy tuần tự `Sequential` chuyển từ Kênh 0 sang Kênh 1), `MGR` sẽ trực tiếp gửi lệnh khởi động timer tới phần cứng (`HW`).

---

## 2. Biểu đồ Sequence của Hệ thống

```mermaid
sequenceDiagram
    autonumber
    actor Host as PC
    
    %% Các đối tượng tham gia
    participant MAIN as app_main (main)
    participant UART_CMD as uart_cmd_task
    participant MGR as pump_manager_task
    participant SENSOR as sensor_task
    participant LOG as log_task
    participant CALC as pump_step_calc_task
    participant ISR as stepper_isr_callback (ISR)
    participant HW as Hardware (Motor/Encoder)

    %% KỊCH BẢN 0: KHỞI TẠO HỆ THỐNG
    Note over MAIN, HW: Giai đoạn 1: Khởi tạo hệ thống 
    activate MAIN
    MAIN->>MAIN: Khởi tạo hệ thống & Đăng ký các tác vụ 
    deactivate MAIN

    %% Các Task tự động chạy độc lập và tự đưa mình vào trạng thái Blocked để chờ sự kiện
    Note over MGR: Blocked chờ Task Notification hoặc timeout 100ms
    Note over SENSOR: Blocked chờ chu kỳ 20ms 
    Note over LOG: Blocked chờ Task Notification từ SENSOR
    Note over UART_CMD: Blocked chờ dữ liệu UART 

    %% KỊCH BẢN 1: KHỞI ĐỘNG VÀ THIẾT LẬP BƠM
    Note over Host, HW: Giai đoạn 2: Khởi chạy chu kỳ bơm (Chạy liên tiếp)
    
    Host->>UART_CMD: Gửi chuỗi JSON (cmd="START", mode="SEQ", flow, vol...) 
    activate UART_CMD
    Note over UART_CMD: Thức dậy do có dữ liệu
    UART_CMD->>UART_CMD: Ghi tham số cấu hình vào biến hệ thống
    Note over CALC: Thức dậy khi có Notify 
    UART_CMD->>HW: Kích hoạt GPTimer của Kênh 0 bắt đầu chạy 
    deactivate UART_CMD

    %% KỊCH BẢN 2: PHẢN HỒI KÍN (FEEDBACK LOOP & PID)
    Note over Host, HW: Giai đoạn 3: Vòng lặp phản hồi cảm biến và hiệu chỉnh tốc độ thực tế
    
    loop Mỗi chu kỳ 20ms (sensor_task)
        activate SENSOR
        SENSOR->>HW: Đọc góc xoay thô từ cảm biến
        HW-->>SENSOR: Phản hồi giá trị góc xoay thô 
        SENSOR->>SENSOR: Tính góc lũy kế & Ghi vào biến 
        opt Định kỳ mỗi 1.0 giây (50 chu kỳ)
            SENSOR->>SENSOR: Tính lưu lượng và thể tích ước tính
            SENSOR->>SENSOR: Ghi thống kê vào biến hệ thống
            opt Nếu dùng thuật toán TRAP+PID
                SENSOR->>SENSOR: Đưa trạng thái của cờ lên 1
            end
            SENSOR->>LOG: Gửi Task Notification đánh thức tác vụ
        end
        deactivate SENSOR
    end

    loop Đợi Task Notification từ SENSOR (log_task)
        activate LOG
        LOG->>LOG: Thức dậy khi nhận thông báo 
        LOG-->>Host: Ghi telemetry JSON ra cổng UART
        deactivate LOG
    end

    loop Mỗi khi động cơ quay (GPTimer ngắt phần cứng)
        HW->>ISR: GPTimer Alarm ngắt phần cứng
        activate ISR
        ISR->>HW: Phát xung STEP điều khiển TMC2209
        opt Tích lũy đủ 10 bước
            ISR->>CALC: Đánh thức tác vụ tính toán 
        end
        ISR->>HW: Cập nhật khoảng thời gian bước tiếp theo 
        deactivate ISR
        
        activate CALC
        CALC->>CALC: Nhận thông báo đánh thức & Tính toán khoảng thời gian bước tiếp theo
        
        opt Nếu thuật toán là TRAP+PID và có cờ pid update ready là 1
            CALC->>CALC: Đọc lưu lượng ước tính từ biến hệ thống & thực hiện tính toán PID
            CALC->>CALC: Reset cờ 
        end
        deactivate CALC
    end

    %% KỊCH BẢN 3: HOÀN THÀNH VÀ ĐIỀU PHỐI TUẦN TỰ
    Note over Host, HW: Giai đoạn 4: Đạt thể tích đích và chuyển kênh bơm (Chạy liên tiếp)
    
    CALC->>CALC: Phát hiện đạt thể tích đích (current_pos >= target_pos)
    activate CALC
    CALC->>HW: Dừng GPTimer phát xung Kênh 0 (gptimer_stop)
    CALC->>CALC: Ghi trạng thái Kênh 0 = PUMP_DONE vào biến hệ thống
    deactivate CALC

    loop Định kỳ (hoặc khi có thông báo hoàn thành kênh)
        activate MGR
        MGR->>MGR: Kiểm tra chế độ hoạt động & trạng thái các kênh trong biến hệ thống
        MGR->>MGR: Cập nhật trạng thái hệ thống/kênh
        opt Nếu ở chế độ chạy liên tiếp & Kênh 0 vừa hoàn thành
            MGR->>HW: Kích hoạt GPTimer cho Kênh 1 bắt đầu phát xung (gptimer_start)
        end
        deactivate MGR
    end
```
