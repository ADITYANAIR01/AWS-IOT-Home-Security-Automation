# ESP32 Setup on Windows — Certs + LittleFS Upload

The ESP32 firmware loads its AWS IoT certificates from the **LittleFS flash partition**, not from the sketch itself. Flashing the `.ino` alone is not enough — you must also upload the filesystem image. This guide covers the full process on Windows.

## Why this is needed

```
ERROR: cannot open /AmazonRootCA1.pem
ERROR: missing cert files in LittleFS
```

This error in Serial Monitor means the sketch is running but the cert files are not in flash. The fix: put the 3 cert files in `aws-esp32\data\` and upload the LittleFS image.

## Step 1 — Copy the certificate files

The cert files are **gitignored** (they're secrets), so they are NOT in the repo. You must copy them manually to the Windows machine from wherever you ran the AWS setup.

Create this folder next to the sketch:

```
<repo>\aws-esp32\data\
```

Copy these 3 files into it:

| File | Source (on your other machine) |
|---|---|
| `AmazonRootCA1.pem` | `certs/AmazonRootCA1.pem` |
| `device.pem.crt` | `certs/device.pem.crt` |
| `private.pem.key` | `certs/private.pem.key` |

Final structure:

```
aws-esp32\
├── aws-esp32.ino
├── secrets.h          (WiFi credentials — create if missing)
└── data\              <-- NEW, required for LittleFS upload
    ├── AmazonRootCA1.pem
    ├── device.pem.crt
    └── private.pem.key
```

> ⚠️ Transfer these over USB drive, email-to-self, or scp — never commit them to git.

If `secrets.h` doesn't exist on the Windows machine either, create it in `aws-esp32\`:

```c
#pragma once
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

## Step 2 — Install the LittleFS upload plugin (one-time)

Arduino IDE 2.x on Windows uses a VSIX plugin for filesystem uploads.

1. Download the latest **`.vsix`** file from:
   https://github.com/earlephilhower/arduino-littlefs-upload/releases
2. Create the plugins folder if it doesn't exist:
   ```
   C:\Users\<your-username>\.arduinoIDE\plugins\
   ```
3. Move the downloaded `.vsix` file into that folder
4. **Restart Arduino IDE** completely

You should now have an "Upload LittleFS" option in the Command Palette.

## Step 3 — Configure the IDE

1. Open `aws-esp32\aws-esp32.ino`
2. **Tools → Board** → your ESP32 board (e.g., "ESP32 Dev Module")
3. **Tools → Port** → your COM port (e.g., `COM5`)
4. **Tools → Partition Scheme** → keep whatever you used to flash the sketch (default **"Default 4MB with spiffs"** is fine)
   - ⚠️ The partition scheme used for the LittleFS upload **must match** the one used for the sketch, otherwise the firmware won't find the files

## Step 4 — Close the Serial Monitor

**Close the Serial Monitor before uploading.** The plugin cannot access the COM port while the monitor holds it open — you'll get:

```
A fatal error occurred: Could not open <serial port>
```

## Step 5 — Upload the LittleFS image

1. Press **Ctrl + Shift + P** (Command Palette)
2. Type: **Upload LittleFS to Pico/ESP8266/ESP32**
3. Press Enter
4. Wait for: **`LittleFS Image Uploaded`**

## Step 6 — Verify

1. Reopen Serial Monitor (**9600 baud**)
2. You should see:

```
Loaded /AmazonRootCA1.pem (1188 bytes)
Loaded /device.pem.crt (1220 bytes)
Loaded /private.pem.key (1675 bytes)
Connecting to Wi-Fi......
Connected to Wi-Fi
192.168.x.x
Connecting to AWS IoT Core...connected
Publishing: {"device":"home-security-esp32","water_level":45,...}
```

## Troubleshooting

| Symptom | Fix |
|---|---|
| `Cannot open serial port` during upload | Close Serial Monitor, unplug/replug ESP32, check COM port in Tools → Port |
| Upload fails right after installing the plugin | Known IDE glitch: change Board to any other board, change it back, retry |
| Still get `cannot open /AmazonRootCA1.pem` after upload | Partition scheme mismatch between sketch flash and LittleFS upload — use the same scheme for both, reflash sketch, re-upload FS |
| `Permission denied` / port busy | Another program (Serial Monitor, another IDE, ESP32 tooling) is holding the COM port — close it |
| WiFi connects but MQTT never connects | Serial Monitor will print `MQTT error` / `rc=` codes — check that `secrets.h` WiFi is 2.4GHz (ESP32 can't use 5GHz) |

## Note

The LittleFS upload **survives sketch re-flashes** (different flash region), so you only need to redo Step 5 if you change certificates or erase flash. If you ever use "Erase Flash" in the IDE, you must redo Steps 1, 5 (certs are wiped).
