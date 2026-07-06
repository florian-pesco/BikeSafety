#ifndef BIKE_SAFETY_BLE_H
#define BIKE_SAFETY_BLE_H

#include <NimBLEDevice.h>
#include <Arduino.h>

enum class ThreatLevel : uint8_t {
    SAFE = 0,
    APPROACHING = 1,
    DANGER = 2
};

class BikeSafetyBLE {

public:

    BikeSafetyBLE();

    void begin();

    void update();

    bool isConnected() const;

    ThreatLevel getThreatLevel() const;

private:

    void setConnected(bool connected);

    void setThreatLevel(ThreatLevel level);

    unsigned long lastPacketMillis = 0;

    static constexpr unsigned long PACKET_TIMEOUT = 5000;

    bool connected = false;

    ThreatLevel currentThreatLevel = ThreatLevel::SAFE;

    NimBLEServer* server = nullptr;
    NimBLEService* service = nullptr;
    NimBLECharacteristic* characteristic = nullptr;

};

void BikeSafetyBLE::setConnected(bool value) {

    connected = value;

}

void BikeSafetyBLE::setThreatLevel(ThreatLevel level) {

    if (currentThreatLevel == level)
        return;

    currentThreatLevel = level;
    lastPacketMillis = millis();

}

#endif