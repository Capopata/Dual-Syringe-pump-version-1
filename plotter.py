"""
pump_monitor.py
---------------
- Tự động flash firmware ESP32 (esptool)
- Đọc và parse JSON từ UART log_task
- In bảng dữ liệu real-time ra terminal

Cài thư viện cần thiết:
    pip install pyserial esptool rich
"""

import serial
import serial.tools.list_ports
import json
import argparse
import subprocess
import sys
import time
import threading
from datetime import datetime
from pathlib import Path

try:
    from rich.console import Console
    from rich.table import Table
    from rich.live import Live
    from rich.panel import Panel
    from rich.text import Text
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False

# ─── Cấu hình mặc định ───────────────────────────────────────────────
DEFAULT_PORT    = None          # None = tự dò cổng COM
DEFAULT_BAUD    = 115200
MAX_CHANNELS    = 2
CSV_LOG         = True          # Ghi file CSV song song
# ─────────────────────────────────────────────────────────────────────

console = Console() if RICH_AVAILABLE else None


# ══════════════════════════════════════════════════════
#  1. AUTO-DETECT PORT
# ══════════════════════════════════════════════════════
def find_esp32_port() -> str | None:
    """Tìm cổng COM của ESP32 (CP210x / CH340 / FTDI)."""
    KNOWN_VID_PID = [
        (0x10C4, 0xEA60),  # CP2102 / CP2104
        (0x1A86, 0x7523),  # CH340
        (0x0403, 0x6001),  # FTDI FT232
        (0x0403, 0x6010),  # FTDI FT2232
        (0x1A86, 0x55D4),  # CH9102
    ]
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        for vid, pid in KNOWN_VID_PID:
            if p.vid == vid and p.pid == pid:
                print(f"[AUTO] Phát hiện ESP32 tại {p.device}  ({p.description})")
                return p.device
    if len(ports) == 1:
        print(f"[AUTO] Dùng port duy nhất: {ports[0].device}")
        return ports[0].device
    return None


# ══════════════════════════════════════════════════════
#  2. FLASH FIRMWARE (GỌI FILE BAT)
# ══════════════════════════════════════════════════════
def flash_firmware(port: str, build_dir: str) -> bool:
    bat_file = Path(__file__).parent / "flash_esp32.bat"

    if not bat_file.exists():
        print(f"[FLASH] ❌ Không tìm thấy file {bat_file.name} tại {bat_file}")
        return False

    abs_build_dir = str(Path(build_dir).resolve())
    cmd = [str(bat_file), port, abs_build_dir]
    print(f"\n[FLASH] Đang gọi kịch bản nạp code: {bat_file.name}")
    print(f"[FLASH] Port: {port} | Build Dir: {abs_build_dir}\n")

    try:
        proc = subprocess.run(cmd, timeout=120)
        if proc.returncode == 0:
            print("[FLASH] ✅ Nạp code bằng file .bat thành công!")
            return True
        else:
            print(f"[FLASH] ❌ Tiến trình nạp bị lỗi (Exit code: {proc.returncode})")
            return False
    except FileNotFoundError:
        print("[FLASH] ❌ Không thể thực thi file .bat.")
        return False
    except subprocess.TimeoutExpired:
        print("[FLASH] ❌ Quá thời gian chờ (120s) khi chạy file nạp.")
        return False


# ══════════════════════════════════════════════════════
#  3. DATA MODEL
# ══════════════════════════════════════════════════════
class ChannelData:
    def __init__(self, ch: int):
        self.ch            = ch
        self.algo          = "—"
        self.vol_infused   = 0.0
        self.vol_target    = 0.0
        self.flow_measure  = 0.0
        self.flow_setpoint = 0.0
        self.time_run      = 0.0
        self.state         = 0
        self.steps         = 0
        self.kp            = 0.0
        self.ki            = 0.0
        self.kd            = 0.0
        self.updated_at    = None

    def update(self, obj: dict):
        self.algo          = obj.get("algo",          self.algo)
        self.vol_infused   = obj.get("vol_infused",   self.vol_infused)
        self.vol_target    = obj.get("vol_target",    self.vol_target)
        # firmware key: "flow_measure" = flow_actual
        self.flow_measure  = obj.get("flow_measure",  self.flow_measure)
        self.flow_setpoint = obj.get("flow_setpoint", self.flow_setpoint)
        self.time_run      = obj.get("time_run",      self.time_run)
        self.state         = obj.get("state",         self.state)
        self.steps         = obj.get("steps",         self.steps)
        self.kp            = obj.get("kp",            self.kp)
        self.ki            = obj.get("ki",            self.ki)
        self.kd            = obj.get("kd",            self.kd)
        self.updated_at    = datetime.now()

    @property
    def state_str(self) -> str:
        return {0: "IDLE", 1: "RUN", 2: "DONE", 3: "HOMING"}.get(self.state, str(self.state))

    @property
    def progress_pct(self) -> float:
        if self.vol_target <= 0:
            return 0.0
        return min(100.0, self.vol_infused / self.vol_target * 100.0)


# ══════════════════════════════════════════════════════
#  4. READER THREAD
# ══════════════════════════════════════════════════════
class UartReader:
    def __init__(self, port: str, baud: int, channels: list[ChannelData],
                 csv_path: str | None = None):
        self.port        = port
        self.baud        = baud
        self.channels    = channels
        self.running     = False
        self._ser        = None
        self._thread     = None
        self.line_count  = 0
        self.error_count = 0

        self._csv_file = None
        self._csv_path = csv_path
        if csv_path:
            self._csv_file = open(csv_path, "w", encoding="utf-8")
            self._csv_file.write(
                "timestamp,ch,algo,vol_infused,vol_target,"
                "flow_measure,flow_setpoint,time_run,state,steps,"
                "kp,ki,kd\n"
            )

    def start(self):
        self._ser = serial.Serial()
        self._ser.port     = self.port
        self._ser.baudrate = self.baud
        self._ser.bytesize = serial.EIGHTBITS
        self._ser.parity   = serial.PARITY_NONE
        self._ser.stopbits = serial.STOPBITS_ONE
        self._ser.timeout  = 1
        self._ser.dtr      = False
        self._ser.rts      = False
        self._ser.open()
        time.sleep(0.1)
        self._ser.setDTR(False)
        self._ser.setRTS(False)

        self.running  = True
        self._thread  = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self.running = False
        if self._thread:
            self._thread.join(timeout=2)
        if self._ser and self._ser.is_open:
            self._ser.close()
        if self._csv_file:
            self._csv_file.flush()
            self._csv_file.close()

    def send(self, data: bytes):
        if self._ser and self._ser.is_open:
            self._ser.write(data)

    def _run(self):
        buf = b""
        while self.running:
            try:
                chunk = self._ser.read(256)
                if not chunk:
                    continue
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    self._parse(line.strip())
            except serial.SerialException as e:
                print(f"\n[UART] Lỗi serial: {e}")
                self.running = False

    def _parse(self, raw: bytes):
        if not raw:
            return
        text = raw.decode("utf-8", errors="replace").strip()
        if not text.startswith('{'):
            return
        try:
            obj = json.loads(text)
        except json.JSONDecodeError:
            self.error_count += 1
            return

        ch_id = obj.get("ch")
        if ch_id is None or not (0 <= ch_id < len(self.channels)):
            return

        self.channels[ch_id].update(obj)
        self.line_count += 1

        if self._csv_file:
            ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            c  = self.channels[ch_id]
            self._csv_file.write(
                f"{ts},{c.ch},{c.algo},{c.vol_infused:.5f},{c.vol_target:.5f},"
                f"{c.flow_measure:.5f},{c.flow_setpoint:.5f},{c.time_run:.1f},"
                f"{c.state},{c.steps},"
                f"{c.kp:.2f},{c.ki:.2f},{c.kd:.2f}\n"
            )


# ══════════════════════════════════════════════════════
#  5. RICH TABLE UI
# ══════════════════════════════════════════════════════
def make_progress_bar(pct: float, bar_len: int = 20) -> Text:
    """Tạo progress bar có màu theo tiến độ."""
    filled = round(pct / 100.0 * bar_len)
    filled = max(0, min(bar_len, filled))
    bar_str = "█" * filled + "░" * (bar_len - filled)
    label   = f" {pct:5.1f}%"

    if pct < 33:
        style = "red"
    elif pct < 66:
        style = "yellow"
    else:
        style = "green"

    return Text(bar_str + label, style=style)


def make_table(channels: list[ChannelData], packets: int, errors: int) -> Panel:
    t = Table(title="🩺 Syringe Pump Monitor", border_style="bright_blue",
              show_lines=True, expand=True)

    t.add_column("CH",              justify="center", style="bold cyan", width=4)
    t.add_column("State",           justify="center",                    width=7)
    t.add_column("Algo",            justify="center", style="dim",       width=10)
    t.add_column("Flow SP\n(mL/h)", justify="right",                    width=12)
    t.add_column("Flow Act\n(mL/h)",justify="right",                    width=12)
    t.add_column("Vol Inf\n(mL)",   justify="right",                    width=12)
    t.add_column("Vol Tgt\n(mL)",   justify="right",                    width=12)
    t.add_column("Progress",        justify="left",                      width=28)
    t.add_column("Time\n(s)",       justify="right",                     width=8)
    t.add_column("Steps",           justify="right", style="dim",        width=10)

    STATE_COLOR = {0: "dim", 1: "green bold", 2: "yellow bold", 3: "magenta"}

    for c in channels:
        state_style = STATE_COLOR.get(c.state, "white")
        pct         = c.progress_pct

        t.add_row(
            str(c.ch),
            Text(c.state_str, style=state_style),
            c.algo,
            f"{c.flow_setpoint:.5f}",
            f"{c.flow_measure:.5f}",
            f"{c.vol_infused:.5f}",
            f"{c.vol_target:.5f}",
            make_progress_bar(pct),
            f"{c.time_run:.1f}",
            str(c.steps),
        )

    footer = (f"  Packets: {packets}   Parse errors: {errors}"
              f"   {datetime.now().strftime('%H:%M:%S')}")
    return Panel(t, subtitle=footer)


def run_rich_ui(reader: UartReader, channels: list[ChannelData]):
    with Live(make_table(channels, 0, 0), refresh_per_second=2,
              console=console, screen=False) as live:
        try:
            while reader.running:
                live.update(make_table(channels, reader.line_count, reader.error_count))
                time.sleep(0.5)
        except KeyboardInterrupt:
            pass


def run_plain_ui(reader: UartReader, channels: list[ChannelData]):
    header = ("CH | State  | Algo      | FlowSP      | FlowAct"
              "     | VolInf      | VolTgt      | Prog%  | Time |  Steps")
    sep    = "-" * len(header)
    try:
        while reader.running:
            print("\033[2J\033[H")
            print(sep)
            print(header)
            print(sep)
            for c in channels:
                print(
                    f"{c.ch:2d} | {c.state_str:<6} | {c.algo:<9} | "
                    f"{c.flow_setpoint:11.5f} | {c.flow_measure:11.5f} | "
                    f"{c.vol_infused:11.5f} | {c.vol_target:11.5f} | "
                    f"{c.progress_pct:5.1f}% | {c.time_run:4.0f} | {c.steps:8d}"
                )
            print(sep)
            print(f"Packets: {reader.line_count}  Errors: {reader.error_count}"
                  f"  [{datetime.now().strftime('%H:%M:%S')}]  Ctrl+C to quit")
            time.sleep(1)
    except KeyboardInterrupt:
        pass


# ══════════════════════════════════════════════════════
#  6. MAIN
# ══════════════════════════════════════════════════════
def main():
    parser = argparse.ArgumentParser(description="ESP32 Syringe Pump Monitor")
    parser.add_argument("--port",  "-p", default=None,
                        help="Cổng COM (VD: COM3 hoặc /dev/ttyUSB0). Mặc định: tự dò.")
    parser.add_argument("--baud",  "-b", default=DEFAULT_BAUD, type=int,
                        help=f"Baudrate (mặc định: {DEFAULT_BAUD})")
    parser.add_argument("--flash", "-f", default=None, metavar="BUILD_DIR",
                        help="Đường dẫn thư mục build IDF. Nếu truyền, sẽ flash trước.")
    parser.add_argument("--no-csv", action="store_true",
                        help="Không ghi file CSV log")
    args = parser.parse_args()

    # ── 1. Tìm port ──
    port = args.port or find_esp32_port()
    if not port:
        print("[LỖI] Không tìm thấy cổng COM. Dùng --port COM3 để chỉ định thủ công.")
        sys.exit(1)

    # ── 2. Flash nếu có build_dir ──
    if args.flash:
        print(f"\n[FLASH] Đang nạp firmware từ: {args.flash}")
        ok = flash_firmware(port, args.flash)
        if not ok:
            print("[FLASH] Nạp thất bại. Tiếp tục đọc UART không flash.")
        else:
            print("[FLASH] Chờ ESP32 khởi động lại...")
            time.sleep(5)

    # ── 3. Khởi tạo data model ──
    channels = [ChannelData(i) for i in range(MAX_CHANNELS)]

    # ── 4. Tạo CSV path ──
    csv_path = None
    if CSV_LOG and not args.no_csv:
        log_dir = Path("Log")
        log_dir.mkdir(parents=True, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        csv_path = str(log_dir / f"pump_log_{ts}.csv")
        print(f"[LOG] Ghi CSV → {csv_path}")

    # ── 5. Khởi động reader ──
    print(f"[UART] Kết nối {port} @ {args.baud} baud...\n")
    reader = UartReader(port, args.baud, channels, csv_path)
    try:
        reader.start()
        time.sleep(0.5) 
    except serial.SerialException as e:
        print(f"[LỖI] Không mở được cổng serial: {e}")
        sys.exit(1)

    print("Đang đọc dữ liệu... Nhấn Ctrl+C để thoát.\n")

    # ── 6. UI ──
    if RICH_AVAILABLE:
        run_rich_ui(reader, channels)
    else:
        print("[INFO] Cài 'rich' để có giao diện đẹp hơn: pip install rich")
        run_plain_ui(reader, channels)

    reader.stop()
    print(f"\n[DONE] Tổng packets: {reader.line_count}  Lỗi parse: {reader.error_count}")
    if csv_path:
        print(f"[DONE] Log đã lưu tại: {csv_path}")


if __name__ == "__main__":
    main()