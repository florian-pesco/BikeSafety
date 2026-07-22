#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

/*
  ============================================================
  LINKER HANDGRIFF – ESP-NOW-EMPFÄNGER
  ============================================================

  Angenommene Motorbelegung:

  L1 außen = GPIO 6
  L2       = GPIO 7
  L3       = GPIO 8
  L4       = GPIO 9
  L5 innen = GPIO 10

  Szenarien:
  0 = Stop
  1 = Motortest
  2 = Überholen
  3 = Dooring
  4 = Rechtsabbieger

  Seiten:
  0 = keine
  1 = links
  2 = rechts
*/

// ------------------------------------------------------------
// MOTOR-KONFIGURATION
// ------------------------------------------------------------

// Räumliche Reihenfolge:
// Lenkerende/außen → Lenkermitte/innen
const uint8_t motors[] = {6, 7, 8, 9, 10};

const uint8_t motorCount =
  sizeof(motors) / sizeof(motors[0]);

const uint32_t pwmFrequency = 5000;
const uint8_t pwmResolution = 8;

// PWM: 0–255
const uint8_t intensityLow = 255;
const uint8_t intensityMedium = 255;
const uint8_t intensityHigh = 255;
const uint8_t intensityAcute = 255;

// Dauer einer Annäherungsstufe
const unsigned long approachStageDuration = 1800;

// Pulsmuster bleibt gleich.
// Primär wird die Intensität verändert.
const unsigned long pulseOnTime = 250;
const unsigned long pulseOffTime = 400;

// Muss bei allen drei ESPs identisch sein.
const uint8_t espNowChannel = 6;

// Beim Rechtsabbieger beginnt der rechte Griff zuerst.
// Der linke Griff folgt leicht verzögert.
const unsigned long rightTurnLeftDelay = 120;

// ------------------------------------------------------------
// BEFEHLE DES STEUER-ESP
// ------------------------------------------------------------

enum Scenario : uint8_t {
  SCENARIO_STOP = 0,
  SCENARIO_MOTOR_TEST = 1,
  SCENARIO_OVERTAKE = 2,
  SCENARIO_DOORING = 3,
  SCENARIO_RIGHT_TURN = 4
};

enum DangerSide : uint8_t {
  SIDE_NONE = 0,
  SIDE_LEFT = 1,
  SIDE_RIGHT = 2
};

// Muss auf allen drei ESPs identisch sein.
struct ControlMessage {
  uint8_t scenario;
  uint8_t side;
};

volatile bool newCommandAvailable = false;
volatile uint8_t receivedScenario = SCENARIO_STOP;
volatile uint8_t receivedSide = SIDE_NONE;

// Wird bei jedem neuen Befehl erhöht.
// So können laufende Muster abgebrochen werden.
volatile uint32_t commandGeneration = 0;

// ------------------------------------------------------------
// MOTOR-GRUNDFUNKTIONEN
// ------------------------------------------------------------

void setMotor(uint8_t index, uint8_t intensity) {
  if (index >= motorCount) {
    return;
  }

  ledcWrite(motors[index], intensity);
}

void stopAllMotors() {
  for (uint8_t i = 0; i < motorCount; i++) {
    setMotor(i, 0);
  }
}

void setAllMotors(uint8_t intensity) {
  for (uint8_t i = 0; i < motorCount; i++) {
    setMotor(i, intensity);
  }
}

/*
  Für Dooring werden zunächst L4 und L5 verwendet.

  Diese beiden Motoren sollen im stabilen
  Handballen-/Daumenballenbereich liegen.

  Falls andere Positionen besser spürbar sind,
  hier die Indizes ändern.
*/
void setDooringMotors(uint8_t intensity) {
  stopAllMotors();

  setMotor(3, intensity);  // L4
  setMotor(4, intensity);  // L5
}

// Unterbrechbare Wartefunktion
bool waitInterruptible(
  unsigned long duration,
  uint32_t generationAtStart
) {
  unsigned long startTime = millis();

  while (millis() - startTime < duration) {
    if (commandGeneration != generationAtStart) {
      stopAllMotors();
      return false;
    }

    delay(5);
  }

  return true;
}

// ------------------------------------------------------------
// MOTORKALIBRIERUNG
// ------------------------------------------------------------

void playMotorTest(uint32_t generationAtStart) {
  Serial.println("Motortest: L1 bis L5");

  for (uint8_t i = 0; i < motorCount; i++) {
    Serial.print("Motor L");
    Serial.print(i + 1);
    Serial.print(" auf GPIO ");
    Serial.println(motors[i]);

    setMotor(i, intensityHigh);

    if (!waitInterruptible(700, generationAtStart)) {
      return;
    }

    setMotor(i, 0);

    if (!waitInterruptible(350, generationAtStart)) {
      return;
    }
  }

  stopAllMotors();
  Serial.println("Motortest beendet");
}

// ------------------------------------------------------------
// SZENARIO 1: ÜBERHOLEN
// ------------------------------------------------------------

/*
  Beim normalen Überholen kommt das Fahrzeug links.

  Deshalb spielt der linke Griff die Phi-Welle:

  L1 außen → L2 → L3 → L4 → L5 innen

  Die Geschwindigkeit bleibt gleich.
  Die Intensität steigt automatisch:

  schwach → mittel → stark
*/

bool playPhiWave(
  uint8_t intensity,
  uint32_t generationAtStart
) {
  for (uint8_t i = 0; i < motorCount; i++) {
    setMotor(i, intensity);

    if (!waitInterruptible(140, generationAtStart)) {
      return false;
    }

    setMotor(i, 0);

    if (!waitInterruptible(55, generationAtStart)) {
      return false;
    }
  }

  return true;
}

bool playPhiStage(
  uint8_t intensity,
  unsigned long duration,
  uint32_t generationAtStart
) {
  unsigned long stageStart = millis();

  while (millis() - stageStart < duration) {
    if (!playPhiWave(intensity, generationAtStart)) {
      return false;
    }

    if (!waitInterruptible(180, generationAtStart)) {
      return false;
    }
  }

  return true;
}

void playOvertakeLeft(uint32_t generationAtStart) {
  Serial.println(
    "Überholen links: automatische Annäherung"
  );

  Serial.println("Stufe 1: Fahrzeug weiter entfernt");

  if (!playPhiStage(
        intensityLow,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 2: Fahrzeug kommt näher");

  if (!playPhiStage(
        intensityMedium,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 3: Fahrzeug sehr nah");

  if (!playPhiStage(
        intensityHigh,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println(
    "Überholvorgang unmittelbar am Fahrrad"
  );

  // Abschließende deutliche Phi-Welle
  if (!playPhiWave(
        intensityAcute,
        generationAtStart
      )) {
    return;
  }

  stopAllMotors();
  Serial.println("Überholsequenz beendet");
}

void handleOvertake(
  uint8_t side,
  uint32_t generationAtStart
) {
  if (side == SIDE_LEFT) {
    playOvertakeLeft(generationAtStart);
    return;
  }

  if (side == SIDE_RIGHT) {
    Serial.println(
      "Überholen rechts: linker Griff bleibt ruhig"
    );

    stopAllMotors();
    return;
  }

  Serial.println("Überholen: keine gültige Seite");
}

// ------------------------------------------------------------
// SZENARIO 2: DOORING
// ------------------------------------------------------------

/*
  Dooring ist eine Präsenz-/Vorsichtswarnung:

  "Links stehen parkende Fahrzeuge.
   Hier könnte sich eine Tür öffnen."

  Keine Phi-Welle.
  Kein akuter Vollalarm.
  Sanfte, wiederkehrende Pulse auf L4 und L5.

  Das Muster läuft, bis ein neuer Befehl oder STOP kommt.
*/

void playDooringLeft(uint32_t generationAtStart) {
  Serial.println(
    "Dooring links: Präsenzwarnung aktiv"
  );

  while (commandGeneration == generationAtStart) {
    setDooringMotors(intensityLow);

    if (!waitInterruptible(
          220,
          generationAtStart
        )) {
      return;
    }

    stopAllMotors();

    if (!waitInterruptible(
          850,
          generationAtStart
        )) {
      return;
    }
  }
}

void handleDooring(
  uint8_t side,
  uint32_t generationAtStart
) {
  if (side == SIDE_LEFT) {
    playDooringLeft(generationAtStart);
    return;
  }

  if (side == SIDE_RIGHT) {
    Serial.println(
      "Dooring rechts: linker Griff bleibt ruhig"
    );

    stopAllMotors();
    return;
  }

  Serial.println("Dooring: keine gültige Seite");
}

// ------------------------------------------------------------
// SZENARIO 3: RECHTSABBIEGER
// ------------------------------------------------------------

/*
  Der Rechtsabbieger baut von rechts eine Barriere auf.

  Rechter Griff:
  beginnt sofort.

  Linker Griff:
  beginnt 120 ms später.

  Dadurch entsteht über beide Hände der Eindruck:
  Die Gefahr breitet sich von rechts nach links aus.

  Danach laufen auf beiden Griffen dieselben Stufen:

  schwach → mittel → stark → akut
*/

bool playAllMotorPulseStage(
  uint8_t intensity,
  unsigned long duration,
  uint32_t generationAtStart
) {
  unsigned long stageStart = millis();

  while (millis() - stageStart < duration) {
    setAllMotors(intensity);

    if (!waitInterruptible(
          pulseOnTime,
          generationAtStart
        )) {
      return false;
    }

    stopAllMotors();

    if (!waitInterruptible(
          pulseOffTime,
          generationAtStart
        )) {
      return false;
    }
  }

  return true;
}

void playRightTurn(uint32_t generationAtStart) {
  Serial.println(
    "Rechtsabbieger: linker Griff folgt verzögert"
  );

  // Rechter Griff startet zuerst.
  if (!waitInterruptible(
        rightTurnLeftDelay,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 1: möglicher Konflikt");

  if (!playAllMotorPulseStage(
        intensityLow,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 2: Fahrzeug kommt näher");

  if (!playAllMotorPulseStage(
        intensityMedium,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 3: unmittelbare Nähe");

  if (!playAllMotorPulseStage(
        intensityHigh,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("AKUT: Radweg wird gekreuzt");

  setAllMotors(intensityAcute);

  if (!waitInterruptible(
        750,
        generationAtStart
      )) {
    return;
  }

  stopAllMotors();
  Serial.println(
    "Rechtsabbieger-Sequenz beendet"
  );
}

// ------------------------------------------------------------
// BEFEHLSVERARBEITUNG
// ------------------------------------------------------------

void executeCommand(
  uint8_t scenario,
  uint8_t side
) {
  stopAllMotors();

  const uint32_t generationAtStart =
    commandGeneration;

  Serial.println();
  Serial.print("Szenario empfangen: ");
  Serial.print(scenario);
  Serial.print(" | Seite: ");
  Serial.println(side);

  switch (scenario) {
    case SCENARIO_STOP:
      Serial.println("STOP");
      stopAllMotors();
      break;

    case SCENARIO_MOTOR_TEST:
      playMotorTest(generationAtStart);
      break;

    case SCENARIO_OVERTAKE:
      handleOvertake(
        side,
        generationAtStart
      );
      break;

    case SCENARIO_DOORING:
      handleDooring(
        side,
        generationAtStart
      );
      break;

    case SCENARIO_RIGHT_TURN:
      playRightTurn(generationAtStart);
      break;

    default:
      Serial.println("Unbekanntes Szenario");
      stopAllMotors();
      break;
  }
}

// ------------------------------------------------------------
// ESP-NOW-EMPFANG
// ------------------------------------------------------------

void onDataReceived(
  const esp_now_recv_info_t *info,
  const uint8_t *data,
  int dataLength
) {
  if (dataLength != sizeof(ControlMessage)) {
    return;
  }

  ControlMessage message;
  memcpy(&message, data, sizeof(message));

  receivedScenario = message.scenario;
  receivedSide = message.side;

  commandGeneration++;
  newCommandAvailable = true;
}

// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);

  for (uint8_t i = 0; i < motorCount; i++) {
    if (!ledcAttach(
          motors[i],
          pwmFrequency,
          pwmResolution
        )) {
      Serial.print("PWM-Fehler an GPIO ");
      Serial.println(motors[i]);
    }

    ledcWrite(motors[i], 0);
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(
    espNowChannel,
    WIFI_SECOND_CHAN_NONE
  );

  Serial.println();
  Serial.println("Linker Griff startet");

  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.macAddress());

  Serial.print("ESP-NOW-Kanal: ");
  Serial.println(espNowChannel);

  if (esp_now_init() != ESP_OK) {
    Serial.println(
      "ESP-NOW konnte nicht gestartet werden"
    );

    stopAllMotors();
    return;
  }

  esp_now_register_recv_cb(onDataReceived);

  Serial.println("ESP-NOW bereit");
  Serial.println(
    "Warte auf Befehle des Steuer-ESP"
  );
}

// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop() {
  if (!newCommandAvailable) {
    delay(5);
    return;
  }

  noInterrupts();

  const uint8_t scenario = receivedScenario;
  const uint8_t side = receivedSide;

  newCommandAvailable = false;

  interrupts();

  executeCommand(scenario, side);
}