# ESP32 TV Lift Controller

ESP32-based web interface for controlling the Vevor TV lift. The ESP32 connects to the lift controller via serial communication and provides a web interface for raising, lowering, and setting memory positions.

## Hardware Requirements

- **ESP32 Development Board** - Recommended: [ESP32 DevKit C](https://www.espressif.com/en/products/devkits/esp32-devkitc) or similar
- RJ45 connector and wires for lift connection
- Level shifter (if needed) - The lift operates at 5V, ESP32 uses 3.3V
- USB cable for programming

## ESP32 Board

This project is designed for ESP32 development boards such as:
- [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc)
- [NodeMCU-32S](https://www.nodemcu.com/index_en.html#fr_54747661d775ef1a3600019e)
- Any ESP32-WROOM-32 based development board

## Wiring Connections

### Proxy Setup (Default)

The ESP32 acts as a proxy between the wired remote and lift controller, allowing both the physical remote and web interface to control the lift:

1. **Connection 1: Remote → ESP32 (Serial1)**
   | ESP32 Pin | Remote RJ45 Pin | Description | Notes |
   |-----------|-----------------|-------------|-------|
   | GPIO 9 (RX) | Pin 2 (TxD) | Receive from remote (button commands) | Use level shifter (5V → 3.3V) |
   | GPIO 10 (TX) | Pin 4 (RxD) | Transmit to remote (status frames) | Use level shifter (3.3V → 5V) |
   | GND | Pin 3 (Ground) | Ground | Direct connection |
   | 5V | Pin 5 (5.0V) | Power remote | From lift controller or external supply |

2. **Connection 2: ESP32 → Lift (Serial2)**
   | ESP32 Pin | Lift RJ45 Pin | Description | Notes |
   |-----------|---------------|-------------|-------|
   | GPIO 16 (RX) | Pin 2 (TxD) | Receive from lift (status frames) | Use level shifter (5V → 3.3V) |
   | GPIO 17 (TX) | Pin 4 (RxD) | Transmit to lift (button commands) | Use level shifter (3.3V → 5V) |
   | GND | Pin 3 (Ground) | Ground | Direct connection |
   | 5V | Pin 5 (5.0V) | Power | Optional - lift may power itself |

**⚠️ Important:** The lift controller and remote operate at 5V logic levels, while ESP32 uses 3.3V. You should use bidirectional level shifters (such as a [Bidirectional Logic Level Converter](https://www.sparkfun.com/products/12009)) to protect your ESP32. However, many ESP32 boards can tolerate 5V inputs on GPIO pins - use at your own risk.

**How it works:**
- The ESP32 forwards status frames from lift → remote so the remote display shows correct position
- The ESP32 forwards button commands from remote → lift so the physical remote still works
- The web interface can inject commands directly to the lift (via Serial2)
- Both the physical remote and web interface can control the lift simultaneously

## Serial Communication

- **Baud Rate:** 9600
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1

## Software Setup

### Prerequisites

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (ESP32 IoT Development Framework)
   - Follow the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
   - Make sure to set up the environment (run `install.sh`/`install.bat` and `export.sh`/`export.bat`)

2. Update WiFi credentials in `main/main.c`:
   ```c
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   ```

3. Update pin assignments if needed (default: Lift RX=GPIO16, TX=GPIO17; Remote RX=GPIO9, TX=GPIO10)

### Build and Flash

1. Navigate to the `esp32` directory:
   ```bash
   cd esp32
   ```

2. Set the target (if not already set):
   ```bash
   idf.py set-target esp32
   ```

3. Configure the project (optional - uses defaults):
   ```bash
   idf.py menuconfig
   ```

4. Build the project:
   ```bash
   idf.py build
   ```

5. Flash the firmware to your ESP32:
   ```bash
   idf.py -p PORT flash
   ```
   Replace `PORT` with your serial port (e.g., `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-*` on macOS, `COM3` on Windows)

6. Monitor serial output:
   ```bash
   idf.py -p PORT monitor
   ```
   Or combine flash and monitor:
   ```bash
   idf.py -p PORT flash monitor
   ```

## Usage

1. After flashing, the ESP32 will connect to WiFi automatically

2. Check the serial monitor output for the WiFi IP address (should show: "WiFi connected! IP address: x.x.x.x")

3. Connect your device to the same WiFi network

4. Open a web browser and navigate to the IP address shown in Serial Monitor

4. Use the web interface to:
   - **Up/Down buttons:** Press and hold to move lift up/down, release to stop
   - **Position 1-4:** Move to stored memory positions

## Protocol Details

The ESP32 communicates with the lift using the serial protocol documented in the main README. Button commands are sent as 5-byte frames:

- Up Press: `55 AA E3 E3 E3`
- Up Release: `55 AA E1 E1 E1`
- Down Press: `55 AA E2 E2 E2`
- Down Release: `55 AA E3 E3 E3`
- Position 1-4: `55 AA D1-D7 D1-D7 D1-D7`

The lift continuously sends 6-byte status frames containing position information (see main README for decoding details).

## Troubleshooting

- **WiFi not connecting:** Check credentials in `main/main.c`, ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- **Build errors:** Make sure ESP-IDF is properly installed and environment is set up (run `export.sh`/`export.bat`)
- **No response from lift:** Check wiring, verify level shifter connections, confirm 9600 baud rate
- **Commands not working:** Check serial monitor for errors, verify pin assignments match your wiring
- **Port not found:** Use `idf.py -p PORT flash` and specify the correct serial port, or use `idf.py flash` to auto-detect

## Notes

- The ESP32 acts as a proxy, forwarding messages bidirectionally:
  - **Lift → Remote:** Status frames (position data) are forwarded so the remote display works
  - **Remote → Lift:** Button commands from the physical remote are forwarded to the lift
  - **Web Interface → Lift:** Commands from the web interface are sent directly to the lift
- Button commands must be sent repeatedly while held (the web interface handles this)
- The physical remote and web interface can both control the lift simultaneously
- All serial communication is also logged to Serial Monitor for debugging
