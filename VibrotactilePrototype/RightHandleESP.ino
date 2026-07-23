#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

/*
  ============================================================
  RECHTER HANDGRIFF – ESP-NOW-EMPFÄNGER
  ============================================================

  Motoren:
  R1 = GPIO 3
  R2 = GPIO 2
  R3 = GPIO 1
  R4 = GPIO 0
  R5 = GPIO 5

  Der Steuer-ESP sendet:
  - Szenario
  - Gefahrenseite

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

const uint8_t motors[] = {26, 27, 25, 12, 13};
const uint8_t motorCount = sizeof(motors) / sizeof(motors[0]);

// Muss später zur räumlichen Reihenfolge im 3D-Griff passen:
// motors[0] = R1
// motors[1] = R2
// motors[2] = R3
// motors[3] = R4
// motors[4] = R5

const uint32_t pwmFrequency = 5000;
const uint8_t pwmResolution = 8;

// PWM-Bereich: 0 bis 255
// Diese Werte sind Startwerte und müssen am echten Griff kalibriert werden.
const uint8_t intensityLow = 255;
const uint8_t intensityMedium = 255;
const uint8_t intensityHigh = 255;
const uint8_t intensityAcute = 255;

// Alle drei Annäherungsstufen dauern gleich lang.
const unsigned long approachStageDuration = 1800;

// Konstantes Pulsmuster.
// Für die Eskalation wird zunächst nur die Intensität verändert.
const unsigned long pulseOnTime = 250;
const unsigned long pulseOffTime = 400;

// ESP-NOW und Webinterface müssen denselben WLAN-Kanal verwenden.
const uint8_t espNowChannel = 6;

// ------------------------------------------------------------
// BEFEHLE DES STEUER-ESP
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

// Diese Struktur muss später im Steuer-ESP identisch sein.
struct ControlMessage {
  uint8_t scenario;
  uint8_t side;
};

// Der Callback setzt nur den neuen Befehl.
// Die Vibrationslogik läuft anschließend im normalen loop().
volatile bool newCommandAvailable = false;
volatile uint8_t receivedScenario = SCENARIO_STOP;
volatile uint8_t receivedSide = SIDE_NONE;

// Wird erhöht, sobald ein neuer Befehl eintrifft.
// Damit lassen sich laufende Muster sofort unterbrechen.
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

// Aktiviert nur R4 und R5.
// Diese Positionen sind als Handballenbereich vorgesehen.
void setDooringMotors(uint8_t intensity) {
  stopAllMotors();

  setMotor(3, intensity);  // R4
  setMotor(4, intensity);  // R5
}

// Wartet, kann aber sofort durch einen neuen ESP-NOW-Befehl abbrechen.
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
  Serial.println("Motortest: R1 bis R5");

  for (uint8_t i = 0; i < motorCount; i++) {
    Serial.print("Motor R");
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
  Das Phi-Muster stellt räumliche Bewegung dar.

  Der Ablauf ist:
  R1 → R2 → R3 → R4 → R5

  Die Geschwindigkeit der Welle bleibt konstant.
  Nur die Intensität steigt mit der Annäherung.

  Der rechte Griff spielt diese Welle nur bei einem Fahrzeug,
  das rechts überholt.

  Beim normalen Überholen auf der linken Seite bleibt der
  rechte Griff ruhig. Die linke Welle läuft dann auf dem
  linken Griff.
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

void playOvertakeRight(uint32_t generationAtStart) {
  Serial.println("Überholen rechts: automatische Annäherung");

  Serial.println("Stufe 1: weit entfernt");
  if (!playPhiStage(
        intensityLow,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 2: kommt näher");
  if (!playPhiStage(
        intensityMedium,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Stufe 3: sehr nah");
  if (!playPhiStage(
        intensityHigh,
        approachStageDuration,
        generationAtStart
      )) {
    return;
  }

  Serial.println("Überholvorgang unmittelbar am Fahrrad");

  // Noch einmal eine sehr deutliche Welle,
  // jedoch kein langer Vollalarm.
  if (!playPhiWave(intensityAcute, generationAtStart)) {
    return;
  }

  stopAllMotors();
  Serial.println("Überholsequenz beendet");
}

void handleOvertake(
  uint8_t side,
  uint32_t generationAtStart
) {
  if (side == SIDE_RIGHT) {
    playOvertakeRight(generationAtStart);
    return;
  }

  if (side == SIDE_LEFT) {
    Serial.println(
      "Überholen links: rechter Griff bleibt ruhig"
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
  Dooring bedeutet hier NICHT:
  "Die Tür öffnet sich gerade."

  Es bedeutet:
  "Du fährst an parkenden Fahrzeugen entlang.
   Sei aufmerksam – hier könnte eine Tür geöffnet werden."

  Deshalb:
  - keine Phi-Welle
  - kein Maximalalarm
  - kurze, sanfte Präsenzimpulse
  - nur auf der Seite der parkenden Autos

  Dooring rechts:
  R4 und R5 am rechten Griff pulsieren.

  Dooring links:
  rechter Griff bleibt ruhig;
  der linke Griff übernimmt die Warnung.
*/

void playDooringRight(uint32_t generationAtStart) {
  Serial.println("Dooring rechts: Präsenzwarnung aktiv");

  while (commandGeneration == generationAtStart) {
    setDooringMotors(intensityLow);

    if (!waitInterruptible(220, generationAtStart)) {
      return;
    }

    stopAllMotors();

    if (!waitInterruptible(850, generationAtStart)) {
      return;
    }
  }
}

void handleDooring(
  uint8_t side,
  uint32_t generationAtStart
) {
  if (side == SIDE_RIGHT) {
    playDooringRight(generationAtStart);
    return;
  }

  if (side == SIDE_LEFT) {
    Serial.println(
      "Dooring links: rechter Griff bleibt ruhig"
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
  Beim Rechtsabbieger wächst eine konkrete Konfliktgefahr.

  Das Phi-Phänomen wird nicht verwendet, weil keine Bewegung
  entlang der Hand dargestellt werden muss.

  Alle Motoren pulsieren gleichzeitig.
  Die Annäherung läuft automatisch:

  schwach → mittel → stark → kurzer Akutimpuls

  Pulsdauer und Pausen bleiben konstant.
  Primär steigt die Amplitude.
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

void playAppThreat(
  uint8_t threatLevel,
  uint32_t generationAtStart
) {
  if (threatLevel == 1) {
    Serial.println("App-Warnung: APPROACHING");

    while (commandGeneration == generationAtStart) {
      setAllMotors(intensityLow);

      if (!waitInterruptible(220, generationAtStart)) {
        return;
      }

      stopAllMotors();

      if (!waitInterruptible(780, generationAtStart)) {
        return;
      }
    }

    return;
  }

  if (threatLevel == 2) {
    Serial.println("App-Warnung: DANGER");

    while (commandGeneration == generationAtStart) {
      setAllMotors(intensityHigh);

      if (!waitInterruptible(360, generationAtStart)) {
        return;
      }

      stopAllMotors();

      if (!waitInterruptible(180, generationAtStart)) {
        return;
      }
    }

    return;
  }

  Serial.println("App-Warnung: unbekannte Stufe");
  stopAllMotors();
}

void playRightTurn(uint32_t generationAtStart) {
  Serial.println(
    "Rechtsabbieger: automatische Annäherung"
  );

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

  if (!waitInterruptible(750, generationAtStart)) {
    return;
  }

  stopAllMotors();
  Serial.println("Rechtsabbieger-Sequenz beendet");
}

// ------------------------------------------------------------
// BEFEHLSVERARBEITUNG
// ------------------------------------------------------------

void executeCommand(uint8_t scenario, uint8_t side) {
  stopAllMotors();

  // Aktuelle Befehlsversion merken.
  const uint32_t generationAtStart = commandGeneration;

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
      handleOvertake(side, generationAtStart);
      break;

    case SCENARIO_DOORING:
      handleDooring(side, generationAtStart);
      break;

    case SCENARIO_RIGHT_TURN:
      playRightTurn(generationAtStart);
      break;

    case SCENARIO_APP_THREAT:
      playAppThreat(side, generationAtStart);
      break;

    default:
      Serial.println("Unbekanntes Szenario");
      stopAllMotors();
      break;
  }
}

// ------------------------------------------------------------
// ESP-NOW EMPFANG
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

  // Motoren/PWM initialisieren
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

  // Alle drei ESPs müssen denselben WLAN-Kanal verwenden.
  esp_wifi_set_channel(
    espNowChannel,
    WIFI_SECOND_CHAN_NONE
  );

  Serial.println();
  Serial.println("Rechter Griff startet");
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.macAddress());
  Serial.print("ESP-NOW-Kanal: ");
  Serial.println(espNowChannel);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW konnte nicht gestartet werden");
    stopAllMotors();
    return;
  }

  esp_now_register_recv_cb(onDataReceived);

  Serial.println("ESP-NOW bereit");
  Serial.println("Warte auf Befehle des Steuer-ESP");
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
