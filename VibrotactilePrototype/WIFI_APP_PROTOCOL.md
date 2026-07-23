# Android to Control ESP WiFi Protocol

The control ESP creates its own WiFi access point:

```text
SSID: BikePrototype
Password: 12345678
IP: 192.168.4.1
```

The Android app sends threat changes with HTTP POST:

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

The control ESP prints each HTTP request to Serial and forwards changed values to
the handle ESPs via ESP-NOW.

Forwarding:

| App Threat | ESP-NOW Scenario | Second Byte |
| ---------: | ---------------- | ----------: |
| 0 | `SCENARIO_STOP` | `SIDE_NONE` |
| 1 | `SCENARIO_APP_THREAT` | `1` |
| 2 | `SCENARIO_APP_THREAT` | `2` |

`SCENARIO_APP_THREAT` is scenario value `5`.

The handle ESPs pulse all motors until a new command arrives:

- `APPROACHING`: slower low-intensity pulse
- `DANGER`: faster high-intensity pulse
- `SAFE`: stop all motors

## Manual Test

After flashing `ControlESP.ino`, connect a phone or laptop to `BikePrototype`.

Open the existing web page:

```text
http://192.168.4.1/
```

Then test the app endpoint:

```text
http://192.168.4.1/api/status
```

Use any HTTP client to send POST requests:

```text
POST http://192.168.4.1/api/threat?level=1
POST http://192.168.4.1/api/threat?level=2
POST http://192.168.4.1/api/threat?level=0
```

For quick testing, the same endpoint also accepts browser GET requests:

```text
http://192.168.4.1/api/threat?level=1
http://192.168.4.1/api/threat?level=2
http://192.168.4.1/api/threat?level=0
```

Expected control ESP Serial output includes:

```text
HTTP /api/threat
Raw level: 1
Threat request decoded: 1 = APPROACHING
Command: APPROACHING
Left queued: yes
Right queued: yes
```
