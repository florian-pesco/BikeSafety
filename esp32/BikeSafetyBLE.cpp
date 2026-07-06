#include "BikeSafetyBLE.h"

/*
Bike Safety BLE Protocol

Device Name:
BikeSafetyESP

Service UUID:
12345678-1234-1234-1234-1234567890ab

Characteristic UUID:
87654321-4321-4321-4321-ba0987654321

Payload:

0 = SAFE

1 = APPROACHING

2 = DANGER
*/

static const char* DEVICE_NAME = "BikeSafetyESP";

static const char* SERVICE_UUID =
    "12345678-1234-1234-1234-1234567890ab";

static const char* CHARACTERISTIC_UUID =
    "87654321-4321-4321-4321-ba0987654321";


/*
 * Pointer to the active BikeSafetyBLE instance.
 * This allows the BLE callbacks to update the object.
 */
static BikeSafetyBLE* activeInstance = nullptr;


/*
 * ===========================================================
 * Server callbacks
 * ===========================================================
 */

class ServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer* server) override {

        if (activeInstance != nullptr) {
            activeInstance->setConnected(true);
            Serial.println("Phone connected");
        }

    }

    void onDisconnect(NimBLEServer* server) override {

        if (activeInstance != nullptr) {
            activeInstance->setConnected(false);
            activeInstance->setThreatLevel(ThreatLevel::SAFE);
            Serial.println("Phone disconnected");
        }

        NimBLEDevice::startAdvertising();
    }

};


/*
 * ===========================================================
 * Characteristic callbacks
 * ===========================================================
 */

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {

    void onWrite(NimBLECharacteristic* characteristic) override {

        if (activeInstance == nullptr)
            return;

        std::string value = characteristic->getValue();

        if (value.length() != 1)
            return;

        uint8_t received = value[0];

        switch (received) {

            case 0:
                activeInstance->setThreatLevel(ThreatLevel::SAFE);
                break;

            case 1:
                activeInstance->setThreatLevel(ThreatLevel::APPROACHING);
                break;

            case 2:
                activeInstance->setThreatLevel(ThreatLevel::DANGER);
                break;

            default:
                // Ignore invalid values
                break;
        }
    }

};


/*
 * ===========================================================
 * BikeSafetyBLE
 * ===========================================================
 */

BikeSafetyBLE::BikeSafetyBLE() {

    activeInstance = this;

}

void BikeSafetyBLE::begin() {
    Serial.println("BikeSafetyBLE started");
    Serial.println("Advertising...");

    NimBLEDevice::init(DEVICE_NAME);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    service =
        server->createService(SERVICE_UUID);

    characteristic =
        service->createCharacteristic(
            CHARACTERISTIC_UUID,
            NIMBLE_PROPERTY::WRITE
        );

    characteristic->setCallbacks(
        new CharacteristicCallbacks()
    );

    service->start();

    NimBLEAdvertising* advertising =
        NimBLEDevice::getAdvertising();

    advertising->addServiceUUID(SERVICE_UUID);

    advertising->start();

}

void BikeSafetyBLE::update() {

    if (!connected) {
        return;
    }

    if (millis() - lastPacketMillis > PACKET_TIMEOUT) {
        setThreatLevel(ThreatLevel::SAFE);
    }

}

bool BikeSafetyBLE::isConnected() const {

    return connected;

}

ThreatLevel BikeSafetyBLE::getThreatLevel() const {

    return currentThreatLevel;

}