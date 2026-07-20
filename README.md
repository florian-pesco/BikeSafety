# Bike Safety Prototype

A smartphone-based bicycle safety assistance system that detects vehicles approaching from behind and warns the rider through intuitive haptic feedback.

This project was developed as a proof-of-concept to investigate whether a standard Android smartphone can be used as a low-cost rear sensing system for bicycles.

---

## Project Goal

The objective is to warn cyclists about vehicles approaching from behind without requiring them to look at a display.

The smartphone continuously analyzes the rear camera feed using a real-time object detection model. When an approaching vehicle is detected, the phone communicates the warning to an ESP32 mounted inside the handlebars. The ESP32 then activates vibration motors and LEDs to provide intuitive feedback to the cyclist.

The focus of this project is validating the complete sensing and communication pipeline rather than building a production-ready assistance system.

---

## Features

### Android Application

- Live rear camera feed using CameraX
- Real-time object detection using YOLOv8 TensorFlow Lite
- Vehicle filtering
- Lightweight object tracking
- Threat evaluation based on bounding box growth
- Bluetooth Low Energy (BLE) communication
- Automatic BLE reconnection
- Minimal user interface

### ESP32

- BLE peripheral
- Automatic advertising
- Automatic reconnection
- Receives threat levels from Android
- Detailed Serial debug output
- Minimal one-file receiver sketch

---

## System Architecture

```
                 Rear Camera
                      │
                      ▼
              Android Smartphone
                      │
               YOLOv8 Detection
                      │
               Vehicle Tracking
                      │
              Threat Evaluation
                      │
          Bluetooth Low Energy
                      │
                      ▼
                   ESP32
                      │
        ┌─────────────┴─────────────┐
        ▼                           ▼
   Vibration Motors            LED Indicators
```

---

## BLE Communication

The Android application communicates with the ESP32 using Bluetooth Low Energy.

The protocol intentionally remains extremely small.

| Value | Meaning |
|-------:|---------|
| 0 | SAFE |
| 1 | APPROACHING |
| 2 | DANGER |

Only a single byte is transmitted whenever the warning level changes.

---

## Repository Structure

```
.
├── android/
│   └── ...
│
├── esp32/
│   ├── README.md
│   └── example.ino
│
└── README.md
```

---

## Android Technologies

- Kotlin
- Android Studio
- CameraX
- TensorFlow Lite
- YOLOv8
- Bluetooth Low Energy (BLE)

---

## ESP32 Technologies

- Arduino Framework
- Official ESP32 Arduino BLE library

---

## Building the Android App

Open the Android project in Android Studio.

Run

```
Build → Generate Signed Bundle / APK
```

or build a debug APK directly from Android Studio.

---

## ESP32 Setup

Open and upload

```
esp32/example.ino
```

with the Arduino IDE. The sketch uses the BLE library included with the ESP32
Arduino Core, not NimBLE. More details are in `esp32/README`.

---

## Limitations

This project is intended as a proof-of-concept.

Current limitations include:

- Monocular camera only
- Bounding-box based distance estimation
- No lane estimation
- No speed estimation
- No object re-identification across long occlusions
- No weather or nighttime optimization
- Android only
