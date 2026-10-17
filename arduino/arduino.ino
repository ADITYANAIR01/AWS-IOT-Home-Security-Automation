#include <SoftwareSerial.h>

// Pin definitions
#define FIRE_PIN 3
#define IR_PIN 4
#define VIBRATION_PIN 5
#define MOTION_PIN 6
#define TRIG_PIN 7
#define ECHO_PIN 8
#define TILT_PIN 9
#define RX_PIN 0
#define TX_PIN 12
#define LDR_PIN A1
#define LPG_PIN A2
#define RAIN_PIN A3

#define LPG_THRESHOLD 130
const int RAIN_THRESHOLD = 750;       // Light rain threshold
const int HEAVY_RAIN_THRESHOLD = 500; // Heavy rain threshold
const int SAMPLE_COUNT = 10;          // Number of samples for rain sensor averaging

// Arduino TX (pin 12) -> ESP32 RXD2 (GPIO 16)
SoftwareSerial esp(RX_PIN, TX_PIN);

String waterLevel, fireStatus, motionStatus, lpgStatus, irSensorStatus, tiltStatus, vibrationStatus, lightStatus, rainStatus;
long duration;
int distanceCm, distancePer;

void setup() {
  Serial.begin(9600); // For Serial Monitor
  esp.begin(9600);    // For ESP32 communication

  pinMode(FIRE_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(LPG_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TILT_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
}

// Function to get averaged rain sensor reading
int getAveragedReading() {
  long total = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    total += analogRead(RAIN_PIN);
    delay(100); // Small delay between samples
  }
  return total / SAMPLE_COUNT; // Return averaged value
}

// Function to determine rain status
void setRainStatus(int reading) {
  if (reading >= RAIN_THRESHOLD) {
    rainStatus = "No_Rain";
  } else if (reading >= HEAVY_RAIN_THRESHOLD) {
    rainStatus = "Light_Rain";
  } else {
    rainStatus = "Heavy_Rain";
  }
}

void loop() {
  // Water Level (Ultrasonic)
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distanceCm = duration * 0.034 / 2;
  distancePer = map(distanceCm, 10, 0, 0, 100);
  distancePer = constrain(distancePer, 0, 100);
  waterLevel = String(distancePer);

  // Fire Status
  fireStatus = (digitalRead(FIRE_PIN) == LOW) ? "Fire_Detected" : "No_Fire";

  // Motion Status
  motionStatus = (digitalRead(MOTION_PIN) == HIGH) ? "Motion_Detected" : "No_Motion";

  // IR Sensor Status
  irSensorStatus = (digitalRead(IR_PIN) == LOW) ? "Door_is_Closed" : "Door_is_Opened";

  // LPG Status
  int gasValue = analogRead(LPG_PIN);
  lpgStatus = (gasValue <= LPG_THRESHOLD) ? "LPG_Leak_Detected" : "LPG_No_Leakage";

  // Tilt Sensor
  tiltStatus = (digitalRead(TILT_PIN) == LOW) ? "Tilt_Detected" : "No_Tilt";

  // Vibration Sensor
  vibrationStatus = (digitalRead(VIBRATION_PIN) == HIGH) ? "Vibration_Detected" : "No_Vibration";

  // Light Sensor (LDR)
  int lightValue = analogRead(LDR_PIN);
  lightStatus = String(map(lightValue, 0, 1023, 0, 100));

  // Rain Sensor
  int rainReading = getAveragedReading();
  setRainStatus(rainReading);

  // Print to Arduino Serial Monitor
  Serial.println("--- Sensor Readings ---");
  Serial.println("Water Level: " + waterLevel + "%");
  Serial.println("Motion: " + motionStatus);
  Serial.println("Fire: " + fireStatus);
  Serial.println("LPG: " + lpgStatus);
  Serial.println("IR Sensor: " + irSensorStatus);
  Serial.println("Tilt: " + tiltStatus);
  Serial.println("Vibration: " + vibrationStatus);
  Serial.println("Light: " + lightStatus + "%");
  Serial.println("Rain: " + rainStatus);
  Serial.println("-----------------------");

  // Send data to ESP32 (9 comma-separated values)
  String data = waterLevel + "," + motionStatus + "," +
                fireStatus + "," + lpgStatus + "," + irSensorStatus + "," +
                tiltStatus + "," + vibrationStatus + "," + lightStatus + "," +
                rainStatus + "\n";
  esp.print(data);

  delay(5000); // Update every 5 seconds
}
