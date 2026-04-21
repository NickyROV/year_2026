#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <MS5837.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h> // Added for ESP32-S3 Built-in LED

// ============================================================================
// SHARED STRUCTURES (PACKED for ESP-NOW)
// ============================================================================

typedef struct __attribute__((packed)) {
    char company_id[10];
    uint32_t timestamp;  
    float pressure_kpa;
    float depth_m;
    float temp_c;
} struct_message;

typedef struct __attribute__((packed)) {
    char cmd[16];
    char company_id[10];
    float target_fd;      // First depth target (2.5m)
    float target_sd;      // Second depth target (0.4m)
    int fdt;              // First depth hold time (30 sec)
    int sdt;              // Second depth hold time (30 sec)
} struct_command;

typedef struct __attribute__((packed)) {
    char msg[32];
} struct_status;

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

#define STEP_PIN 13
#define DIR_PIN 12
#define LIMIT_FWD 9
#define LIMIT_BWD 10
#define SDA_PIN 4
#define SCL_PIN 5

// NeoPixel Configuration for ESP32-S3
#define RGB_BRIGHTNESS 50 // 0-255 scale
#define LED_PIN 48        // Built-in NeoPixel on ESP32-S3
Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================================
// STATE MACHINE DEFINITION
// ============================================================================

enum MissionState { 
    IDLE, 
    CALIBRATING, 
    DESCEND_P1_LOW, 
    HOLD_P1_LOW, 
    ASCEND_P1_HIGH, 
    HOLD_P1_HIGH, 
    DESCEND_P2_LOW, 
    HOLD_P2_LOW, 
    ASCEND_P2_HIGH, 
    HOLD_P2_HIGH, 
    SURFACING, 
    MISSION_DONE 
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

MissionState currentState = IDLE;

MS5837 sensor;
uint8_t controlMac[] = {0xAC, 0xA7, 0x04, 0x29, 0x8F, 0x84};  // Control station MAC

// Mission parameters
float surface_pressure_kpa = 0;
float surface_pressure_pa = 0;      // For accurate depth calculation
char active_company_id[10] = "PENDING";
float target_fd = 0;                 // Target first depth (2.5m)
float target_sd = 0;                 // Target second depth (0.4m)
int fdt = 0;                         // First depth hold time
int sdt = 0;                         // Second depth hold time

// Control flags
bool start_mission = false;
bool transmit_requested = false;

// Timing variables
unsigned long missionStartTime = 0; 
unsigned long logTimer = 0;

// Hold tracking variables for 7-packet verification
// Profile 1
unsigned long p1_low_hold_start = 0;
int p1_low_valid_packets = 0;
bool p1_low_active = false;
unsigned long p1_high_hold_start = 0;
int p1_high_valid_packets = 0;
bool p1_high_active = false;

// Profile 2
unsigned long p2_low_hold_start = 0;
int p2_low_valid_packets = 0;
bool p2_low_active = false;
unsigned long p2_high_hold_start = 0;
int p2_high_valid_packets = 0;
bool p2_high_active = false;

// Data logging
struct_message sensor_data[500]; 
int log_index = 0;

// Sensor offset (if pressure sensor not at bottom/top)
// For this example, assume sensor is at bottom of float
// If sensor is 25cm above bottom, set SENSOR_BOTTOM_OFFSET = -0.25
// If sensor is at top, set SENSOR_TOP_OFFSET = -0.25 for 40cm depth
const float SENSOR_BOTTOM_OFFSET = 0.0;   // Sensor at bottom: offset 0
const float SENSOR_TOP_OFFSET = 0.0;      // Sensor at top: offset 0
// Declare these to judge before deployment!

// ============================================================================
// DEPTH CALCULATION
// ============================================================================

float getDepth() {
    sensor.read();
    // MS5837 pressure() returns pressure in millibars (mbar)
    // 1 mbar = 100 Pa = 0.1 kPa
    float pressure_pa = sensor.pressure() * 100.0f;  // Convert to Pascals
    
    // Depth calculation using EGADS solution density (1025 kg/m³)
    // P = ρ * g * h  =>  h = P / (ρ * g)
    // g = 9.80665 m/s², ρ = 1025 kg/m³
    float depth = (pressure_pa - surface_pressure_pa) / (1025.0f * 9.80665f);
    
    // Ensure depth is not negative due to noise
    if (depth < 0) depth = 0;
    
    return depth;
}

// Get depth with bottom offset (for 2.5m target)
float getBottomDepth() {
    return getDepth() + SENSOR_BOTTOM_OFFSET;
}

// Get depth with top offset (for 0.4m target)
float getTopDepth() {
    return getDepth() + SENSOR_TOP_OFFSET;
}

// ============================================================================
// LED INDICATORS (Visual feedback for judge)
// ============================================================================

void setLEDs(float current_depth, float target_low, float target_high) {
    // Check ranges based on MATE specs [cite: 179]
    bool in_low_range = (abs(current_depth - target_low) <= 0.33);
    bool in_high_range = (abs(current_depth - target_high) <= 0.33);

    if (in_low_range) {
        pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue for 2.5m
    } else if (in_high_range) {
        pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green for 0.4m
    } else if (currentState != IDLE && currentState != MISSION_DONE) {
        // Blink Red if moving/active but not in range
        static bool blink = false;
        blink = !blink;
        pixel.setPixelColor(0, blink ? pixel.Color(255, 0, 0) : pixel.Color(0, 0, 0));
    } else {
        pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // OFF
    }
    pixel.show();
}

// ============================================================================
// DATA LOGGING
// ============================================================================

void logData() {
    if (log_index >= 500) {
        Serial.println("WARNING: Log buffer full!");
        return;
    }
    
    sensor.read();
    float current_depth = getDepth();
    
    strncpy(sensor_data[log_index].company_id, active_company_id, 10);
    sensor_data[log_index].timestamp = (millis() - missionStartTime) / 1000;
    sensor_data[log_index].pressure_kpa = sensor.pressure() / 10.0f;
    sensor_data[log_index].depth_m = current_depth;
    sensor_data[log_index].temp_c = sensor.temperature();
    
    Serial.printf("[LOG %d] T:%us D:%.3fm P:%.1fkPa T:%.1fC\n", 
                  log_index, 
                  sensor_data[log_index].timestamp, 
                  sensor_data[log_index].depth_m, 
                  sensor_data[log_index].pressure_kpa,
                  sensor_data[log_index].temp_c);
    
    log_index++;
}

// ============================================================================
// BUOYANCY ENGINE CONTROL
// ============================================================================
// IMPORTANT: This moves piston to DISCRETE positions, then STOPS.
// The float moves naturally by buoyancy, NOT by motor thrust.
// This complies with MATE rules for a buoyancy engine.

// Pre-calculated piston positions (calibrate these values)
//const int PISTON_POS_SURFACE = 0;      // Most volume displaced (float up)
//const int PISTON_POS_40CM = 200;       // Partial volume (neutral at 40cm)
//const int PISTON_POS_2_5M = 400;       // Least volume displaced (float down)
int currentPistonPosition = 0;

//Stepper movement control
void movePistonTo(int target_steps, int speed_us = 800) {
    if (target_steps == currentPistonPosition) return;
    
    // DIR HIGH = SINK (Forward to switch)
    // DIR LOW = RISE (Backward away from switch)
    digitalWrite(DIR_PIN, (target_steps > currentPistonPosition) ? HIGH : LOW);
    
    int steps_to_move = abs(target_steps - currentPistonPosition);
    
    for (int i = 0; i < steps_to_move; i++) {
        // Safety: Only check limit switch if moving HIGH (Sinking/Forward)
        if (digitalRead(LIMIT_FWD) == LOW && (target_steps > currentPistonPosition)) {
        //    break; 
        }
        
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(speed_us);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(speed_us);
    }
    currentPistonPosition = target_steps; 
}
// Set buoyancy for target depth using discrete positions

void setBuoyancyForDepth(float target_depth) {
    static unsigned long lastAdjustTime = 0;
    float current_depth = getDepth(); 
    const int NUDGE_STEPS = 50;   
    const float DEADZONE = 0.08;  
    
    if (millis() - lastAdjustTime < 1500) return;

    if (target_depth <= 0.1) {
        if (currentPistonPosition > 0) {
            movePistonTo(0); // Move to 0
            lastAdjustTime = millis();
        }
        return;
    }

    if (current_depth < target_depth - DEADZONE) {
        // Create a LOCAL target variable
        int new_target = currentPistonPosition + NUDGE_STEPS;
        if (new_target > 2200) new_target = 2200; // Updated to 2200
        
        Serial.printf(">>> Nudging SINK: From %d to %d\n", currentPistonPosition, new_target);
        movePistonTo(new_target); 
        lastAdjustTime = millis();
    } 
    else if (current_depth > target_depth + DEADZONE) {
        int new_target = currentPistonPosition - NUDGE_STEPS;
        if (new_target < 0) new_target = 0; 
        
        Serial.printf(">>> Nudging RISE: From %d to %d\n", currentPistonPosition, new_target);
        movePistonTo(new_target); 
        lastAdjustTime = millis();
    }
}
// ============================================================================
// ESP-NOW CALLBACKS
// ============================================================================

void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(struct_command)) {
        struct_command received_cmd;
        memcpy(&received_cmd, incomingData, sizeof(received_cmd));
        
        if (strcmp(received_cmd.cmd, "predive") == 0) {
            // Pre-dive verification - send current data to control station
            sensor.read();
            struct_message p;
            strncpy(p.company_id, received_cmd.company_id, 10);
            p.timestamp = 0;
            p.pressure_kpa = sensor.pressure() / 10.0f;
            p.depth_m = 0;
            p.temp_c = sensor.temperature();
            esp_now_send(controlMac, (uint8_t *) &p, sizeof(p));
            Serial.println(">>> PRE-DIVE DATA SENT to control station");
        } 
        else if (strcmp(received_cmd.cmd, "deploy") == 0) {
            strncpy(active_company_id, received_cmd.company_id, 10);
            target_fd = received_cmd.target_fd; 
            target_sd = received_cmd.target_sd;
            fdt = received_cmd.fdt; 
            sdt = received_cmd.sdt;
            start_mission = true;
            Serial.printf(">>> MISSION CONFIG: %.2fm (hold %ds), %.2fm (hold %ds)\n", 
                          target_fd, fdt, target_sd, sdt);
        } 
        else if (strcmp(received_cmd.cmd, "send_now") == 0) {
            transmit_requested = true;
            Serial.println(">>> DATA REQUEST RECEIVED - will transmit after recovery");
        }
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("---SYSTEM BOOTING---");

    // Initialize NeoPixel
    pixel.begin();
    pixel.setBrightness(RGB_BRIGHTNESS);
    pixel.setPixelColor(0, pixel.Color(255, 255, 255)); // White on boot
    pixel.show();

    // Pin configuration
    pinMode(LIMIT_FWD, INPUT_PULLUP);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    
    // PHASE 1: Seek the Forward Limit Switch (Deepest/Least Buoyant point)
    digitalWrite(DIR_PIN, HIGH); 
    while(digitalRead(LIMIT_FWD) == HIGH) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(800);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(800);
    }
    // At this exact moment, the piston is at its MAX SINK position.

    // PHASE 2: Move 2200 steps BACKWARD to the Surface position
    digitalWrite(DIR_PIN, LOW); 
    for(int i = 0; i < 2200; i++) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(800);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(800);
    }

    // PHASE 3: THE FIX
    // We tell the code: "Where we are right now is ZERO (Surface)."
    currentPistonPosition = 0; 
    
    Serial.println("Piston homed: Surface = 0, Max Sink capability = 2200");
    
    // ESP-NOW initialization
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed!");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, controlMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    
    // Pressure sensor initialization
    Wire.begin(SDA_PIN, SCL_PIN);
    struct_status status;
    
    if (!sensor.init()) {
        strcpy(status.msg, "I2C ERROR");
        esp_now_send(controlMac, (uint8_t *) &status, sizeof(status));
        Serial.println("MS5837 init FAILED!");
        pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Static Red for error
    } else {
        sensor.setModel(MS5837::MS5837_30BA);
        sensor.setFluidDensity(1025);  // EGADS solution density
        strcpy(status.msg, "Ready: 0x76 OK");
        esp_now_send(controlMac, (uint8_t *) &status, sizeof(status));
        Serial.println("MS5837 initialized successfully");
        pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // Clear after init
    }
    pixel.show();
    
    Serial.println("--- FLOAT READY ---");
    Serial.println("Sensor offset declarations (provide to judge):");
    Serial.printf("  Bottom offset (2.5m target): %.2fm\n", SENSOR_BOTTOM_OFFSET);
    Serial.printf("  Top offset (0.4m target): %.2fm\n", SENSOR_TOP_OFFSET);
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Continuous logging every 5 seconds during entire mission
    // (excluding IDLE before mission start and after completion)
    if (currentState != IDLE && currentState != MISSION_DONE) {
        if (millis() - logTimer >= 5000) { 
            logData(); 
            logTimer = millis(); 
        }
    }
    
    // Mission start sequence
    if (start_mission && currentState == IDLE) {
        Serial.println("[START] Initializing mission...");
        setBuoyancyForDepth(0);  // Surface position
        currentState = CALIBRATING;
        start_mission = false;
    }
    
    // State machine
    switch(currentState) {
        
        // ================================================================
        // CALIBRATION - Get surface pressure reference
        // ================================================================
        case CALIBRATING: {
            Serial.println("[CALIBRATING] Measuring surface pressure...");
            float sum_pa = 0;
            for(int i = 0; i < 20; i++) { 
                sensor.read(); 
                sum_pa += (sensor.pressure() * 100.0f); 
                delay(50); 
            }
            surface_pressure_pa = sum_pa / 20.0f;
            surface_pressure_kpa = surface_pressure_pa / 1000.0f;
            
            missionStartTime = millis(); 
            logTimer = millis();
            logData();
            
            currentState = DESCEND_P1_LOW;
            Serial.printf("[CALIBRATION] Surface pressure: %.2f kPa\n", surface_pressure_kpa);
            break;
        }
        
        // ================================================================
        // PROFILE 1 - DESCEND to 2.5m
        // ================================================================
        case DESCEND_P1_LOW:
            setBuoyancyForDepth(target_fd);
            setLEDs(getBottomDepth(), target_fd, target_sd);
            if (abs(getBottomDepth() - target_fd) < 0.05) {
                currentState = HOLD_P1_LOW;
                p1_low_hold_start = millis();
                p1_low_valid_packets = 0;
                p1_low_active = true;
            }
            break;
        
        // ================================================================
        // PROFILE 1 - HOLD at 2.5m (±33cm) with 7-packet verification
        // ================================================================
        case HOLD_P1_LOW: { 
            setBuoyancyForDepth(target_fd);
            float p1_low_depth = getBottomDepth();
            bool p1_low_in_range = (abs(p1_low_depth - target_fd) <= 0.33);
            
            setLEDs(p1_low_depth, target_fd, target_sd);
            
            if (!p1_low_in_range) {
                p1_low_hold_start = millis();
                p1_low_valid_packets = 0;
            } else {
                int intervals_completed = (millis() - p1_low_hold_start) / 5000;
                if (intervals_completed > p1_low_valid_packets && intervals_completed <= 7) {
                    p1_low_valid_packets = intervals_completed;
                }
            }
            
            if (p1_low_valid_packets >= 7) {
                currentState = ASCEND_P1_HIGH;
                p1_low_active = false;
            }
            break;
        } 
        
        // ================================================================
        // PROFILE 1 - ASCEND to 40cm
        // ================================================================
        case ASCEND_P1_HIGH:
            setBuoyancyForDepth(target_sd);
            setLEDs(getTopDepth(), target_fd, target_sd);
            if (abs(getTopDepth() - target_sd) < 0.05) {
                currentState = HOLD_P1_HIGH;
                p1_high_hold_start = millis();
                p1_high_valid_packets = 0;
                p1_high_active = true;
            }
            break;
        
        // ================================================================
        // PROFILE 1 - HOLD at 40cm (±33cm) with 7-packet verification
        // ================================================================
        case HOLD_P1_HIGH: { 
            setBuoyancyForDepth(target_sd);
            float p1_high_depth = getTopDepth();
            bool p1_high_in_range = (abs(p1_high_depth - target_sd) <= 0.33);
            
            setLEDs(p1_high_depth, target_fd, target_sd);
            
            if (!p1_high_in_range) {
                p1_high_hold_start = millis();
                p1_high_valid_packets = 0;
            } else {
                int intervals_completed = (millis() - p1_high_hold_start) / 5000;
                if (intervals_completed > p1_high_valid_packets && intervals_completed <= 7) {
                    p1_high_valid_packets = intervals_completed;
                }
            }
            
            if (p1_high_valid_packets >= 7) {
                currentState = DESCEND_P2_LOW;
                p1_high_active = false;
            }
            break;
        } 
        
        // ================================================================
        // PROFILE 2 - DESCEND to 2.5m
        // ================================================================
        case DESCEND_P2_LOW:
            setBuoyancyForDepth(target_fd);
            setLEDs(getBottomDepth(), target_fd, target_sd);
            if (abs(getBottomDepth() - target_fd) < 0.05) {
                currentState = HOLD_P2_LOW;
                p2_low_hold_start = millis();
                p2_low_valid_packets = 0;
                p2_low_active = true;
            }
            break;
        
        // ================================================================
        // PROFILE 2 - HOLD at 2.5m with verification
        // ================================================================
        case HOLD_P2_LOW: { 
            setBuoyancyForDepth(target_fd);
            float p2_low_depth = getBottomDepth();
            bool p2_low_in_range = (abs(p2_low_depth - target_fd) <= 0.33);
            
            setLEDs(p2_low_depth, target_fd, target_sd);
            
            if (!p2_low_in_range) {
                p2_low_hold_start = millis();
                p2_low_valid_packets = 0;
            } else {
                int intervals_completed = (millis() - p2_low_hold_start) / 5000;
                if (intervals_completed > p2_low_valid_packets && intervals_completed <= 7) {
                    p2_low_valid_packets = intervals_completed;
                }
            }
            
            if (p2_low_valid_packets >= 7) {
                currentState = ASCEND_P2_HIGH;
                p2_low_active = false;
            }
            break;
        } 
        
        // ================================================================
        // PROFILE 2 - ASCEND to 40cm
        // ================================================================
        case ASCEND_P2_HIGH:
            setBuoyancyForDepth(target_sd);
            setLEDs(getTopDepth(), target_fd, target_sd);
            if (abs(getTopDepth() - target_sd) < 0.05) {
                currentState = HOLD_P2_HIGH;
                p2_high_hold_start = millis();
                p2_high_valid_packets = 0;
                p2_high_active = true;
            }
            break;
        
        // ================================================================
        // PROFILE 2 - HOLD at 40cm with verification
        // ================================================================
        case HOLD_P2_HIGH: { 
            setBuoyancyForDepth(target_sd);
            float p2_high_depth = getTopDepth();
            bool p2_high_in_range = (abs(p2_high_depth - target_sd) <= 0.33);
            
            setLEDs(p2_high_depth, target_fd, target_sd);
            
            if (!p2_high_in_range) {
                p2_high_hold_start = millis();
                p2_high_valid_packets = 0;
            } else {
                int intervals_completed = (millis() - p2_high_hold_start) / 5000;
                if (intervals_completed > p2_high_valid_packets && intervals_completed <= 7) {
                    p2_high_valid_packets = intervals_completed;
                }
            }
            
            if (p2_high_valid_packets >= 7) {
                currentState = SURFACING;
                p2_high_active = false;
            }
            break;
        } 
        
        // ================================================================
        // SURFACING - Return to surface for recovery
        // ================================================================
        case SURFACING:
            setBuoyancyForDepth(0);
            setLEDs(getTopDepth(), target_fd, target_sd);
            if (getTopDepth() < 0.10) {
                currentState = MISSION_DONE;
            }
            break;
        
        // ================================================================
        // MISSION DONE - Wait for data transmission request
        // ================================================================
        case MISSION_DONE:
            pixel.setPixelColor(0, pixel.Color(128, 0, 128)); // Purple for Done
            pixel.show();
            if (transmit_requested) {
                for(int i = 0; i < log_index; i++) {
                    esp_now_send(controlMac, (uint8_t *) &sensor_data[i], sizeof(sensor_data[i]));
                    delay(80);
                }
                transmit_requested = false;
            }
            break;
        
        case IDLE:
        default:
            break;
    }
        
    // Small delay to prevent watchdog issues
    delay(10);
}
