import json
import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

# --- CẤU HÌNH ---
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "uet/mechanics/data"
MAX_POINTS = 50  # Số điểm ảnh hiển thị trên đồ thị (độ dài trục X)

# Khởi tạo các hàng đợi (deque) để lưu trữ dữ liệu (tối đa 50 điểm)
# Giả sử ta vẽ 3 biến quan trọng nhất: v1, v2, v3
data_v1 = deque([0]*MAX_POINTS, maxlen=MAX_POINTS)
data_v2 = deque([0]*MAX_POINTS, maxlen=MAX_POINTS)
data_v3 = deque([0]*MAX_POINTS, maxlen=MAX_POINTS)
time_step = deque(list(range(MAX_POINTS)), maxlen=MAX_POINTS)

# --- XỬ LÝ MQTT ---
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    try:
        # Giải mã JSON từ ESP32
        payload = msg.payload.decode()
        json_data = json.loads(payload)
        
        # Lấy giá trị từ JSON (v1, v2, v3... phải khớp với key bên ESP32)
        v1 = json_data.get("pos", 0)
        v2 = json_data.get("vel", 0)
        v3 = json_data.get("acc", 0)

        # Đẩy dữ liệu vào hàng đợi
        data_v1.append(v1)
        data_v2.append(v2)
        data_v3.append(v3)
        
    except Exception as e:
        print(f"Error parsing JSON: {e}")

# --- THIẾT LẬP ĐỒ THỊ ---
fig, ax = plt.subplots()
line1, = ax.plot(time_step, data_v1, label='Variable 1 (pos)', color='r')
line2, = ax.plot(time_step, data_v2, label='Variable 2 (vel)', color='g')
line3, = ax.plot(time_step, data_v3, label='Variable 3 (acc)', color='b')

ax.set_ylim(-10, 110) # Điều chỉnh giới hạn trục Y tùy theo dữ liệu của bạn
ax.set_title("Real-time Data from ESP32 via MQTT")
ax.set_xlabel("Time Step")
ax.set_ylabel("Value")
ax.legend(loc='upper right')
ax.grid(True)

def update_plot(frame):
    """Hàm này được gọi liên tục để cập nhật đồ thị"""
    line1.set_ydata(data_v1)
    line2.set_ydata(data_v2)
    line3.set_ydata(data_v3)
    return line1, line2, line3,

# --- CHƯƠNG TRÌNH CHÍNH ---
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

print(f"Connecting to {BROKER}...")
client.connect(BROKER, PORT, 60)

# Chạy MQTT trong một luồng riêng để không làm treo giao diện đồ thị
client.loop_start()

# Bắt đầu hiệu ứng hoạt họa đồ thị
# interval=100 nghĩa là cập nhật đồ thị mỗi 100ms (nhanh hơn tốc độ gửi 500ms của ESP32)
ani = FuncAnimation(fig, update_plot, interval=100, blit=True)

try:
    plt.show()
except KeyboardInterrupt:
    print("Stopping...")
finally:
    client.loop_stop()
    client.disconnect()