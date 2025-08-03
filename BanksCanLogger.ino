/*
  Banks iDash CAN Logger – Temp Frame Only + Debug Option + SD Logging
  -------------------------------------------------------------------
  MCU    : Arduino Uno / Nano (ATmega328P @ 16 MHz)
  CAN    : MCP2515 + TJA1050, 8 MHz crystal @ 1 Mbit/s
  Author : Matt K. – August 2025

  Purpose:
    Listen silently on the CAN bus and ONLY display frames with meaningful data.
    Suppresses known filler frames.
    Decodes and displays temperature data from frame 0x5D7C.
    Logs CSV data to SD card with rolling log files.

  Output:
    - CSV: time_ms,ID,DLC,data0,...,dataN
    - Decoded Fahrenheit temp from byte 3 of the message (0x5D7C)
    - Identical CSV stored to SD card
*/

#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>

#ifndef MCP_EFLG_TXBO
#define MCP_EFLG_TXBO 0x20  // Bus-off flag bit in EFLG register
#endif

#define CS_PIN       10
#define SD_CS_PIN     4
#define SERIAL_BAUD 115200
#define CAN_CLOCK    MCP_8MHZ
#define CAN_RATE     CAN_1000KBPS
#define DEBUG_MODE   true
#define MAX_LOG_SIZE (1024UL * 1024UL)   // 1 MB per file

// Temperature scaling (adjust if real scaling is discovered)
const float TEMP_SCALE_C = 1.0f;   // degrees C per bit
const float TEMP_OFFSET_C = 0.0f;  // offset in degrees C

MCP_CAN CAN0(CS_PIN);
File logFile;
uint16_t logIndex = 0;

const uint32_t ignoredIDs[] = {
  0x00033B, 0x06767, 0x08E6F, 0x0031F,
  0x00000000, 0x06F72, 0x07D4F, 0x08A6F, 0x0033B
};

bool shouldIgnoreID(uint32_t id) {
  for (uint8_t i = 0; i < sizeof(ignoredIDs) / sizeof(ignoredIDs[0]); ++i) {
    if (id == ignoredIDs[i]) return true;
  }
  return false;
}

void setupFilters() {
  if (DEBUG_MODE) return; // allow all frames in debug mode
  // Mask all bits and accept only ID 0x5D7C
  CAN0.init_Mask(0, 1, 0x1FFFFFFF); // mask for filters 0-1
  CAN0.init_Mask(1, 1, 0x1FFFFFFF); // mask for filters 2-5
  for (uint8_t filt = 0; filt < 6; ++filt) {
    CAN0.init_Filt(filt, 1, 0x5D7C);
  }
}

bool openNextLogFile() {
  char name[12];
  for (; logIndex < 1000; ++logIndex) {
    snprintf(name, sizeof(name), "LOG%03u.CSV", logIndex);
    if (!SD.exists(name)) {
      logFile = SD.open(name, FILE_WRITE);
      return logFile;
    }
  }
  return false;
}

void rollLogFile() {
  if (logFile && logFile.size() >= MAX_LOG_SIZE) {
    logFile.close();
    openNextLogFile();
  }
}

void handleErrors() {
  uint8_t eflg;
  if (CAN0.checkError(&eflg) != CAN_OK) {
    Serial.print(F("CAN error 0x"));
    Serial.println(eflg, HEX);
    if (eflg & MCP_EFLG_TXBO) { // bus-off
      CAN0.begin(MCP_ANY, CAN_RATE, CAN_CLOCK);
      CAN0.setMode(MCP_LISTENONLY);
      setupFilters();
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial);

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH); // deselect SD

  if (CAN0.begin(MCP_ANY, CAN_RATE, CAN_CLOCK) != CAN_OK) {
    Serial.println(F("CAN init failed – check wiring and rate"));
    while (true);
  }
  setupFilters();
  CAN0.setMode(MCP_LISTENONLY);

  if (SD.begin(SD_CS_PIN)) {
    openNextLogFile();
  } else {
    Serial.println(F("SD init failed – logging disabled"));
  }

  Serial.println(F("CAN bus initialized"));
  Serial.println(F("time_ms,ID,DLC,data0,data1,data2,data3,data4,data5,data6,data7,Temp_F"));
}

void logLine(const char *line) {
  if (logFile) {
    logFile.println(line);
    logFile.flush();
    rollLogFile();
  }
}

float convertToFahrenheit(uint8_t raw) {
  float degC = raw * TEMP_SCALE_C + TEMP_OFFSET_C;
  return degC * 9.0 / 5.0 + 32.0;
}

void loop() {
  if (CAN0.checkReceive() != CAN_MSGAVAIL) {
    handleErrors();
    return;
  }

  uint32_t id;
  uint8_t len;
  uint8_t buf[8];

  CAN0.readMsgBuf(&id, &len, buf);

  if (DEBUG_MODE) Serial.println(id, HEX);

  uint32_t cleanID = id & 0x1FFFFFFF;
  if (shouldIgnoreID(cleanID)) return;
  if (cleanID != 0x5D7C) return;

  unsigned long timestamp = millis();
  char line[128];
  int n = snprintf(line, sizeof(line), "%lu,%08X,%u", timestamp, id, len);
  for (uint8_t i = 0; i < len; ++i) {
    n += snprintf(line + n, sizeof(line) - n, ",%02X", buf[i]);
  }
  if (len >= 4) {
    float degF = convertToFahrenheit(buf[3]);
    snprintf(line + n, sizeof(line) - n, ",%.1f", degF);
  }
  Serial.println(line);
  logLine(line);
}
