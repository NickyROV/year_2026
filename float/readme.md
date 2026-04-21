# MATE Mission: Buoyancy Engine Project Guide

This document serves as the primary reference for the **Ranger 04 Float Project**. It covers the system introduction, operation instructions, and the technical logic behind the code.

---

## 1. Introduction
The system consists of two primary components:
1.  **The Float:** An autonomous underwater vehicle using a "Buoyancy Engine" (a stepper-motor-driven piston) to change its displacement and move vertically in the water column.
2.  **The Control Station:** A shore-based ESP32 unit that configures the mission, triggers deployment, and recovers logged data via ESP-NOW wireless communication.

### Mission Goals
- Perform two depth profiles (2.5m and 0.4m).
- Hold each depth for a minimum of 35 seconds.
- Log data every 5 seconds, providing exactly 7 packets within the ±0.33m tolerance window for each hold.

---

## 2. User Guide (Operation)

### Setup & Homing
When the Float is powered on, it enters a **Homing Sequence**:
1.  **Seek Forward:** The piston moves forward (`DIR_PIN HIGH`) until it triggers the **Forward Limit Switch (GPIO 9)**. This is the "Max Sink" position.
2.  **Retreat:** The piston moves backward 2200 steps (`DIR_PIN LOW`).
3.  **Zeroing:** The system declares this 2200-step retreat position as `0` (Surface/Max Buoyancy).

### Mission Execution
1.  **Pre-dive:** Power on the Control Station. Ensure Serial communication is established.
2.  **Calibration:** Press the **Pre-dive Button (Pin 42)** to request surface pressure data.
3.  **Deploy:** Once pre-dive is confirmed, press the **Deploy Button (Pin 1)**. The float will begin its automated state machine.
4.  **Recovery:** After the float surfaces and the mission state is `MISSION_DONE` (LED turns Purple), press the **Send Button (Pin 2)** to download all 500 log entries to the Control Station.

---

## 3. Programmer's Guide

### Key Global Variables
| Variable | Description |
| :--- | :--- |
| `currentPistonPosition` | Tracks the physical step count (0 = Surface, 2200 = Deepest). |
| `surface_pressure_pa` | Captured during the `CALIBRATING` state to provide a 0.0m depth reference. |
| `target_fd` / `target_sd` | Target depths (2.5m and 0.4m) sent from the Control Station. |
| `log_index` | Pointer for the `sensor_data[500]` array to store mission logs. |

### The "Nudging" Logic (`setBuoyancyForDepth`)
To comply with buoyancy-only movement, the float does not "drive" to a depth. Instead, it "nudges" its volume:
* **Too Shallow?** If `current_depth < target_depth`, it adds `NUDGE_STEPS` (50) to the position.
* **Too Deep?** If `current_depth > target_depth`, it subtracts `NUDGE_STEPS` from the position.
* **The Sync Rule:** The global `currentPistonPosition` variable is **only** updated at the end of the `movePistonTo()` function. This prevents the logic from thinking it has arrived before the motor has actually finished spinning.

### State Machine Flow
1.  **IDLE:** Waiting for `deploy` command.
2.  **CALIBRATING:** Averaging 20 pressure samples to find the surface.
3.  **DESCEND/ASCEND:** Moving toward the target depth.
4.  **HOLD:** Monitoring depth. If the float stays within ±0.33m for 35 seconds (7 log intervals), it transitions to the next state.
5.  **SURFACING:** Fully retracts the piston to `0` steps.
6.  **MISSION_DONE:** Stops logging and waits for the `send_now` command to transmit data.

---

## 4. Hardware Mapping
- **STEP_PIN:** GPIO 13
- **DIR_PIN:** GPIO 12 (HIGH = Sink, LOW = Rise)
- **LIMIT_FWD:** GPIO 9 (Forward/Bottom limit)
- **SDA/SCL:** GPIO 4 / 5 (MS5837 Sensor)
- **NeoPixel:** GPIO 48 (Visual status feedback)

---
*Note: This system relies on Software Limits for the backward direction. Ensure the 2200-step retreat in setup does not exceed physical hardware limits.*
"""
