#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// State machine states
enum ControlState {
  STATE_WAITING_FLOAT_READY,
  STATE_WAITING_USER_DEPLOY,
  STATE_MONITORING_MISSION,
  STATE_WAITING_FLOAT_RECOVERY,
  STATE_WAITING_USER_SEND,
  STATE_RECEIVING_DATA,
  STATE_PLOTTING_DATA
};

// Global variables
ControlState current_state = STATE_WAITING_FLOAT_READY;
unsigned long state_start_time = 0;

// WiFi Access Point configuration
const char* ssid = "float_control";
const char* password = "float_pass";

// MQTT Configuration
WiFiClient espClient;
PubSubClient client(espClient);

// Pin definitions
#define DEPLOY_BUTTON_PIN 15
#define SEND_BUTTON_PIN 16

// Communication topics
const char* status_topic = "float/status";
const char* command_topic = "control/command";
const char* data_topic = "float/data";

// Data storage for plotting
#define MAX_RECEIVED_PACKETS 1000
struct ReceivedData {
  unsigned long timestamp;
  float pressure;
  float depth;
  float temperature;
};
ReceivedData received_data[MAX_RECEIVED_PACKETS];
int received_count = 0;

// Flags
bool float_ready = false;
bool float_ready_to_send = false;

// ===== FUNCTION PROTOTYPES (CRITICAL FIX) =====
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void handleButtonPresses();
void processStateMachine();

void setup() {
  Serial.begin(115200);
  
  // Initialize button pins
  pinMode(DEPLOY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SEND_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize data array
  memset(received_data, 0, sizeof(received_data));
  
  // Create WiFi Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Control Station AP IP address: ");
  Serial.println(IP);
  
  // Setup MQTT - FIX: Use laptop IP instead of "localhost"
  client.setServer("192.168.4.2", 1883);  // ⚠️ CHANGE TO YOUR LAPTOP'S IP ON THIS NETWORK
  client.setCallback(callback);
  
  Serial.println("Control Station Ready - Waiting for float to connect");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Handle button presses
  handleButtonPresses();
  
  // State machine processing
  processStateMachine();
  
  delay(100);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received message on topic [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (strcmp(topic, status_topic) == 0) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.println("Failed to parse status JSON");
      return;
    }
    String status = doc["status"];
    if (status == "ready") {
      float_ready = true;
      Serial.println("Float is READY - Press DEPLOY button to start mission");
    } else if (status == "ready_to_send") {
      float_ready_to_send = true;
      Serial.println("Float is READY TO SEND - Press SEND button to receive data");
    }
  } else if (strcmp(topic, data_topic) == 0) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.println("Failed to parse data JSON");
      return;
    }
    if (received_count < MAX_RECEIVED_PACKETS) {
      received_data[received_count].timestamp = doc["timestamp"];
      received_data[received_count].pressure = doc["pressure"];
      received_data[received_count].depth = doc["depth"];
      received_data[received_count].temperature = doc["temperature"];
      
      Serial.printf("Received time=%lu, pressure=%f, depth=%f, temp=%f\n",
        received_data[received_count].timestamp,
        received_data[received_count].pressure,
        received_data[received_count].depth,
        received_data[received_count].temperature);
      
      // Send to Teleplot
      Serial.printf("teleplot:received_time,%lu,received_pressure,%f\n",
        received_data[received_count].timestamp,
        received_data[received_count].pressure);
      Serial.printf("teleplot:received_time_depth,%lu,received_depth,%f\n",
        received_data[received_count].timestamp,
        received_data[received_count].depth);
      
      received_count++;
    }
  }
}

void processStateMachine() {
  switch(current_state) {
    case STATE_WAITING_FLOAT_READY:
      if (float_ready) {
        current_state = STATE_WAITING_USER_DEPLOY;
        Serial.println("State: WAITING USER DEPLOY - Press DEPLOY button to start mission");
      }
      break;
      
    case STATE_WAITING_USER_DEPLOY:
      // Handled by button press
      break;
      
    case STATE_MONITORING_MISSION:
      Serial.println("State: MONITORING MISSION - Float is executing profile");
      break;
      
    case STATE_WAITING_FLOAT_RECOVERY:
      if (float_ready_to_send) {
        current_state = STATE_WAITING_USER_SEND;
        Serial.println("State: WAITING USER SEND - Press SEND button to receive data");
      }
      break;
      
    case STATE_WAITING_USER_SEND:
      // Handled by button press
      break;
      
    case STATE_RECEIVING_DATA:
      Serial.println("Currently receiving data packets...");
      break;
      
    case STATE_PLOTTING_DATA:
      Serial.println("State: PLOTTING DATA - Data plotted as received.");
      break;
  }
}

void handleButtonPresses() {
  static bool last_deploy_button = HIGH;
  static bool last_send_button = HIGH;
  
  bool current_deploy_button = digitalRead(DEPLOY_BUTTON_PIN);
  bool current_send_button = digitalRead(SEND_BUTTON_PIN);

  // Deploy button pressed (active LOW due to pullup)
  if (last_deploy_button == HIGH && current_deploy_button == LOW) {
    if (current_state == STATE_WAITING_USER_DEPLOY) {
      String deploy_cmd = "{\"command\":\"deploy\"}";
      if (client.publish(command_topic, deploy_cmd.c_str())) {
        Serial.println("Sent DEPLOY command to float");
        current_state = STATE_MONITORING_MISSION;
      }
    }
  }

  // Send button pressed
  if (last_send_button == HIGH && current_send_button == LOW) {
    if (current_state == STATE_WAITING_USER_SEND) {
      String send_cmd = "{\"command\":\"send_now\"}";
      if (client.publish(command_topic, send_cmd.c_str())) {
        Serial.println("Sent SEND_NOW command to float");
        current_state = STATE_RECEIVING_DATA;
      }
    }
  }

  last_deploy_button = current_deploy_button;
  last_send_button = current_send_button;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ControlStation-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(status_topic);
      client.subscribe(data_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}