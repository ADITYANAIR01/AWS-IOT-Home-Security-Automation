# AWS IoT Home Security Automation

A full-stack IoT system for real-time home monitoring. Sensor data is collected by an Arduino, transmitted to an ESP32 gateway, published to **AWS IoT Core** over MQTT/TLS, stored in **DynamoDB**, visualized live on a web dashboard via **MQTT-over-WebSocket**, and critical events (fire/LPG leak) trigger **SNS** email alerts.

## Architecture

```
                                    ┌─────────────────────────────┐
Arduino ──Serial──► ESP32 ──MQTT──► │  AWS IoT Core               │
                  (TLS, X.509)      │  topic: home/security/sensors│
                                    └──────┬──────────┬───────────┘
                                           │          │ (fire/LPG filter rule)
                                    DynamoDB    SNS alert (email)
                                    (history)       │
                                           ▲    adityanairtech@gmail.com
                                           │
Browser ──guest auth (Cognito)──► Dashboard ──MQTT/WSS──► AWS IoT Core (live data)
(S3 + CloudFront: https://iot.adityanair.tech)
```

## Repository layout

| Path | Purpose |
|---|---|
| `arduino/arduino.ino` | Reads 9 sensors, sends CSV over Serial to ESP32 |
| `aws-esp32/aws-esp32.ino` | ESP32 gateway: CSV → JSON → AWS IoT Core (MQTT/TLS) |
| `aws-esp32/secrets.h` | WiFi credentials (gitignored — create from template below) |
| `dashboard/index.html` | Web dashboard (S3 + CloudFront hosted) |
| `certs/` | Device X.509 certificates (gitignored) |
| `cloud-setup-outputs.md` | All AWS resource IDs/ARNs from the CLI setup |

## Sensors

| Sensor | Pin | Reported as |
|---|---|---|
| Ultrasonic (water level) | D7/D8 | `water_level` (0–100%) |
| Flame | D3 | `fire` (`Fire_Detected` / `No_Fire`) |
| PIR motion | D6 | `motion` |
| IR (door) | D4 | `ir_sensor` |
| MQ-series gas (LPG) | A2 | `lpg` (`LPG_Leak_Detected` / `LPG_No_Leakage`) |
| Tilt (window) | D9 | `tilt` |
| Vibration | D5 | `vibration` |
| LDR (light) | A1 | `light` (0–100%) |
| Rain | A3 | `rain` (`No_Rain` / `Light_Rain` / `Heavy_Rain`) |

Arduino → ESP32 wiring: **Arduino pin 12 (TX) → ESP32 GPIO 16 (RXD2)**, common GND, 9600 baud.

## AWS resources (region `ap-south-1`)

| Resource | Name / ID |
|---|---|
| IoT Thing | `home-security-esp32` |
| IoT endpoint | `adndeuo6w4mnl-ats.iot.ap-south-1.amazonaws.com` |
| MQTT topic | `home/security/sensors` (JSON, retained) |
| DynamoDB table | `SensorReadings` (PK `deviceId`, SK `timestamp`, on-demand) |
| SNS topic | `home-security-alerts` (email alerts for fire/LPG) |
| IoT rule: persist | `home_security_persist` → DynamoDB |
| IoT rule: alert | `home_security_alert` (fire/LPG filter) → SNS |
| Cognito Identity Pool | guest (unauthenticated) access — dashboard connect auth |
| S3 bucket | `iot-adityanair-tech-dashboard` (private, CloudFront OAC only) |
| CloudFront | `EHTL2WMRZT8XW` → `d2quyry4cgda3q.cloudfront.net` + alias `iot.adityanair.tech` |
| ACM cert | `iot.adityanair.tech` (us-east-1, DNS validated) |

Full IDs/ARNs: see [`cloud-setup-outputs.md`](cloud-setup-outputs.md).

## Setup from scratch

### 1. AWS CLI provisioning

All resources were created with CLI commands. To reproduce (or on a new account):

```bash
export AWS_REGION=ap-south-1
ACCOUNT=$(aws sts get-caller-identity --query Account --output text)

# --- IoT thing + certificate ---
aws iot create-thing --thing-name home-security-esp32
aws iot create-keys-and-certificate --set-as-active \
  --certificate-pem-outfile certs/device.pem.crt \
  --public-key-outfile certs/public.pem.key \
  --private-key-outfile certs/private.pem.key
# attach: aws iot attach-thing-principal / attach-policy (see cloud-setup-outputs.md)
curl -o certs/AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem

# --- Storage + alerts ---
aws dynamodb create-table --table-name SensorReadings \
  --attribute-definitions AttributeName=deviceId,AttributeType=S AttributeName=timestamp,AttributeType=N \
  --key-schema AttributeName=deviceId,KeyType=HASH AttributeName=timestamp,KeyType=RANGE \
  --billing-mode PAY_PER_REQUEST
aws sns create-topic --name home-security-alerts
aws sns subscribe --topic-arn <TOPIC_ARN> --protocol email --notification-endpoint <YOUR_EMAIL>
# ⚠️ click the SNS confirmation link in your inbox

# --- IoT rules (persist to DynamoDB, alert via SNS) ---
# payloads documented in git history / cloud-setup-outputs.md

# --- Dashboard auth: Cognito Identity Pool, guest access only ---
aws cognito-identity create-identity-pool --identity-pool-name home-security-guest \
  --allow-unauthenticated-identities
# guest role: iot:Connect, iot:Subscribe (topicfilter), iot:Receive on the sensor topic
```

### 2. ESP32 firmware

1. Install the **PubSubClient** library (Nick O'Leary) via Library Manager
2. Copy certificates into the LittleFS data folder:
   ```bash
   mkdir -p aws-esp32/data
   cp certs/AmazonRootCA1.pem certs/device.pem.crt certs/private.pem.key aws-esp32/data/
   ```
3. Upload the filesystem image:
   - Arduino IDE: *Sketch → ESP32 LittleFS Data Upload* (requires the "ESP32 LittleFS Data Upload" plugin), or
   - CLI: `arduino-cli upload -p PORT --fqbn esp32:esp32:esp32 -i filesystem --input-dir aws-esp32/data`
4. Create `aws-esp32/secrets.h`:
   ```c
   #pragma once
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   ```
5. Flash `aws-esp32/aws-esp32.ino`. Serial monitor (9600) shows:
   ```
   Loaded /AmazonRootCA1.pem ...
   Connected to Wi-Fi
   Connecting to AWS IoT Core...connected
   Publishing: {"device":"home-security-esp32","water_level":45,...}
   ```

### 3. Arduino

Flash `arduino/arduino.ino` as-is (no libraries beyond built-in SoftwareSerial). It streams 9 comma-separated values every 5 s to the ESP32.

### 4. Dashboard

```bash
aws s3 cp dashboard/index.html s3://iot-adityanair-tech-dashboard/index.html --content-type text/html
aws cloudfront create-invalidation --distribution-id EHTL2WMRZT8XW --paths "/*"
```

DNS (at your registrar, not Route 53):

| Type | Name | Value |
|---|---|---|
| CNAME | `_ff9256473c4b802d6aaf38e0cffb822a.iot` | `_537ea43d1a4d19fa65a4401a4b7466a7.wzccmgtwzk.acm-validations.aws.` (ACM validation — already completed) |
| CNAME | `iot` | `d2quyry4cgda3q.cloudfront.net` |

The dashboard connects as a **guest** via the Cognito Identity Pool, signs a WebSocket URL with SigV4 (Web Crypto API, no AWS SDK needed), and subscribes to `home/security/sensors`. All UI behavior (fire/rain popups, rain animation, window flip, seismograph, dark mode, charts) matches the original Firebase dashboard.

## Security notes

- Device auth is **X.509 certificate**-based; certs live in ESP32 LittleFS flash and `certs/` (gitignored) — never in source
- The IoT device policy allows only: connect as client `home-security-esp32`, publish to `home/security/sensors`
- The dashboard guest role is **read-only** (connect/subscribe/receive) — visitors cannot publish or modify anything
- Dashboard is intentionally **open access** for demonstration. To lock it down:
  ```bash
  aws cognito-identity update-identity-pool \
    --identity-pool-id ap-south-1:daac6d6a-4c03-441a-bb3c-a16f24f478f3 \
    --identity-pool-name home-security-guest \
    --allow-unauthenticated-identities   # remove this flag to disable guest access
  ```
- S3 bucket is fully private; only CloudFront (via OAC) can read it. Site is HTTPS-only with TLS 1.2+

## Cost

Everything runs within AWS free tier or a few cents/month at hobby scale: IoT Core messaging (free tier: 250 min connectivity/month... device + 1 dashboard ≈ 730+730 min ≈ just over free tier, ~$0.10/month), DynamoDB on-demand (free tier covers it), SNS ($0 for first 1,000 emails), S3+CloudFront (free tier covers it), Cognito (free tier covers it).
