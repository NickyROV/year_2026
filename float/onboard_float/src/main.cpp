#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <MS5837.h> // BlueRobotics MS5837 Library
#include <Wire.h>
#include <ArduinoJson.h>

// Initialize pressure sensor
MS5837 sensor;

// State machine states
enum FloatState {
  STATE_WAITING_READY,
  STATE_WAITING_DEPLOY,
  STATE_RECORDING,
  STATE_DESCEND_TO_SECOND_DEPTH,
  STATE_MAINTAIN_SECOND_DEPTH,
  STATE_ASCEND_TO_FIRST_DEPTH,
  STATE_MAINTAIN_FIRST_DEPTH,
  STATE_SECOND_PROFILE_START,
  STATE_SECOND_DESCEND,
  STATE_SECOND_MAINTAIN_SECOND,
  STATE_SECOND_ASCEND,
  STATE_SECOND_MAINTAIN_FIRST,
  STATE_RETURN_TO_SURFACE,
  STATE_WAITING_SEND,
  STATE_TRANSMITTING_DATA
};

// Global variables
FloatState current_state = STATE_WAITING_READY;
unsigned long state_start_time = 0;
unsigned long next_sample_time = 0;

// User-defined parameters
float surface_pressure_kPa = 101.3;  // kPa at sea level
int first_depth_cm = 40;             // cm
int first_depth_time_sec = 30;
int second_depth_cm = 250;           // cm
int second_depth_time_sec = 30;

float first_depth_m = 0.4;   // meters
float second_depth_m = 2.5;  // meters

// Sensor data
struct SensorReading {
  float pressure_kpa;
  float depth_meters;
  float temperature_c;
  unsigned long timestamp;
};

#define MAX_SAMPLES 1000
SensorReading sensor_data[MAX_SAMPLES];
int sample_count = 0;

// WiFi & MQTT
const char* ssid = "float_control";
const char* password = "float_pass";
WiFiClient espClient;
PubSubClient client(espClient);

// Pins
#define BATTERY_SENSOR_PIN A2
#define STEPPER_DIR_PIN 12
#define STEPPER_STEP_PIN 13
#define BUTTON_PIN 14

// Topics
const char* status_topic = "float/status";
const char* command_topic = "control/command";
const char* data_topic = "float/data";

// Flags
bool received_deploy_signal = false;
bool received_send_now_signal = false;

// ======================
// Function Prototypes
// ======================
void connectToWiFi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void handleButtonPress();
void processStateMachine();
void recordSample();
void controlBuoyancyForDepth(float target_depth);
void controlBuoyancyForSurface();
float getCurrentDepth();
float calculateDepth(float pressure_kpa);
void transmitAllData();

// ======================
// Setup
// ======================
void setup() {
  Serial.begin(115200);
  pinMode(STEPPER_DIR_PIN, OUTPUT);
  pinMode(STEPPER_STEP_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  if (!sensor.init()) {
    Serial.println("Pressure sensor init failed!");
    while (1) delay(1000);
  }
  Serial.println("MS5837 initialized successfully.");

  memset(sensor_data, 0, sizeof(sensor_data));

  // Update depth in meters
  first_depth_m = first_depth_cm / 100.0f;
  second_depth_m = second_depth_cm / 100.0f;

  connectToWiFi();
  client.setServer("192.168.4.1", 1883);
  client.setCallback(callback);

  String ready_msg = "{\"status\":\"ready\"}";
  if (client.publish(status_topic, ready_msg.c_str())) {
    Serial.println("Sent READY signal to control station");
  }

  Serial.println("Onboard Float System Ready - Waiting for DEPLOY signal");
}

// ======================
// Loop
// ======================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  handleButtonPress();
  processStateMachine();
  delay(100);
}

// ======================
// Callback for MQTT
// ======================
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received command: ");
  Serial.println(message);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }

  String command = doc["command"];
  if (command == "deploy") {
    received_deploy_signal = true;
    Serial.println("Received DEPLOY signal");
  } else if (command == "send_now") {
    received_send_now_signal = true;
    Serial.println("Received SEND_NOW signal");
  }
}

// ======================
// State Machine
// ======================
void processStateMachine() {
  switch (current_state) {
    case STATE_WAITING_READY:
      if (client.connected()) {
        current_state = STATE_WAITING_DEPLOY;
        Serial.println("State: Waiting for DEPLOY signal");
      }
      break;

    case STATE_WAITING_DEPLOY:
      if (received_deploy_signal) {
        current_state = STATE_RECORDING;
        state_start_time = millis();
        next_sample_time = millis();
        recordSample();
        Serial.println("State: RECORDING - Started data collection");
      }
      break;

    case STATE_RECORDING:
      if (millis() >= next_sample_time) {
        recordSample();
        next_sample_time = millis() + 4000;
      }
      if (millis() - state_start_time >= 10000) {
        current_state = STATE_DESCEND_TO_SECOND_DEPTH;
        state_start_time = millis();
        Serial.println("State: DESCENDING to second depth");
      }
      break;

    case STATE_DESCEND_TO_SECOND_DEPTH:
      controlBuoyancyForDepth(second_depth_m);
      if (abs(getCurrentDepth() - second_depth_m) <= 0.05) {
        current_state = STATE_MAINTAIN_SECOND_DEPTH;
        state_start_time = millis();
        Serial.println("State: MAINTAINING second depth");
      }
      break;

    case STATE_MAINTAIN_SECOND_DEPTH:
      if (millis() - state_start_time >= (second_depth_time_sec * 1000UL)) {
        current_state = STATE_ASCEND_TO_FIRST_DEPTH;
        state_start_time = millis();
        Serial.println("State: ASCENDING to first depth");
      }
      break;

    case STATE_ASCEND_TO_FIRST_DEPTH:
      controlBuoyancyForDepth(first_depth_m);
      if (abs(getCurrentDepth() - first_depth_m) <= 0.05) {
        current_state = STATE_MAINTAIN_FIRST_DEPTH;
        state_start_time = millis();
        Serial.println("State: MAINTAINING first depth");
      }
      break;

    case STATE_MAINTAIN_FIRST_DEPTH:
      if (millis() - state_start_time >= (first_depth_time_sec * 1000UL)) {
        current_state = STATE_SECOND_PROFILE_START;
        state_start_time = millis();
        Serial.println("State: Starting SECOND PROFILE");
      }
      break;

    case STATE_SECOND_PROFILE_START:
      current_state = STATE_SECOND_DESCEND;
      state_start_time = millis();
      Serial.println("State: SECOND DESCENT");
      break;

    case STATE_SECOND_DESCEND:
      controlBuoyancyForDepth(second_depth_m);
      if (abs(getCurrentDepth() - second_depth_m) <= 0.05) {
        current_state = STATE_SECOND_MAINTAIN_SECOND;
        state_start_time = millis();
        Serial.println("State: MAINTAINING second depth (2nd time)");
      }
      break;

    case STATE_SECOND_MAINTAIN_SECOND:
      if (millis() - state_start_time >= (second_depth_time_sec * 1000UL)) {
        current_state = STATE_SECOND_ASCEND;
        state_start_time = millis();
        Serial.println("State: SECOND ASCEND");
      }
      break;

    case STATE_SECOND_ASCEND:
      controlBuoyancyForDepth(first_depth_m);
      if (abs(getCurrentDepth() - first_depth_m) <= 0.05) {
        current_state = STATE_SECOND_MAINTAIN_FIRST;
        state_start_time = millis();
        Serial.println("State: MAINTAINING first depth (2nd time)");
      }
      break;

    case STATE_SECOND_MAINTAIN_FIRST:
      if (millis() - state_start_time >= (first_depth_time_sec * 1000UL)) {
        current_state = STATE_RETURN_TO_SURFACE;
        state_start_time = millis();
        Serial.println("State: RETURNING TO SURFACE");
      }
      break;

    case STATE_RETURN_TO_SURFACE:
      controlBuoyancyForSurface();
      if (getCurrentDepth() <= 0.05) {
        current_state = STATE_WAITING_SEND;
        String msg = "{\"status\":\"ready_to_send\"}";
        client.publish(status_topic, msg.c_str());
        Serial.println("State: WAITING SEND - At surface");
      }
      break;

    case STATE_WAITING_SEND:
      if (received_send_now_signal) {
        current_state = STATE_TRANSMITTING_DATA;
        Serial.println("State: TRANSMITTING DATA");
      }
      break;

    case STATE_TRANSMITTING_DATA:
      transmitAllData();
      Serial.println("Data transmission completed");
      break;
  }
}

// ======================
// Record Sample + Teleplot
// ======================
void recordSample() {
  if (sample_count >= MAX_SAMPLES) {
    Serial.println("Sample buffer full!");
    return;
  }

  sensor.read(); // Returns void — always call before accessing values
  if (isnan(sensor.pressure()) || isnan(sensor.temperature())) {
    Serial.println("Invalid sensor reading!");
    return;
  }

  sensor_data[sample_count].pressure_kpa = sensor.pressure();
  sensor_data[sample_count].temperature_c = sensor.temperature();
  sensor_data[sample_count].depth_meters = calculateDepth(sensor_data[sample_count].pressure_kpa);
  sensor_data[sample_count].timestamp = millis();

  // Send to Teleplot
  Serial.printf("teleplot:time,%lu,pressure,%f\n",
                sensor_data[sample_count].timestamp,
                sensor_data[sample_count].pressure_kpa);
  Serial.printf("teleplot:time_depth,%lu,depth,%f\n",
                sensor_data[sample_count].timestamp,
                sensor_data[sample_count].depth_meters);
  Serial.printf("teleplot:time_temp,%lu,temperature,%f\n",
                sensor_data[sample_count].timestamp,
                sensor_data[sample_count].temperature_c);

  sample_count++;
}

// ======================
// Buoyancy Control
// ======================
void controlBuoyancyForDepth(float target_depth) {
  float current = getCurrentDepth();
  if (current < target_depth - 0.05) {
    digitalWrite(STEPPER_DIR_PIN, LOW); // Sink
    for (int i = 0; i < 10; i++) {
      digitalWrite(STEPPER_STEP_PIN, HIGH);
      delayMicroseconds(2000);
      digitalWrite(STEPPER_STEP_PIN, LOW);
      delayMicroseconds(2000);
    }
  } else if (current > target_depth + 0.05) {
    digitalWrite(STEPPER_DIR_PIN, HIGH); // Rise
    for (int i = 0; i < 10; i++) {
      digitalWrite(STEPPER_STEP_PIN, HIGH);
      delayMicroseconds(2000);
      digitalWrite(STEPPER_STEP_PIN, LOW);
      delayMicroseconds(2000);
    }
  }
}

void controlBuoyancyForSurface() {
  digitalWrite(STEPPER_DIR_PIN, HIGH);
  for (int i = 0; i < 500; i++) {
    digitalWrite(STEPPER_STEP_PIN, HIGH);
    delayMicroseconds(1000);
    digitalWrite(STEPPER_STEP_PIN, LOW);
    delayMicroseconds(1000);
  }
}

// ======================
// Depth Calculation
// ======================
float getCurrentDepth() {
  sensor.read();
  if (isnan(sensor.pressure())) {
    Serial.println("Error: Invalid pressure reading");
    return 0.0;
  }
  return calculateDepth(sensor.pressure());
}

float calculateDepth(float pressure_kpa) {
  return (pressure_kpa - surface_pressure_kPa) * 0.102f;
}

// ======================
// Data Transmission
// ======================
void transmitAllData() {
  for (int i = 0; i < sample_count && i < MAX_SAMPLES; i++) {
    String packet = "{";
    packet += "\"company_id\":\"RANGER01\",";
    packet += "\"timestamp\":" + String(sensor_data[i].timestamp) + ",";
    packet += "\"pressure\":" + String(sensor_data[i].pressure_kpa) + ",";
    packet += "\"depth\":" + String(sensor_data[i].depth_meters) + ",";
    packet += "\"temperature\":" + String(sensor_data[i].temperature_c);
    packet += "}";
    
    if (!client.publish(data_topic, packet.c_str())) {
      Serial.println("Failed to send packet " + String(i));
    }
    delay(100);
  }
}

// ======================
// Button & Network Helpers
// ======================
void handleButtonPress() {
  static bool last_state = HIGH;
  bool current = digitalRead(BUTTON_PIN);
  if (last_state == HIGH && current == LOW) {
    if (current_state == STATE_WAITING_DEPLOY) {
      received_deploy_signal = true;
    } else if (current_state == STATE_WAITING_SEND) {
      received_send_now_signal = true;
    }
  }
  last_state = current;
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String id = "FloatClient-" + String(random(0xffff), HEX);
    if (client.connect(id.c_str())) {
      client.subscribe(command_topic);
      Serial.println("MQTT connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5s");
      delay(5000);
    }
  }
}