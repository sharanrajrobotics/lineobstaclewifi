# RoboDrive — ESP32 4WD Robot

A mobile-friendly control dashboard and starter firmware for an ESP32 robot with an L298N driver, 5-channel digital line sensor, HC-SR04 ultrasonic sensor, and a small servo.

## Features
- Dark, light, and neon themes
- Touch-friendly virtual joystick and directional buttons
- Motor speed slider (0–255 PWM)
- Manual, line-following, and obstacle-avoidance modes
- Connection indicator and emergency stop
- ESP32 Wi-Fi access point; no router required

## Repository files
- `firmware/RoboDrive_ESP32.ino` — ESP32 firmware
- `web/index.html` — standalone dashboard source
- `data/index.html` — dashboard served by the ESP32 from SPIFFS

## Hardware pin map (edit if your board differs)
| Function | ESP32 GPIO |
|---|---:|
| L298N ENA / left speed | 25 |
| L298N IN1, IN2 | 26, 27 |
| L298N ENB / right speed | 14 |
| L298N IN3, IN4 | 32, 33 |
| Line sensor outputs, left to right | 16, 17, 18, 19, 21 |
| Servo signal | 13 |
| HC-SR04 TRIG | 22 |
| HC-SR04 ECHO | 34 through a voltage divider |

The two motors on each side are assumed to be connected in parallel to one L298N channel. Check the combined stall current: an L298N board may be undersized for four motors.

## Build and upload
1. Install the ESP32 board package in Arduino IDE.
2. Install the **ESP32Servo** library.
3. Open `firmware/RoboDrive_ESP32.ino`, select the correct ESP32 board and port, and upload the sketch.
4. Upload `data/index.html` to the ESP32 SPIFFS filesystem using a filesystem uploader compatible with your Arduino IDE / ESP32 core version. The sketch serves it at the root path.
5. Connect your phone to Wi-Fi **RobotCar**, password **robot123**.
6. Open **http://192.168.4.1**. Tap Connect if the dashboard does not auto-check the connection.

The dashboard uses `/cmd?c=F/B/L/R/S`, `/mode?m=manual/line/avoid`, and `/speed?v=0..255`. The firmware also exposes `/status`.

## Important calibration and safety notes
- The line sensor code assumes the sensor outputs LOW over the dark line. If your module behaves oppositely, change `LINE_ACTIVE_LOW` in the sketch.
- HC-SR04 Echo is commonly 5 V. **Never connect it directly to an ESP32 GPIO**; use a resistor divider or level shifter.
- Use a suitable regulated supply for the ESP32 and servo, and connect grounds together. Do not power motors from the ESP32 3.3 V pin.
- Two 18650 cells in series are a 2S pack (7.2–7.4 V nominal, 8.4 V fully charged). Use a protected 2S pack/BMS and a proper 8.4 V charger.
- Lift the wheels before the first test. Keep a physical battery disconnect within reach. The software stop is not a substitute for a hardware power switch.
- The pin map and code are a starting point; confirm your exact ESP32 expansion board, sensor output polarity, motor wiring, and current requirements before driving.

## Current limitations
The basic obstacle-avoidance routine performs timed turns and can briefly block web requests while turning. Tune the distance threshold and turn duration for your chassis. This code has not been tested on your exact robot hardware.
