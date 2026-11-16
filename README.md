# Pwny

**Pwny** is a custom, compact handheld multi-protocol security testing and signal acquisition platform designed for wireless embedded security research. Built around an ESP32 microcontroller with a deterministic FreeRTOS firmware architecture, Pwny offloads its user interface and data management to a custom native Android companion application over Bluetooth.

---

## Overview

Traditional wireless security multi-tools often rely on onboard screens and physical buttons, which increase hardware footprint, power consumption, and UI complexity. **Pwny** decouples the timing-critical signal processing hardware from the user interface:

1. **The Hardware Node (ESP32):** Handles deterministic, microsecond-accurate signal capture, modulation, and protocol execution across Sub-GHz RF, Infrared (IR), and NFC interfaces.
2. **The Companion App (Android):** Provides a clean, responsive mobile interface over Bluetooth for commanding the hardware, managing saved signal profiles and executing replay attacks.

---

## Hardware Architecture

The hardware is built on a custom-designed **2-layer PCB** engineered in **Altium Designer** and fabricated via **JLCPCB**.

### Core & Subsystems
* **Microcontroller:** **ESP32-WROOM-32**
  * Utilizes the hardware **RMT (Remote Control Transceiver)** peripheral for precise, hardware-backed pulse-width measurement and arbitrary waveform generation without CPU jitter.
* **Sub-GHz Transceiver:** **CC1101**
  * Supports multi-band Sub-GHz RF signal capture, raw OOK demodulation, packet sniffing, and transmission.
* **NFC / RFID Module:** **PN532** (UART interface)
  * Supports reading, writing, and tag emulation for ISO/IEC 14443 Type A/B and MIFARE contactless cards.
* **Infrared (IR) Subsystem:**
  * **Receiver:** **TSOP1838** 38 kHz demodulating IR receiver for capturing standard remote control protocols.
  * **Transmitter:** High-output **940 nm IR LED** driven by a **BC547C** NPN transistor stage for extended transmission range.
* **Power Management:**
  * **Battery:** 3.7V 1000 mAh LiPo cell.
  * **Charging:** **TP4056** linear Li-Ion battery charger with USB-C input.
  * **Regulation:** **XL63000** buck-boost DC-DC converter ensuring a stable 3.3V rail across the entire battery discharge curve.

---

## Firmware Architecture

The embedded firmware is developed using **FreeRTOS** to guarantee deterministic timing and non-blocking execution across peripherals:

* **Task Isolation:** Dedicated FreeRTOS tasks separate time-critical RF/IR/NFC hardware operations from asynchronous Bluetooth communication parsing.
* **Hardware-Accelerated Timing:** Direct integration with the ESP32 RMT peripheral ensures that high-frequency pulse trains are captured and replayed with minimal latency and zero interference from wireless stack interrupts.
* **Serial Bluetooth Protocol:** Implements a lightweight, structured command-and-telemetry protocol over Bluetooth Classic (SPP) to stream raw signal timings and tag dumps to the Android client.

---

## Mobile Companion App

To maintain a sleek hardware form factor and leverage modern mobile computing power, all user interactions are handled via a custom **native Java Android application**.

### Key Features
* **Bluetooth Device Management:** Seamless pairing, connection state monitoring, and low-latency command dispatch to the ESP32.
* **Protocol Modules:** Dedicated UI views for Sub-GHz RF, Infrared, and NFC/RFID operations.
* **Signal Capture & Replay:** Real-time logging of incoming signals, parameter configuration (frequency, modulation, protocol), and one-tap transmission/replay.
* **Local Storage & Library:** Save, categorize, rename, and export captured signal profiles and NFC dumps directly on the Android device.

---

## Getting Started

### Prerequisites
* **Hardware:** Custom Pwny PCB populated with the components listed in the Hardware Architecture section.
* **Firmware Toolchain:** Arduino IDE with FreeRTOS support.
* **Mobile Toolchain:** Android Studio (JDK 11+ recommended) for building the native Java application.

---

## Firmware Dependancies
* **Arduino ESP32 core (v2.0.17):** Arduino wrapper functions for ESP32
* **RadioLib (v7.5.0):** CC1101 interface 
* **Adafruit_PN532 (v1.3.4):** PN532 interface
* **IRRemoteESP8266 (v8.2.6):** IR receiver and transmitter interface
* **BluetoothSerial (ESP32 core v2.0.17 version):** Bluetooth interface
---

## Disclaimer

**For Educational and Authorized Security Research Only.**  
Pwny was developed as part of academic research (Master's thesis) into wireless embedded system vulnerabilities and deterministic signal acquisition. Users are responsible for complying with all applicable local laws and regulations regarding radio frequency transmission and security auditing. Do not test against systems or devices without explicit permission.

---

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

