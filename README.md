# ECE445L Lab 4E - IoT Alarm Clock with MQTT and ESP8266

An embedded IoT alarm clock built on the **TM4C123 LaunchPad** with an **ESP8266 Wi-Fi module**, **MQTT-based communication**, and a **web application interface**. This project extends a prior clock system by enabling real-time remote monitoring and control over Wi-Fi.

## Overview
This project implements a “smart object” that publishes clock state to an MQTT broker and receives control commands from a web application. The system bridges embedded firmware, UART communication, Wi-Fi networking, and a browser-based frontend.

## Features
- Real-time clock displayed on the ST7735 LCD
- Remote control through MQTT publish/subscribe
- ESP8266 used as the Wi-Fi bridge between the TM4C123 and the broker
- Web app with clock controls:
  - Hour++
  - Hour--
  - Minute++
  - Minute--
  - Second++
  - Second--
  - Toggle 12/24-hour mode
- UART-based communication between TM4C123 and ESP8266
- Periodic state publishing from the board to the web app
- Subscription handling for incoming web commands
- Reuse and extension of prior clock/alarm functionality

## System Architecture
The data flow is:

TM4C123 -> UART -> ESP8266 -> Wi-Fi -> MQTT Broker -> Web App  
Web App -> MQTT Broker -> ESP8266 -> UART -> TM4C123

The TM4C publishes time data, and the web app sends back control commands using MQTT topics scoped to the user/team EID.

## Hardware
- TI EK-TM4C123GXL LaunchPad
- ESP8266-ESP01
- ST7735 TFT LCD
- External 3.3V regulator for ESP8266
- Switches
- Speaker / alarm hardware reused from Lab 3
- Supporting resistors, capacitors, MOSFET, diode, and wiring

## Software Components
### Embedded firmware
- TM4C123 firmware in C
- UART interface to ESP8266
- MQTT serialization/parsing support
- Clock/alarm logic and LCD update routines

### ESP8266 firmware
- Arduino-based firmware
- Wi-Fi connection handling
- MQTT broker connection
- Topic subscription and publication
- UART bridge between LaunchPad and MQTT broker

### Web application
- HTML / JavaScript MQTT client
- WebSocket-based broker communication
- Live time display and command buttons

## MQTT Interface
Example topic structure:

- `<eid>/b2w/mode`
- `<eid>/b2w/hour`
- `<eid>/b2w/min`
- `<eid>/b2w/sec`
- `<eid>/w2b`

Where:
- `b2w` = board to web
- `w2b` = web to board

## Key Technical Work
- Configured UART5 communication between the TM4C123 and ESP8266
- Modified parser logic for web-to-board commands
- Built CSV-based data transfer from TM4C firmware to the ESP8266
- Extended a browser MQTT app for remote clock control
- Integrated Lab 3 clock behavior into a networked IoT system
- Debugged timing, subscriptions, and state synchronization across devices

## Validation
This project was validated by:
- confirming broker connection from the ESP8266
- verifying topic publish/subscribe behavior
- checking synchronized updates on both the LCD and web interface
- testing remote control actions from the web app
- observing UART/debug output and timing behavior

## Repository Structure
```text
src/        TM4C123 firmware
esp/        ESP8266 Arduino code
web/        MQTT web application
docs/       images, diagrams, report assets
project/    Keil or IDE project files
