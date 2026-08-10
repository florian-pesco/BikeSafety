# Bike Safety Prototype

This repository contains the digital appendix for a university bicycle safety prototype.

The prototype uses an Android smartphone as a rear-facing camera system. The app detects vehicles in the camera image, estimates whether the detected vehicle is approaching, and sends the resulting threat level to an ESP32-based handlebar prototype. The ESP32 prototype then forwards commands to left and right handle ESPs that drive vibration patterns.

## System Overview

```text
Android phone camera
        |
        v
YOLO object detection
        |
        v
Vehicle tracking and threat evaluation
        |
        v
WiFi HTTP request
        |
        v
Control ESP32 access point and web server
        |
        v
ESP-NOW
        |
        v
Left and right handle ESP32 vibration patterns
```

## Current Communication Path

The final prototype uses WiFi, not BLE.

The Control ESP creates a WiFi access point:

```text
SSID: BikePrototype
Password: 12345678
Control ESP IP: 192.168.4.1
```

The Android app sends threat changes to:

```text
POST http://192.168.4.1/api/threat?level=0
POST http://192.168.4.1/api/threat?level=1
POST http://192.168.4.1/api/threat?level=2
```

Threat levels:

| Level | Meaning |
| ----: | ------- |
| 0 | SAFE |
| 1 | APPROACHING |
| 2 | DANGER |

BLE was tested earlier in the project, but it was removed from this final appendix because the ESP32 BLE initialization was unreliable for the available hardware and time frame.

## Repository Structure

```text
.
|-- android/
|   |-- app/
|   |-- gradle/
|   |-- build.gradle.kts
|   |-- settings.gradle.kts
|   |-- gradlew
|   |-- gradlew.bat
|   `-- README.md
|
|-- VibrotactilePrototype/
|   |-- ControlESP.ino
|   |-- LeftHandleESP.ino
|   |-- RightHandleESP.ino
|   `-- README.md
|
`-- README.md
```

## Main Components

### Android App

Located in `android/`.

The app:

- opens the rear camera with CameraX
- runs a TensorFlow Lite YOLO model
- tracks detected vehicles across frames
- estimates threat level from bounding box growth
- sends changed threat levels to the Control ESP over WiFi
- displays detection and WiFi status on screen

See `android/README.md` for setup and build instructions.

### ESP32 Prototype

Located in `VibrotactilePrototype/`.

The prototype consists of three Arduino sketches:

- `ControlESP.ino`: creates the WiFi access point, hosts the web interface and HTTP API, forwards commands using ESP-NOW
- `LeftHandleESP.ino`: receives ESP-NOW commands and plays left handle vibration patterns
- `RightHandleESP.ino`: receives ESP-NOW commands and plays right handle vibration patterns

See `VibrotactilePrototype/README.md` for flashing and test instructions.

## Quick Test

1. Flash `ControlESP.ino` to the Control ESP.
2. Connect a phone or laptop to the WiFi network `BikePrototype`.
3. Open the manual control page:

```text
http://192.168.4.1/
```

4. Test the app endpoint in a browser:

```text
http://192.168.4.1/api/threat?level=1
http://192.168.4.1/api/threat?level=2
http://192.168.4.1/api/threat?level=0
```

5. Watch the Control ESP serial monitor at `115200` baud. It prints every received request and every forwarded ESP-NOW command.

## Notes

- The Android device must be connected to the `BikePrototype` WiFi network before starting the full app test.
- Android may warn that the network has no internet. Stay connected to the network anyway.
- The handle ESP MAC addresses are configured in `ControlESP.ino` and must match the actual handle devices.
- The project is a proof of concept, not a production safety system.
