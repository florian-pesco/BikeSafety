# Android App

This folder contains the Android application for the bicycle safety prototype.

The app uses the rear camera to detect vehicles, tracks the largest relevant detection across frames, estimates the current threat level, and sends the threat level to the Control ESP over WiFi.

## Role In The Prototype

```text
Camera image
    |
    v
YOLO TensorFlow Lite detection
    |
    v
Vehicle tracking
    |
    v
Threat level: SAFE, APPROACHING, or DANGER
    |
    v
HTTP request to Control ESP
```

## WiFi Communication

Before running the app, connect the Android phone to the Control ESP access point:

```text
SSID: BikePrototype
Password: 12345678
```

The app sends changed threat levels to:

```text
http://192.168.4.1/api/threat?level=0
http://192.168.4.1/api/threat?level=1
http://192.168.4.1/api/threat?level=2
```

The endpoint is configured in:

```text
app/src/main/java/com/surendramaran/yolov8tflite/wifi/WifiConstants.kt
```

## Important Source Files

```text
app/src/main/java/com/surendramaran/yolov8tflite/MainActivity.kt
```

Connects the camera, object detector, tracker, status overlay, and WiFi sender.

```text
app/src/main/java/com/surendramaran/yolov8tflite/wifi/WifiEspManager.kt
```

Sends threat levels to the Control ESP and prints debug output with Android Logcat tag `WiFiESP`.

```text
app/src/main/java/com/surendramaran/yolov8tflite/tracker/
```

Contains the lightweight vehicle tracking and threat evaluation logic.

```text
app/src/main/assets/
```

Contains the active TensorFlow Lite model and labels:

```text
yolov8n_float32.tflite
labels.txt
```

## Build And Run

1. Open this `android/` folder in Android Studio.
2. Let Android Studio install or locate the Android SDK.
3. If needed, ensure `local.properties` contains the local SDK path. Example:

```properties
sdk.dir=C\:\\Users\\USER\\AppData\\Local\\Android\\Sdk
```

4. Connect the Android device to the `BikePrototype` WiFi network.
5. Build and run the app on the Android device.

The app needs camera and network permissions. BLE permissions are no longer required in the final WiFi version.

## Debugging

Use Android Studio Logcat:

```text
WiFiESP
```

Useful status messages are also shown in the app overlay:

```text
WiFi: connected, waiting for ESP
WiFi: SAFE sent
WiFi: APPROACHING sent
WiFi: DANGER sent
WiFi: ESP not reachable
```

If the app cannot reach the ESP:

- confirm the phone is connected to `BikePrototype`
- keep the phone connected even if Android warns that the WiFi has no internet
- open `http://192.168.4.1/api/status` in a browser on the phone
- check the Control ESP serial monitor at `115200` baud
