# Banks iDash CAN Sniffing

This repository contains an Arduino sketch that listens passively on a 1 Mbit/s GM-style CAN bus used by the Banks iDash 1.8/2.8. The logger focuses on the extended identifier `0x5D7C` which carries temperature data in the fourth data byte.

## Features

* MCP2515 initialized for 1 Mbps listen-only mode.
* Optional debug mode prints every observed frame ID.
* Hardware filtering restricts traffic to only the `0x5D7C` frame when not in debug mode.
* Known filler frame IDs can be skipped in software.
* Temperature byte converted from raw °C to °F with configurable scaling.
* CSV output to the serial port and to an SD card with rolling log files (1 MB each).
* Basic handling for CAN controller errors and automatic reinitialization on bus-off.

## Building

The sketch targets an Arduino Uno/Nano with an MCP2515 CAN controller and an SPI SD card.
Use the Arduino IDE or `arduino-cli` to compile and upload.

## Output format

```
time_ms,ID,DLC,data0,data1,data2,data3,data4,data5,data6,data7,Temp_F
```

The `Temp_F` column is printed when at least four data bytes are present.
