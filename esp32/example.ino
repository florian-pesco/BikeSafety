#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

/*
  Bike Safety ESP32 BLE receiver

  Board:
    ESP32-WROOM-32E

  Arduino core:
    ESP32 Arduino Core 3.3.10

  BLE protocol:
    Device name:         BikeSafetyESP
    Service UUID:        12345678-1234-1234-1234-1234567890ab
    Characteristic UUID: 87654321-4321-4321-4321-ba0987654321

  Payload:
    0 = SAFE
    1 = APPROACHING
    2 = DANGER

  This sketch only receives one byte and prints the decoded threat level.
  It intentionally contains no motor, LED, or application logic.
*/

static const char* DEVICE_NAME = "BikeSafetyESP";
static const char* SERVICE_UUID = "12345678-1234-1234-1234-1234567890ab";
static const char* CHARACTERISTIC_UUID = "87654321-4321-4321-4321-ba0987654321";

static BLEServer* server = nullptr;
static BLEService* service = nullptr;
static BLECharacteristic* threatCharacteristic = nullptr;

static bool phoneConnected = false;
static bool restartAdvertisingRequested = false;
static bool advertisingConfigured = false;

static void printThreat(uint8_t value) {
  Serial.print("Threat decoded: ");

  switch (value) {
    case 0:
      Serial.println("SAFE");
      break;

    case 1:
      Serial.println("APPROACHING");
      break;

    case 2:
      Serial.println("DANGER");
      break;

    default:
      Serial.print("INVALID (");
      Serial.print(value);
      Serial.println(")");
      break;
  }
}

static void startAdvertising() {
  Serial.println("Starting advertising");

  BLEAdvertising* advertising = BLEDevice::getAdvertising();

  if (!advertisingConfigured) {
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);

    // These values are commonly used in ESP32 BLE examples to improve Android compatibility.
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);

    advertisingConfigured = true;
    Serial.println("Advertising configured");
  }

  BLEDevice::startAdvertising();

  Serial.print("Advertising started as ");
  Serial.println(DEVICE_NAME);
  Serial.println("Waiting for phone...");
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* connectedServer) override {
    (void)connectedServer;

    phoneConnected = true;
    restartAdvertisingRequested = false;

    Serial.println("Connected");
  }

  void onDisconnect(BLEServer* disconnectedServer) override {
    (void)disconnectedServer;

    phoneConnected = false;
    restartAdvertisingRequested = true;

    Serial.println("Disconnected");
    Serial.println("Advertising restart requested");
  }
};

class ThreatCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    Serial.println("Characteristic written");

    String payload = characteristic->getValue();

    Serial.print("Payload received, length=");
    Serial.println(payload.length());

    if (payload.length() != 1) {
      Serial.println("Ignoring payload because it is not exactly one byte");
      return;
    }

    uint8_t threatValue = static_cast<uint8_t>(payload[0]);

    Serial.print("Raw byte: ");
    Serial.println(threatValue);

    printThreat(threatValue);
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Boot");
  Serial.println("Initializing BLE");

  BLEDevice::init(DEVICE_NAME);
  Serial.println("BLE initialized");

  Serial.println("Creating server");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  Serial.println("Server created");

  Serial.println("Creating service");
  service = server->createService(SERVICE_UUID);
  Serial.println("Service created");

  Serial.println("Creating characteristic");
  threatCharacteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  threatCharacteristic->setCallbacks(new ThreatCharacteristicCallbacks());
  Serial.println("Characteristic created");

  Serial.println("Starting service");
  service->start();
  Serial.println("Service started");

  startAdvertising();

  Serial.println("READY");
}

void loop() {
  if (restartAdvertisingRequested) {
    restartAdvertisingRequested = false;
    delay(500);
    startAdvertising();
  }

  delay(50);
}
