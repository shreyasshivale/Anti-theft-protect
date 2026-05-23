#include <HardwareSerial.h>

// ── Pin definitions ─────────────────────────────────────────
#define RELAY_PIN     23     // Relay IN → GPIO23
#define GPS_RX_PIN    4      // GPS TX  → GPIO4
#define SIM_RX_PIN    16     // SIM TXD → GPIO16 (RX2)
#define SIM_TX_PIN    17     // SIM RXD → GPIO17 (TX2)
#define OWNER_NUMBER  "+91883XXXXXX" // Replace with your phone number

// ── Serial ports ────────────────────────────────────────────
HardwareSerial sim800(2);    // UART2 for SIM800L
HardwareSerial gpsSerial(1); // UART1 for GPS

// ── State ───────────────────────────────────────────────────
bool bikeEnabled    = true;
String simBuffer    = "";
String gpsBuffer    = "";
String lastLat      = "0.000000";
String lastLon      = "0.000000";
bool   gpsFix       = false;

// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // HIGH = bike ON (relay inactive)

  // Start GPS on UART1 (RX only — TX not needed)
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, -1);

  // Start SIM800L on UART2
  sim800.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

  Serial.println("Booting — waiting for SIM800L...");
  waitForBoot(10000);

  initSIM800();

  Serial.println("System ready. Bike: ON");
}

// ============================================================
void loop() {
  readGPS();   // Keep parsing GPS sentences every loop
  readSMS();   // Check if SIM800L pushed a new SMS
}

// ============================================================
//  waitForBoot()
//  Blocks until SIM800L sends "Call Ready" or timeout expires.
//  SIM800L boots and sends:  RDY -> +CFUN: 1 -> Call Ready
//  We must wait for "Call Ready" before sending AT commands.
// ============================================================
void waitForBoot(unsigned long timeoutMs) {
  String buf = "";
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (sim800.available()) {
      char c = sim800.read();
      buf += c;
      Serial.write(c); // show raw boot messages
    }
    if (buf.indexOf("Call Ready") != -1) {
      Serial.println("\nSIM800L ready!");
      delay(500);
      return;
    }
  }
  // Timeout — SIM800L may still work, continue anyway
  Serial.println("\nBoot timeout — continuing...");
}

// ============================================================
//  initSIM800()
//  Sends the three setup commands needed for SMS reception.
//  AT+CMGF=1  → switch to text mode (vs PDU mode)
//  AT+CNMI    → push new SMS directly to serial (no polling)
//  AT+CSCS    → set character set to GSM
// ============================================================
void initSIM800() {
  Serial.println("Initializing SIM800L...");

  sendAT("AT",               2000);  // wake / verify alive
  sendAT("AT+CMGF=1",        2000);  // SMS text mode
  sendAT("AT+CNMI=2,2,0,0,0",2000); // push SMS to serial
  sendAT("AT+CSCS=\"GSM\"",  1000);  // GSM character set

  Serial.println("SIM800L initialized.");
}

// ============================================================
//  sendAT()
//  Sends one AT command and waits up to waitMs for OK/ERROR.
//  Returns the full response string.
//  Used by: initSIM800(), sendSMS()
// ============================================================
String sendAT(String cmd, int waitMs) {
  sim800.println(cmd);
  String resp = "";
  unsigned long start = millis();

  while (millis() - start < (unsigned long)waitMs) {
    while (sim800.available()) {
      char c = sim800.read();
      resp += c;
      Serial.write(c);
    }
    if (resp.indexOf("OK") != -1 || resp.indexOf("ERROR") != -1) break;
  }
  Serial.println();
  return resp;
}

// ============================================================
//  readSMS()
//  Called every loop. Reads whatever SIM800L sends byte-by-byte
//  into simBuffer. When a newline arrives, checks if the buffer
//  contains an SMS payload and calls processCommand().
//
//  SIM800L pushes incoming SMS in this format:
//    +CMT: "+91XXXXXXXXXX","","26/04/14,13:00:00+22"
//    GPS          ← actual message text on next line
// ============================================================
void readSMS() {
  while (sim800.available()) {
    char c = sim800.read();
    Serial.write(c);
    simBuffer += c;

    if (c == '\n') {
      String line = simBuffer;
      line.trim();
      simBuffer = "";

      // Ignore header line starting with +CMT:
      if (line.startsWith("+CMT:")) continue;
      // Any non-empty line after +CMT: is the SMS body
      if (line.length() > 0) {
        Serial.println("\n>> SMS received: " + line);
        processCommand(line);
      }
    }
  }
}

// ============================================================
//  processCommand()
//  Converts received SMS text to uppercase and checks for
//  known commands. Calls the appropriate action function.
//  Commands:
//    GPS   → reply with Google Maps link
//    STOP  → cut bike ignition via relay
//    START → restore bike ignition via relay
// ============================================================
void processCommand(String msg) {
  msg.trim();
  String upper = msg;
  upper.toUpperCase();

  if (upper.indexOf("GPS") != -1) {
    Serial.println("CMD: GPS location requested");
    sendLocation();
  }
  else if (upper.indexOf("STOP") != -1) {
    Serial.println("CMD: STOP — cutting ignition");
    bikeEnabled = false;
    digitalWrite(RELAY_PIN, LOW); // LOW = relay ON = ignition CUT
    sendSMS("Bike ignition OFF. Relay activated.");
  }
  else if (upper.indexOf("START") != -1) {
    Serial.println("CMD: START — restoring ignition");
    bikeEnabled = true;
    digitalWrite(RELAY_PIN, HIGH); // HIGH = relay OFF = ignition OK
    sendSMS("Bike ignition ON. Relay deactivated.");
  }
  else {
    Serial.println("CMD: Unknown command — " + msg);
  }
}

// ============================================================
//  readGPS()
//  Called every loop. Reads NMEA sentences from the GPS module
//  one character at a time into gpsBuffer.
//  When a complete sentence (ends with \n) arrives, passes it
//  to parseNMEA() if it starts with $GPRMC or $GNRMC.
//
//  $GPRMC sentence format:
//  $GPRMC,HHMMSS,A,LLLL.LL,N,YYYYY.YY,E,speed,course,date,,,*CS
//  Field [2] = A (active/fix) or V (void/no fix)
//  Field [3+4] = latitude + N/S
//  Field [5+6] = longitude + E/W
// ============================================================
void readGPS() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gpsBuffer += c;

    if (c == '\n') {
      String sentence = gpsBuffer;
      gpsBuffer = "";

      if (sentence.startsWith("$GPRMC") || sentence.startsWith("$GNRMC")) {
        parseNMEA(sentence);
      }
    }
  }
}

// ============================================================
//  parseNMEA()
//  Splits the $GPRMC sentence by commas and extracts:
//   - Fix status (field 2): A = valid, V = no fix
//   - Raw latitude  (field 3) + direction (field 4)
//   - Raw longitude (field 5) + direction (field 6)
//  Then converts from NMEA DDDMM.MMMM format to decimal degrees
//  and stores in lastLat / lastLon globals.
// ============================================================
void parseNMEA(String sentence) {
  // Split by comma into fields array
  String fields[15];
  int fieldIndex = 0;
  int start = 0;

  for (int i = 0; i < sentence.length() && fieldIndex < 15; i++) {
    if (sentence[i] == ',' || sentence[i] == '*') {
      fields[fieldIndex++] = sentence.substring(start, i);
      start = i + 1;
    }
  }

  if (fieldIndex < 7) return; // incomplete sentence

  // Field [2] = A (fix) or V (void)
  if (fields[2] != "A") {
    gpsFix = false;
    return;
  }

  gpsFix = true;

  // Convert NMEA lat: DDDMM.MMMM → decimal degrees
  float rawLat = fields[3].toFloat();
  int   latDeg = (int)(rawLat / 100);
  float latMin = rawLat - (latDeg * 100);
  float lat    = latDeg + (latMin / 60.0);
  if (fields[4] == "S") lat = -lat;

  // Convert NMEA lon: DDDMM.MMMM → decimal degrees
  float rawLon = fields[5].toFloat();
  int   lonDeg = (int)(rawLon / 100);
  float lonMin = rawLon - (lonDeg * 100);
  float lon    = lonDeg + (lonMin / 60.0);
  if (fields[6] == "W") lon = -lon;

  // Store as strings with 6 decimal places
  lastLat = String(lat, 6);
  lastLon = String(lon, 6);
}

// ============================================================
//  sendLocation()
//  Sends an SMS containing a Google Maps link using the last
//  known GPS coordinates. If no GPS fix is available, sends
//  a "no fix" warning instead.
// ============================================================
void sendLocation() {
  if (!gpsFix) {
    sendSMS("No GPS fix yet. Place bike near open sky and try again.");
    return;
  }
  String url = "https://maps.google.com/?q=" + lastLat + "," + lastLon;
  String msg = "Bike location:\n" + url;
  sendSMS(msg);
}

// ============================================================
//  sendSMS()
//  Sends a text message to OWNER_NUMBER using AT commands:
//    AT+CMGF=1         → ensure text mode
//    AT+CMGS="number"  → start message, wait for > prompt
//    <text>            → message body
//    Ctrl+Z (0x1A)     → send signal
// ============================================================
void sendSMS(String text) {
  Serial.println("Sending SMS: " + text);

  sendAT("AT+CMGF=1", 1000);

  sim800.print("AT+CMGS=\"");
  sim800.print(OWNER_NUMBER);
  sim800.println("\"");
  delay(2000); // wait for > prompt

  sim800.print(text);
  delay(200);
  sim800.write(0x1A); // Ctrl+Z — triggers send
  delay(5000);        // wait for CMGS: confirmation

  Serial.println("SMS sent.");
}