# OT AIR GUARD: Real-Time Air Quality Monitoring System for Sterile Environments

An embedded Real-Time Operating System (RTOS) based safety system designed to continuously monitor and maintain sterile air quality parameters within hospital **Operating Theaters (OTs)**. Built using the **LPC1768 Cortex-M3 microcontroller** and the **Keil RTX Kernel**, this project implements event-driven multitasking to eliminate critical delays found in conventional loop-based embedded software.

---

## 🚀 Key Features
* **Deterministic Real-Time Processing:** Built on a Preemptive Priority-Based Scheduling algorithm using the Keil RTX Kernel.
* **Event-Driven Synchronization:** Replaced inefficient CPU polling with **Binary Semaphore Synchronization** (`sem_data_ready`), dropping CPU usage of safety tasks to 0% while waiting for sensor updates.
* **Dual-Sensor Data Acquisition:** Integrated **MQ-2** (smoke, combustible gases, anesthesia leaks) and **MQ-135** (air quality, VOCs, sanitizers) via 12-bit ADC.
* **Automated Safety Actuators:** Automatically triggers a 2-channel isolated relay module to drive a **DC Exhaust Fan** and a **Piezo Buzzer** upon exceeding safe biological thresholds.
* **Independent UI Refreshing:** Implemented a separate, low-priority task for the 16x2 LCD screen to ensure slow text-rendering operations never block life-safety control loops.

---

## 🛠️ Hardware Components
* **Microcontroller:** LPC1768 Development Board (ARM Cortex-M3 @ 100MHz)
* **Sensors:** MQ-2 Gas Sensor & MQ-135 Air Quality Sensor
* **Actuators:** 2-Channel Relay Module (5V DC), DC Motor with Fan Blade, 5V Active Piezo Buzzer
* **Display:** 16x2 LCD
* **Circuit Protections:** Custom Resistive Voltage Divider circuits (R₁ = 10kΩ, R₂ = 20kΩ) to scale the 5V sensor outputs safely down to the 3.3V limits of the LPC1768 ADC pins.

---

## 📊 Software Architecture & Task Scheduling
The firmware splits system operations into three concurrent tasks managed by priorities:

| Task Name | Priority | Role | Blocked Condition |
| :--- | :--- | :--- | :--- |
| **Read_Task** | Priority 3 (High) | Samples MQ-2 and MQ-135 sensors via ADC; posts the semaphore. | Delays for 10 ticks after completion. |
| **Control_Task** | Priority 3 (High) | Safety critical logic comparing thresholds and switching relays. | Sleeps on `os_sem_wait` until new data is ready. |
| **Display_Task** | Priority 2 (Normal) | Formats text strings and updates the 16x2 LCD layout. | Refreshes periodically every 20 ticks. |

### System Threshold Logic
* **SAFE Mode:** Clean baseline air (<10%).
* **NORMAL Mode:** MQ-2 exceeds 40% threshold → Activates **Exhaust Fan Only** to clear localized particulate smoke.
* **DANGER Mode:** MQ-2 exceeds 70% OR MQ-135 exceeds 60% → Triggers the **Piezo Buzzer**. 
  * *Note: Following hospital OT standards, chemical volatile spikes on the MQ-135 only trigger the buzzer and keep the fan OFF to prevent positive room pressure failures and potential ignition risks.*

---

## 💻 Development & Environment
* **IDE:** Keil µVision 4
* **Operating System:** Keil RTX Kernel
* **Programming Language:** Embedded C

---

## 👥 Project Team Associates
* **Ankush S Jalasandi**
* **Shivkumar Bannad**
* **Samarth Shirahatti**
* **Animesh**

*Department of Electrical and Electronics Engineering, KLE Technological University (Jan 2025-26)*
