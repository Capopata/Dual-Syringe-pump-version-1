@echo off
setlocal enabledelayedexpansion

set PORT=%1
set PROJECT_DIR=%2
set BUILD_DIR=%2\build
set LOGFILE=%~dp0flash_log.txt

echo. > "%LOGFILE%"
echo [INFO] Port: %PORT% >> "%LOGFILE%"
echo [INFO] Build dir: %BUILD_DIR% >> "%LOGFILE%"

set ESPTOOL=D:\pc\ESP-IDF\Env\Espressif\python_env\idf5.5_py3.11_env\Scripts\esptool.exe

:: ── Build Firmware ──
echo [INFO] Dang bien dich (build) code moi nhat... >> "%LOGFILE%"
echo [INFO] Vui long doi vai giay den vai phut... >> "%LOGFILE%"

call idf.py build >> "%LOGFILE%" 2>&1

if errorlevel 1 (
    echo [ERROR] Build that bai! >> "%LOGFILE%"
    type "%LOGFILE%" & exit /b 1
)
echo [INFO] Build thanh cong >> "%LOGFILE%"

:: ── Kiểm tra file ──
echo [DEBUG] Checking: "%BUILD_DIR%\bootloader\bootloader.bin" >> "%LOGFILE%"

if not exist "%BUILD_DIR%\bootloader\bootloader.bin" (
    echo [ERROR] Khong tim thay bootloader.bin >> "%LOGFILE%"
    type "%LOGFILE%" & exit /b 1
)
if not exist "%BUILD_DIR%\partition_table\partition-table.bin" (
    echo [ERROR] Khong tim thay partition-table.bin >> "%LOGFILE%"
    type "%LOGFILE%" & exit /b 1
)
if not exist "%BUILD_DIR%\Syringe_pump.bin" (
    echo [ERROR] Khong tim thay Syringe_pump.bin >> "%LOGFILE%"
    type "%LOGFILE%" & exit /b 1
)

echo [INFO] Tat ca file bin OK >> "%LOGFILE%"
echo [INFO] Dang nap code... >> "%LOGFILE%"

:: ── Flash ──
"%ESPTOOL%" --chip esp32 --port %PORT% --baud 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 "%BUILD_DIR%\bootloader\bootloader.bin" 0x8000 "%BUILD_DIR%\partition_table\partition-table.bin" 0x10000 "%BUILD_DIR%\Syringe_pump.bin" >> "%LOGFILE%" 2>&1

if errorlevel 1 (
    echo [ERROR] esptool that bai >> "%LOGFILE%"
    type "%LOGFILE%" & exit /b 1
)

echo [SUCCESS] Nap code thanh cong! >> "%LOGFILE%"
type "%LOGFILE%"
:: ── Reset thêm lần nữa (tùy chọn) ──
"%ESPTOOL%" --chip esp32 --port %PORT% --after hard_reset read_mac >> "%LOGFILE%" 2>&1
echo [INFO] Da reset ESP32 >> "%LOGFILE%"
exit /b 0