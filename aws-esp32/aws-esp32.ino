#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include "secrets.h" // WiFi credentials (gitignored)

/* ============================================================
 * AWS IoT Home Security — ESP32 Gateway
 * Reads 9-field CSV sensor data from Arduino over Serial2,
 * publishes JSON to AWS IoT Core over MQTT/TLS (port 8883).
 *
 * SETUP (one-time):
 * 1. Install library: "PubSubClient" by Nick O'Leary
 * 2. Copy the 4 files from certs/ into aws-esp32/data/ :
 *      AmazonRootCA1.pem, device.pem.crt, private.pem.key
 * 3. Upload the LittleFS filesystem image:
 *      Arduino IDE: Sketch > ESP32 LittleFS Data Upload
 *      arduino-cli: arduino-cli upload -p PORT --fqbn esp32:esp32:esp32 \
 *                   -i filesystem --input-dir aws-esp32/data
 * 4. Fill in secrets.h, then flash this sketch.
 *
 * WIRING: Arduino TX (pin 12) -> ESP32 GPIO16 (RXD2)
 * ============================================================ */

// --- AWS IoT Core (see cloud-setup-outputs.md) ---
#define AWS_IOT_ENDPOINT "adndeuo6w4mnl-ats.iot.ap-south-1.amazonaws.com"
#define AWS_IOT_PORT 8883
#define MQTT_TOPIC "home/security/sensors"
#define CLIENT_ID "home-security-esp32" // must match the iot:Connect policy

// Cert files in LittleFS root
#define CA_PATH   "/AmazonRootCA1.pem"
#define CERT_PATH "/device.pem.crt"
#define KEY_PATH  "/private.pem.key"

// Serial2 pins for Arduino communication
#define RXD2 16
#define TXD2 17

// Cert buffers (PEM files are ~2-4 KB each)
char caCert[4096];
char deviceCert[4096];
char privateKey[4096];

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);

// Read a PEM file from LittleFS into a buffer
bool loadFile(const char *path, char *buffer, size_t maxLen) {
  File f = LittleFS.open(path, "r");
  if (!f || f.isDirectory()) {
    Serial.println(String("ERROR: cannot open ") + path);
    return false;
  }
  size_t n = f.readBytes(buffer, maxLen - 1);
  buffer[n] = '\0';
  f.close();
  Serial.println(String("Loaded ") + path + " (" + n + " bytes)");
  return n > 0;
}

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to AWS IoT Core...");
    // Client ID must match the policy: client/home-security-esp32
    if (mqtt.connect(CLIENT_ID)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" — retrying in 5s");
      delay(5000);
    }
  }
}

// Parse 9-value CSV from Arduino and publish as JSON
void handleSensorData(String data) {
  data.trim();

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
    Serial.println("Invalid data format - expected 9 values, got: " + data);
    return;
  }

  // Build JSON (water_level and light are numbers, rest are strings)
  String json = "{\"device\":\"" CLIENT_ID "\"";
  json += ",\"water_level\":" + values[0];
  json += ",\"motion\":\"" + values[1] + "\"";
  json += ",\"fire\":\"" + values[2] + "\"";
  json += ",\"lpg\":\"" + values[3] + "\"";
  json += ",\"ir_sensor\":\"" + values[4] + "\"";
  json += ",\"tilt\":\"" + values[5] + "\"";
  json += ",\"vibration\":\"" + values[6] + "\"";
  json += ",\"light\":" + values[7];
  json += ",\"rain\":\"" + values[8] + "\"";
  json += ",\"uptime\":" + String(millis() / 1000);
  json += "}";

  Serial.println("Publishing: " + json);
  // retain=true so the dashboard instantly shows the last known state
  if (!mqtt.publish(MQTT_TOPIC, json.c_str(), true)) {
    Serial.println("Publish failed");
  }
}

void setup() {
  Serial.begin(9600);  // ESP32 Serial Monitor
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // Arduino communication

  // Load certificates from LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: LittleFS mount failed — halt");
    while (true) delay(1000);
  }
  if (!loadFile(CA_PATH, caCert, sizeof(caCert)) ||
      !loadFile(CERT_PATH, deviceCert, sizeof(deviceCert)) ||
      !loadFile(KEY_PATH, privateKey, sizeof(privateKey))) {
    Serial.println("ERROR: missing cert files in LittleFS — see file header for setup steps");
    while (true) delay(1000);
  }

  // TLS configuration
  secureClient.setCACert(caCert);
  secureClient.setCertificate(deviceCert);
  secureClient.setPrivateKey(privateKey);

  connectWiFi();

  mqtt.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
  mqtt.setBufferSize(512); // default 256 is too small for our JSON
  connectMQTT();
}

void loop() {
  // Reconnect WiFi if dropped
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost — reconnecting");
    connectWiFi();
  }

  // Maintain MQTT connection
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  // Read sensor data from Arduino and publish
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    if (data.length() > 0) {
      Serial.println("Raw data received: " + data);
      handleSensorData(data);
    }
  }

  delay(100); // avoid overwhelming the loop
}
