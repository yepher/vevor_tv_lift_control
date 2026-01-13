# Vevor TV Lift Control

_(click image to see short YouTube video of lift in action)_
[![Vevor TV Lift](https://img.youtube.com/vi/0HHHwE2EETI/0.jpg)](https://www.youtube.com/watch?v=0HHHwE2EETI)



Here is example of it connected to [LiveKit](https://livekit.io) for voice control:

_(click image to see short YouTube video of agent controlling a TV lift)_  
[![Hide/Unhide TV](https://img.youtube.com/vi/mcz0MOzswV0/0.jpg)](https://youtu.be/mcz0MOzswV0)


**Progress**

- [x] Understand Wireless Remote Communications
- [x] Understand Wired Remote Communications
- [x] Design alternate solution to control lift (ESP32 implementation)
- [ ] Integrate with Home Assistant and control from HA
    - See comment from [Ginxo](https://github.com/yepher/vevor_tv_lift_control/issues/1#issuecomment-3570308709) for HomeAssistant Integration
- [ ] Automate raise/lower of TV based on TV state


I have a Vevor TV lift which is working great. But I have a tendency to turn off the TV and not lower it back down. I really want to have it automatically retract if the TV has been off for more than 10 minutes.

I asked Vevor if there were a way to integrate this with one of the home automation solutions (like Home Assistant). They responded with:

```
Hi buyer， Thank you for your time to the mail. 

Sorry, there is no way to be controlled by the automation system yet. 

The TV stand can only be controlled by the remote control. 

Hope this helps. Best wishes, 

Jennifer Customer Support
```

So..... I guess I have to figure this out on my own. The rest of this document are my notes about the journey to automating the TV lift.


![Vevor TV Lift Image](./images/vevor_ift_product_image.jpg)

* [Vevor TV lift](https://www.amazon.com/gp/product/B08B1M3L1W/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&th=1) on Amazon

## Wireless Remote

There is wireless remote that comes with the unit. After taking apart and looking inside I was able to determine it uses RF (`RLink`) to control the lift. The signal looks like a 433 MHz AM signal.

**Note:** RF cloning (e.g., with Broadlink RM Pro) works well for basic up/down control, but **memory position functions (1-4) do not work via RF cloning** - the lift will only raise to maximum or lower to minimum height. To use memory positions, you must use the wired remote or interface directly with the lift controller via serial communication (see [ESP32 implementation](./esp32/README.md) for an example).

This is what the wireless remote looks like

![Wireless Remote](./images/wireless_remote_000.JPG)

This is what the circuit board looks like

![Wireless Remote Circuit Board](./images/wirelessRemote_001.JPG)


**Onboard key components**

* [EV1527](./docs/sunrom-206000.pdf)


## Wired Control

There is a "wired remote" that has a series of buttons to control the lift. It also has a numeric reading that indicates the lift height in centimeters. The displayed value represents the height from the top of the lift platform to the base. For mine all the way down is `73.0` cm (28.74") and all the way up is `173` cm (68.11"). The "Wired Remote" connects to the lift controller with an RJ45 jack.


The numbered keys are memory positions and the arrow buttons moves the lift up and down.

The numeric display turns off after approximately 20 seconds. Press any key to turn display back on. Have to push the button again to perform the action.

This is what the wired remote looks like:

![Wired Remote](images/wiredRemote_000.jpg)


When you take the wired remote apart and look inside this is what you see (with some notes added to the image)

![Wired Remote Front Circuit Board](./images/wiredRemote_002_2.JPG)

And this is the back of the circuit board

![Wired Remote Back Circuit Board](./images/wiredRemote_003.JPG)

The RJ45 pin out is:

![RJ45 Pins](./images/RJ45.png)

| RJ45<br>Pin # | Board Connector<br>Pin # | Wire<br>Color | Description |
|---|---|---|---|
|1 | 2 | Red | `RxD_3` |
|2 | 3 | Yellow | `TxD` |
|3 | 6 | White | `Ground` |
|4 | 4 | Green | `RxD` |
|5 | 5 | Black | `5.0V`|
|6 | - | - | Not connected |
|7 | - | - | Not connected |
|8 | 1 | Brown | `TxD_3` |

**Serial Communication:**
- Baud rate: 9600
- Use pins 2 (`TxD`) and 4 (`RxD`) for communication with the lift controller
- Ground on pin 3, +5V on pin 5

I tried to search for `XK7BH` but did not find anything helpful.


**Onboard key components**

- micro-controller [STC 8H1K16](https://www.stcmicro.com/datasheet/STC8F-en.pdf)
- Voltage Regulator [AMS 1117](http://www.advanced-monolithic.com/pdf/ds1117.pdf) `5.0v`->`3.3v`
* My "dumb" question about decoding data stream [Electric Overflow](https://electronics.stackexchange.com/questions/599134/how-decode-serial-data-stream?noredirect=1#comment1574777_599134) I was so fixated on the physical wiring I overlooked the obvious I did not finish writing the code I needed to actually see the data. I was just printing the Len (DOH!!!)

### Encoder

**Raw Encoder Samples**

* Going Up [`encoder_raise.txt`](./raw_serial_captures/encoder_raise.txt)
* Going Down [`encoder_lower.txt`](./raw_serial_captures/encoder_lower.txt)

## Serial Data

### Protocol Summary

Communication between the lift controller and wired remote uses a serial protocol over RJ45:

- **Lift → Remote**: Continuous 6-byte status frames containing position data
- **Remote → Lift**: 2-byte or 5-byte command frames for button presses and display control

See [`encoder_at_bottom.txt`](./raw_serial_captures/encoder_at_bottom.txt) for example of lift continuously sending status frames.

### Lift → Remote: Status Frames

**Frame Layout:**
```
55 AA TT LL HH SS
```

- `55 AA` = Sync/header bytes
- `TT` = Message type (observed: `0xA5`, `0xA6`; `0xA5` identifies position frames)
- `LL HH` = Position (little-endian uint16, bytes 4-5, 1-indexed)
- `SS` = Movement status: `0x04` when moving, `0x00` when stopped

**Position Decoding:**

The position is encoded in bytes 4-5 (1-indexed; bytes 3-4 in 0-indexed) as a little-endian uint16. The raw value maps to display units (centimeters) via:

```
display_position_cm = uint16_le(byte4, byte5) / 44.0
```

The position value represents height from the top of the lift platform to the base in centimeters.

**Examples:**

- Top position: `55 aa a6 bd 1d 00`
  - Raw: `0xBD1D` (little-endian) = `0x1DBD` = 7613
  - Display: 7613 / 44 = 173.0 cm

- Bottom position: `55 aa a5 8c 0c 00`
  - Raw: `0x8C0C` (little-endian) = `0x0C8C` = 3212
  - Display: 3212 / 44 = 73.0 cm

### Remote → Lift: Command Frames

**Frame Layouts:**

- **Button press**: `55 AA XX XX XX`
  - `55 AA` = Sync/header bytes
  - `XX XX XX` = Opcode repeated 3× (no checksum observed; triple repetition for reliability)

- **Wake/priming**: `55 AA F0 F0 F0`
  - Sent on button press if remote has been idle
  - User must press button again for action to proceed
  - Note: Some implementations may not use this command

- **Display sleep**: `55 FC`
  - Sent ~20 seconds after last button press
  - Remote LED display turns off when this is sent

**Button Command Table:**

| **Key** | **Code** | **Notes**|
|---|---|---|
| `1` | `55 AA D1 D1 D1` | |
| `2` | `55 AA D2 D2 D2` | |
| `3` | `55 AA D3 D3 D3` | |
| `4` | `55 AA D7 D7 D7` | |
| `Up` | `55 AA E3 E3 E3` | Press |
| `Up` | `55 AA E1 E1 E1` | Release |
| `Down` | `55 AA E2 E2 E2` | Press |
| `Down` | `55 AA E3 E3 E3` | Release |

### Parsing / Resync Notes

- Re-sync by scanning for `0x55 0xAA` sequence and reading the next 4 bytes
- Validate `TT` byte in status frames: should be `0xA5` or `0xA6` to discard noise
- Use `SS` byte to detect movement state: `0x04` = moving, `0x00` = stopped
- Range-check decoded position values (e.g., 70-180 cm) to filter invalid frames
- Button command frames repeat the opcode 3× for reliability; all three bytes should match

### Implementation Notes

- [cous](https://community.home-assistant.io/t/brand-new-to-home-assistant/367252/16?u=yepher) Tested successfully with serial communication at 9600 baud using pins 2 (TxD) and 4 (RxD)
- Button commands work correctly to emulate button presses, including all memory positions (1-4)
- Position frames stream continuously from the lift controller
- The wired remote can be removed and replaced with a microcontroller (e.g., ESP32) for control and monitoring
- See the [ESP32 implementation](./esp32/README.md) for a complete solution that provides web interface control and can be integrated with home automation systems (MQTT, Home Assistant, etc.)


**TO BE CONTINUED....**

