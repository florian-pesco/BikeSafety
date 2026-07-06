#include "BikeSafetyBLE.h"

BikeSafetyBLE ble;

void setup() {

    Serial.begin(115200);

    ble.begin();

}

void loop() {

    ble.update();

    switch (ble.getThreatLevel()) {

        case ThreatLevel::SAFE:
            // keine Vibration
            break;

        case ThreatLevel::APPROACHING:
            // Vibrationsmodus 1
            break;

        case ThreatLevel::DANGER:
            // Vibrationsmodus 2
            break;
    }

    delay(10);

}