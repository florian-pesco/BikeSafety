#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

/*
  ============================================================
  VERSUCHSLEITERSCHNITTSTELLE
  ============================================================

  scenario:
  0 = Stop
  1 = Motortest
  2 = Überholen
  3 = Dooring
  4 = Rechtsabbieger

  side:
  0 = keine
  1 = links
  2 = rechts
*/

// ------------------------------------------------------------
// WLAN
// ------------------------------------------------------------

const char* ssid = "BikePrototype";
const char* password = "12345678";

const uint8_t wifiChannel = 6;

WebServer server(80);

// ------------------------------------------------------------
// MAC-ADRESSEN EINTRAGEN
// ------------------------------------------------------------

// Linker Griff
uint8_t leftHandleMac[] = {
  0xE0, 0x72, 0xA1, 0x21, 0x58, 0x50
};

// Rechter Griff
uint8_t rightHandleMac[] = {
  0x70, 0x4b, 0xca, 0x82, 0x4b, 0xa8
};

// ------------------------------------------------------------
// BEFEHLE
// ------------------------------------------------------------

enum Scenario : uint8_t {
  SCENARIO_STOP = 0,
  SCENARIO_MOTOR_TEST = 1,
  SCENARIO_OVERTAKE = 2,
  SCENARIO_DOORING = 3,
  SCENARIO_RIGHT_TURN = 4,
  SCENARIO_APP_THREAT = 5
};

enum DangerSide : uint8_t {
  SIDE_NONE = 0,
  SIDE_LEFT = 1,
  SIDE_RIGHT = 2
};

struct ControlMessage {
  uint8_t scenario;
  uint8_t side;
};

// ------------------------------------------------------------
// STATUS
// ------------------------------------------------------------

String currentAction = "Ready";
uint8_t currentThreatLevel = 0;
uint8_t lastForwardedThreatLevel = 255;

void handleRoot();
void handleStatus();
void handleThreat();

// ------------------------------------------------------------
// ESP-NOW
// ------------------------------------------------------------

bool addPeer(const uint8_t* macAddress) {
  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr, macAddress, 6);
  peerInfo.channel = wifiChannel;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_AP;

  if (esp_now_is_peer_exist(macAddress)) {
    return true;
  }

  esp_err_t result = esp_now_add_peer(&peerInfo);

  if (result != ESP_OK) {
    Serial.print("Peer error: ");
    Serial.println(result);
    return false;
  }

  return true;
}

bool sendToHandle(
  const uint8_t* macAddress,
  const ControlMessage& message
) {
  esp_err_t result = esp_now_send(
    macAddress,
    reinterpret_cast<const uint8_t*>(&message),
    sizeof(message)
  );

  return result == ESP_OK;
}

void sendCommand(
  uint8_t scenario,
  uint8_t side,
  const String& description
) {
  ControlMessage message = {
    scenario,
    side
  };

  bool leftSent = sendToHandle(leftHandleMac, message);
  bool rightSent = sendToHandle(rightHandleMac, message);

  currentAction = description;

  Serial.println();
  Serial.print("Command: ");
  Serial.println(description);

  Serial.print("Left queued: ");
  Serial.println(leftSent ? "yes" : "no");

  Serial.print("Right queued: ");
  Serial.println(rightSent ? "yes" : "no");
}

const char* threatLabel(uint8_t level) {
  switch (level) {
    case 0:
      return "SAFE";

    case 1:
      return "APPROACHING";

    case 2:
      return "DANGER";

    default:
      return "INVALID";
  }
}

void applyThreatLevel(uint8_t level) {
  Serial.println();
  Serial.print("Threat request decoded: ");
  Serial.print(level);
  Serial.print(" = ");
  Serial.println(threatLabel(level));

  currentThreatLevel = level;

  if (level == lastForwardedThreatLevel) {
    Serial.println("Threat unchanged, no ESP-NOW command sent");
    return;
  }

  lastForwardedThreatLevel = level;

  switch (level) {
    case 0:
      sendCommand(
        SCENARIO_STOP,
        SIDE_NONE,
        "SAFE"
      );
      break;

    case 1:
      sendCommand(
        SCENARIO_APP_THREAT,
        1,
        "APPROACHING"
      );
      break;

    case 2:
      sendCommand(
        SCENARIO_APP_THREAT,
        2,
        "DANGER"
      );
      break;

    default:
      Serial.println("Invalid threat ignored");
      break;
  }
}

// ------------------------------------------------------------
// HTTP
// ------------------------------------------------------------

void sendOK(const String& message) {
  server.send(200, "text/plain", message);
}

void registerApiRoutes() {
  server.on("/api/stop", HTTP_POST, []() {
    sendCommand(
      SCENARIO_STOP,
      SIDE_NONE,
      "Stopped"
    );

    sendOK("STOP");
  });

  server.on("/api/test", HTTP_POST, []() {
    sendCommand(
      SCENARIO_MOTOR_TEST,
      SIDE_NONE,
      "Motor Test"
    );

    sendOK("MOTOR_TEST");
  });

  server.on("/api/overtake/left", HTTP_POST, []() {
    sendCommand(
      SCENARIO_OVERTAKE,
      SIDE_LEFT,
      "Overtaking Left"
    );

    sendOK("OVERTAKE_LEFT");
  });

  server.on("/api/overtake/right", HTTP_POST, []() {
    sendCommand(
      SCENARIO_OVERTAKE,
      SIDE_RIGHT,
      "Overtaking Right"
    );

    sendOK("OVERTAKE_RIGHT");
  });

  server.on("/api/rightturn", HTTP_POST, []() {
    sendCommand(
      SCENARIO_RIGHT_TURN,
      SIDE_RIGHT,
      "Right Turn"
    );

    sendOK("RIGHT_TURN");
  });

  server.on("/api/dooring/left", HTTP_POST, []() {
    sendCommand(
      SCENARIO_DOORING,
      SIDE_LEFT,
      "Dooring Left"
    );

    sendOK("DOORING_LEFT");
  });

  server.on("/api/dooring/right", HTTP_POST, []() {
    sendCommand(
      SCENARIO_DOORING,
      SIDE_RIGHT,
      "Dooring Right"
    );

    sendOK("DOORING_RIGHT");
  });

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/threat", HTTP_POST, handleThreat);
  server.on("/api/threat", HTTP_GET, handleThreat);
}

// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP_STA);

  if (!WiFi.softAP(ssid, password, wifiChannel)) {
    Serial.println("Could not start Wi-Fi");
    return;
  }

  Serial.println();
  Serial.println("Wi-Fi started");
  Serial.print("Network: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("Browser: http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Android threat endpoint: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/api/threat?level=0");
  Serial.print("Channel: ");
  Serial.println(WiFi.channel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Could not start ESP-NOW");
    return;
  }

  bool leftAdded = addPeer(leftHandleMac);
  bool rightAdded = addPeer(rightHandleMac);

  Serial.print("Left handle registered: ");
  Serial.println(leftAdded ? "yes" : "no");

  Serial.print("Right handle registered: ");
  Serial.println(rightAdded ? "yes" : "no");

  server.on("/", HTTP_GET, handleRoot);
  registerApiRoutes();

  server.begin();

  Serial.println("Web server ready");
}

// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop() {
  server.handleClient();
  delay(2);
}

// ------------------------------------------------------------
// STATUS
// ------------------------------------------------------------

void handleStatus() {
  String json = "{\"action\":\"";
  json += currentAction;
  json += "\",\"threat\":";
  json += currentThreatLevel;
  json += "}";

  server.send(200, "application/json", json);
}

void handleThreat() {
  Serial.println();
  Serial.println("HTTP /api/threat");
  Serial.print("Client: ");
  Serial.println(server.client().remoteIP());

  if (!server.hasArg("level")) {
    Serial.println("Missing query parameter: level");
    server.send(400, "text/plain", "MISSING_LEVEL");
    return;
  }

  String rawLevel = server.arg("level");

  Serial.print("Raw level: ");
  Serial.println(rawLevel);

  if (
    rawLevel != "0" &&
    rawLevel != "1" &&
    rawLevel != "2"
  ) {
    Serial.println("Invalid level, expected 0, 1, or 2");
    server.send(400, "text/plain", "INVALID_LEVEL");
    return;
  }

  int level = rawLevel.toInt();

  applyThreatLevel(static_cast<uint8_t>(level));

  String response = "THREAT_";
  response += threatLabel(static_cast<uint8_t>(level));

  server.send(200, "text/plain", response);
}

// ------------------------------------------------------------
// WEBINTERFACE
// ------------------------------------------------------------

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width, initial-scale=1.0"
>

<title>Bike Safety Prototype</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  padding: 28px;
  background: #111317;
  color: white;
  font-family: Arial, sans-serif;
}

.container {
  max-width: 620px;
  margin: 0 auto;
}

h1 {
  font-size: 32px;
  margin: 0 0 24px;
}

.section-title {
  color: #8f96a3;
  font-size: 13px;
  letter-spacing: 1px;
  text-transform: uppercase;
  margin: 24px 0 10px 4px;
}

.status {
  background: #20242c;
  border-radius: 14px;
  padding: 18px 20px;
  margin-bottom: 25px;
  color: #8f96a3;
}

.status strong {
  color: white;
}

.card {
  display: block;
  width: 100%;
  border: 0;
  text-decoration: none;
  color: white;
  background: #20242c;
  border-radius: 14px;
  padding: 28px 24px;
  margin-bottom: 16px;
  text-align: left;
  cursor: pointer;
  font-family: inherit;
}

.card.selected {
  background: #4b5160;
}

.card:active {
  transform: scale(0.985);
}


.card-row {
  display: flex;
  gap: 16px;
  width: 100%;
  margin-bottom: 16px;
}

.card-row .card {
  flex: 1 1 0;
  width: 0;
  min-width: 0;
  margin-bottom: 0;
}

.card-title {
  font-size: 20px;
  font-weight: 800;
}

.action-row {
  display: flex;
  gap: 16px;
  margin-top: 26px;
}

.action-button {
  flex: 1;
  border: 0;
  border-radius: 14px;
  padding: 20px;
  color: white;
  background: #2c313b;
  font-size: 17px;
  font-weight: 800;
  cursor: pointer;
}

.stop-button {
  background: #b93838;
}

@media (max-width: 480px) {
  body {
    padding: 18px;
  }

  .card-row {
    flex-direction: row;
    gap: 10px;
  }

  .card-row .card {
    padding: 22px 12px;
  }

  .card-row .card-title {
    font-size: 17px;
    text-align: center;
  }

  .action-row {
    flex-direction: column;
  }
}

</style>
</head>

<body>

<div class="container">

  <h1>Bike Safety Prototype</h1>

  <div class="card-row">

    <button
      class="card"
      data-endpoint="/api/overtake/left"
      data-label="Overtaking Left"
      onclick="activateScenario(this)"
    >
      <div class="card-title">Overtaking Left</div>
    </button>

    <button
      class="card"
      data-endpoint="/api/overtake/right"
      data-label="Overtaking Right"
      onclick="activateScenario(this)"
    >
      <div class="card-title">Overtaking Right</div>
    </button>

  </div>

  <button
    class="card"
    data-endpoint="/api/rightturn"
    data-label="Right Turn"
    onclick="activateScenario(this)"
  >
    <div class="card-title">Right Turn</div>
  </button>


  <div class="card-row">

    <button
      class="card"
      data-endpoint="/api/dooring/left"
      data-label="Dooring Left"
      onclick="activateScenario(this)"
    >
      <div class="card-title">Dooring Left</div>
    </button>

    <button
      class="card"
      data-endpoint="/api/dooring/right"
      data-label="Dooring Right"
      onclick="activateScenario(this)"
    >
      <div class="card-title">Dooring Right</div>
    </button>

  </div>

  <div class="section-title">Actions</div>

  <div class="action-row">

    <button
      class="action-button"
      onclick="sendAction('/api/test', 'Motor Test')"
    >
      Motor Test
    </button>

    <button
      class="action-button stop-button"
      onclick="stopEverything()"
    >
      STOP
    </button>

  </div>

</div>

<script>
let activeButton = null;
let requestRunning = false;

async function postCommand(endpoint) {
  const response = await fetch(endpoint, {
    method: "POST",
    cache: "no-store"
  });

  if (!response.ok) {
    throw new Error("HTTP " + response.status);
  }

  return response.text();
}

function clearSelection() {
  document
    .querySelectorAll(".card.selected")
    .forEach((button) => {
      button.classList.remove("selected");
    });

  activeButton = null;
}

function setStatus(text) {
  document
    .getElementById("statusText")
    .textContent = text;
}

async function activateScenario(button) {
  if (requestRunning) {
    return;
  }

  requestRunning = true;

  try {
    if (activeButton === button) {
      await postCommand("/api/stop");

      clearSelection();
      setStatus("Stopped");
      return;
    }

    await postCommand(button.dataset.endpoint);

    clearSelection();

    button.classList.add("selected");
    activeButton = button;

    setStatus(button.dataset.label);

  } catch (error) {
    console.error(error);
    setStatus("Connection Error");

  } finally {
    requestRunning = false;
  }
}

async function sendAction(endpoint, label) {
  if (requestRunning) {
    return;
  }

  requestRunning = true;

  try {
    await postCommand(endpoint);

    clearSelection();
    setStatus(label);

  } catch (error) {
    console.error(error);
    setStatus("Connection Error");

  } finally {
    requestRunning = false;
  }
}

async function stopEverything() {
  if (requestRunning) {
    return;
  }

  requestRunning = true;

  try {
    await postCommand("/api/stop");

    clearSelection();
    setStatus("Stopped");

  } catch (error) {
    console.error(error);
    setStatus("Connection Error");

  } finally {
    requestRunning = false;
  }
}
</script>

</body>
</html>
)rawliteral";

  server.send(
    200,
    "text/html; charset=utf-8",
    html
  );
}
