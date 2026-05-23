# Bike Anti-Theft System 🚨🏍️

A GSM + GPS based smart anti-theft system for motorcycles using an ESP32, SIM800L, and NEO-6M GPS module. The system allows the owner to remotely track and control the bike using SMS commands.

---

## Features

- 📍 Live GPS location tracking via SMS
- 🔒 Remote engine immobilization using relay
- 📲 SMS-based control system
- 🛰️ Google Maps location link support
- ⚡ Built using ESP32 dual UART capability
- 🔄 Real-time GPS parsing using NMEA sentences
- 🛡️ Anti-theft protection without internet dependency

---

## Hardware Used

| Component | Description |
|---|---|
| ESP32 38-pin | Main microcontroller |
| SIM800L | GSM module for SMS communication |
| NEO-6M GPS | GPS module for location tracking |
| Relay Module (CW-020) | Ignition control |
| 12V Bike Battery | Power source |
| Buck Converter | 12V → 5V/4V regulation |

---

## Pin Connections

| Module | ESP32 Pin |
|---|---|
| SIM800L TXD | GPIO16 (RX2) |
| SIM800L RXD | GPIO17 (TX2) |
| GPS TX | GPIO4 |
| Relay IN | GPIO23 |

---

## SMS Commands

| Command | Action |
|---|---|
| `GPS` | Sends live bike location |
| `STOP` | Turns OFF bike ignition |
| `START` | Restores bike ignition |

---

## Example Responses

### GPS Request

User sends:

```text
GPS
```

System replies:

```text
Bike location:
https://maps.google.com/?q=18.520430,73.856743
```

---

### Stop Bike

User sends:

```text
STOP
```

System replies:

```text
Bike ignition OFF. Relay activated.
```

---

### Start Bike

User sends:

```text
START
```

System replies:

```text
Bike ignition ON. Relay deactivated.
```

---

## How It Works

1. ESP32 initializes the SIM800L and GPS module.
2. SIM800L continuously listens for incoming SMS commands.
3. GPS module streams NMEA data to ESP32.
4. ESP32 extracts latitude and longitude from `$GPRMC` sentences.
5. Depending on the SMS command:
   - Sends GPS location
   - Cuts ignition using relay
   - Restores ignition

---

## GPS Parsing

The project parses `$GPRMC` / `$GNRMC` NMEA sentences and converts coordinates from:

```text
DDDMM.MMMM
```

to decimal degrees format.

Example:

```text
1825.2260 → 18.420433°
```

---

## Relay Logic

| Relay State | Bike Status |
|---|---|
| HIGH | Ignition ON |
| LOW | Ignition CUT |

---

## Software Requirements

- Arduino IDE
- ESP32 Board Package
- USB Drivers for ESP32

---

## Installation

### 1. Install ESP32 Board

Arduino IDE → Preferences → Additional Board URLs:

```text
https://dl.espressif.com/dl/package_esp32_index.json
```

Then:

```text
Tools → Board Manager → ESP32 → Install
```

---

### 2. Upload Code

1. Open Arduino IDE
2. Select:
   - Board: `ESP32 Dev Module`
   - Correct COM Port
3. Upload the sketch

---

## Serial Monitor

Set baud rate:

```text
115200
```

Expected startup logs:

```text
Booting — waiting for SIM800L...
SIM800L initialized.
System ready. Bike: ON
```

---

## Project Structure

```text
bike-anti-theft-system/
│
├── bike_anti_theft.ino
├── README.md
└── circuit_diagram.png
```

---

## Future Improvements

- Mobile app integration
- Call alerts on theft detection
- Vibration sensor trigger
- Geo-fencing support
- Real-time cloud tracking
- Battery backup system

---

## Safety Notes

- Use proper voltage regulation for SIM800L.
- SIM800L requires high current bursts (~2A).
- Do not directly power SIM800L from ESP32 3.3V pin.
- Test relay wiring carefully before connecting to vehicle ignition.

---

## License

This project is open-source and free to use for educational purposes.

---

## Author

Developed by **Shreyas Shivale** 🚀