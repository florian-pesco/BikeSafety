# Vibrotactile ESP32 Prototype

This folder contains the Arduino code for the ESP32 handlebar prototype.

The prototype uses one Control ESP and two handle ESPs:

- `ControlESP.ino`
- `LeftHandleESP.ino`
- `RightHandleESP.ino`

The Control ESP receives commands from either the Android app or the built-in test website. It forwards compact commands to the handle ESPs with ESP-NOW.

## Architecture

```text
Android app or browser
        |
        v
WiFi HTTP request
        |
        v
ControlESP.ino
        |
        v
ESP-NOW message
        |
        v
LeftHandleESP.ino and RightHandleESP.ino
        |
        v
Vibration motor patterns
```

## Control ESP

`ControlESP.ino` creates a WiFi access point:

```text
SSID: BikePrototype
Password: 12345678
IP address: 192.168.4.1
```

It hosts:

```text
http://192.168.4.1/
```

for manual browser testing.

It also exposes the Android app endpoint:

```text
POST http://192.168.4.1/api/threat?level=0
POST http://192.168.4.1/api/threat?level=1
POST http://192.168.4.1/api/threat?level=2
```

For quick manual testing, the same endpoint also accepts browser GET requests:

```text
http://192.168.4.1/api/threat?level=1
http://192.168.4.1/api/threat?level=2
http://192.168.4.1/api/threat?level=0
```

Threat levels:

| Level | Meaning |
| ----: | ------- |
| 0 | SAFE |
| 1 | APPROACHING |
| 2 | DANGER |

## ESP-NOW Message

The Control ESP forwards this structure to both handle ESPs:

```cpp
struct ControlMessage {
  uint8_t scenario;
  uint8_t side;
};
```

Important scenarios:

| Scenario | Value | Meaning |
| -------: | ----: | ------- |
| `SCENARIO_STOP` | 0 | Stop all motors |
| `SCENARIO_MOTOR_TEST` | 1 | Run motor test |
| `SCENARIO_OVERTAKE` | 2 | Manual overtake demo |
| `SCENARIO_DOORING` | 3 | Manual dooring demo |
| `SCENARIO_RIGHT_TURN` | 4 | Manual right-turn demo |
| `SCENARIO_APP_THREAT` | 5 | Live Android threat warning |

For `SCENARIO_APP_THREAT`, the second byte contains the app threat level:

```text
1 = APPROACHING
2 = DANGER
```

`SAFE` is forwarded as `SCENARIO_STOP`.

## Flashing

Use the Arduino IDE.

Recommended serial monitor speed for all ESPs:

```text
115200 baud
```

Flash the sketches to the matching boards:

```text
ControlESP.ino      -> Control ESP
LeftHandleESP.ino   -> Left handle ESP
RightHandleESP.ino  -> Right handle ESP
```

The handle ESPs print their MAC addresses on boot. These addresses must match the `leftHandleMac` and `rightHandleMac` arrays in `ControlESP.ino`.

## Manual Test Procedure

1. Flash the left and right handle ESPs.
2. Open their serial monitors and confirm:

```text
ESP-NOW bereit
Warte auf Befehle des Steuer-ESP
```

3. Flash the Control ESP.
4. Open the Control ESP serial monitor and confirm:

```text
Wi-Fi started
Network: BikePrototype
Browser: http://192.168.4.1
Web server ready
```

5. Connect a phone or laptop to the WiFi network `BikePrototype`.
6. Open:

```text
http://192.168.4.1/
```

7. Press the browser buttons or open:

```text
http://192.168.4.1/api/threat?level=1
http://192.168.4.1/api/threat?level=2
http://192.168.4.1/api/threat?level=0
```

Expected Control ESP debug output:

```text
HTTP /api/threat
Raw level: 1
Threat request decoded: 1 = APPROACHING
Command: APPROACHING
Left queued: yes
Right queued: yes
```

Expected handle ESP debug output:

```text
Szenario empfangen: 5 | Seite: 1
App-Warnung: APPROACHING
```

## Notes

- All three ESPs must use the same WiFi/ESP-NOW channel.
- The current channel is `6`.
- If ESP-NOW queueing fails, re-check the handle MAC addresses in `ControlESP.ino`.
- The vibration intensities and motor pins are prototype values and may need adjustment for the final hardware.
- The code prints useful debug information even when vibration motors are not connected.
