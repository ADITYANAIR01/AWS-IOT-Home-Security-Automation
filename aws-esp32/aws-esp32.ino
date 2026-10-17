#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h" // WiFi credentials (gitignored) — create from secrets.h.example

// Serial2 pins for Arduino communication
#define RXD2 16
#define TXD2 17

String uid;

void setup() {
  Serial.begin(9600);  // For ESP32 Serial Monitor
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // For Arduino communication

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.println(WiFi.localIP());

  // TODO(migration): initialize AWS IoT Core MQTT client here (X.509 certs over TLS)
}

void uploadToCloud(String data) {
  // Check WiFi is still connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected — reconnecting");
    WiFi.reconnect();
    return;
  }

  // Remove trailing newline or spaces
  data.trim();

  // Parse the string into 9 values
  String values[9];
  int index = 0;
  int commaIndex;
  while ((commaIndex = data.indexOf(',')) != -1 && index < 9) {
    values[index] = data.substring(0, commaIndex);
    data = data.substring(commaIndex + 1);
    index++;
  }
  if (index == 8 && data.length() > 0) {
    values[8] = data;
  } else {
    Serial.println("Invalid data format - Expected 9 values. Received: " + String(index + 1));
    Serial.println("Raw data: " + data);
    return;
  }

  // Debugging: Print received values
  Serial.println("--- Received Sensor Data ---");
  Serial.println("Water Level: " + values[0]);
  Serial.println("Motion: " + values[1]);
  Serial.println("Fire: " + values[2]);
  Serial.println("LPG: " + values[3]);
  Serial.println("IR Sensor: " + values[4]);
  Serial.println("Tilt: " + values[5]);
  Serial.println("Vibration: " + values[6]);
  Serial.println("Light: " + values[7]);
  Serial.println("Rain: " + values[8]);
  Serial.println("----------------------------");

  // TODO(migration): publish as JSON to AWS IoT Core MQTT topic,
  // e.g. topic "home/security/sensors" with payload:
  // {"water_level":N,"motion":"...","fire":"...","lpg":"...","ir_sensor":"...",
  //  "tilt":"...","vibration":"...","light":N,"rain":"...","timestamp":...}
}

void loop() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    if (data.length() > 0) {
      Serial.println("Raw data received: " + data);
      uploadToCloud(data);
    }
  }

  delay(100); // Small delay to avoid overwhelming the loop
}
