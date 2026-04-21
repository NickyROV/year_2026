#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// --- SHARED STRUCTURES (PACKED) ---
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
    float target_fd;
    float target_sd;
    int fdt;
    int sdt;
} struct_command;

typedef struct __attribute__((packed)) {
    char msg[32];
} struct_status;

// ============================================================================
// ON-THE-SPOT PARAMETERS (Update these before uploading)
// ============================================================================
char my_company_id[10] = "Ranger02"; // Provided by MATE [cite: 80]
float my_target_fd     = 2.50;       // Target 1: 2.5m [cite: 34, 107]
float my_target_sd     = 0.40;       // Target 2: 40cm [cite: 34, 111]
int my_fdt             = 30;         // Hold 1: 30 seconds [cite: 34]
int my_sdt             = 30;         // Hold 2: 30 seconds [cite: 34]
// ============================================================================

#define DEPLOY_BTN 15
#define SEND_BTN 16
#define PREDIVE_BTN 17 

uint8_t floatMac[] = {0xAC, 0xA7, 0x04, 0x29, 0x86, 0x44};
bool predive_confirmed = false;

void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(struct_status)) {
        struct_status status;
        memcpy(&status, incomingData, sizeof(status));
        Serial.print(">>> [STATUS RECEIVED]: "); Serial.println(status.msg);
        
        if (strcmp(status.msg, "Ready: 0x76 OK") == 0) {
            Serial.println(">>> STEP 1: Press 'Pre-dive Transmission' (Pin 17) <<<");
        }
    } 
    else if (len == sizeof(struct_message)) {
        struct_message data;
        memcpy(&data, incomingData, sizeof(data));

        if (!predive_confirmed) {
            // PHASE 1: PRE-DIVE VERIFICATION (For the Mission Judge)
            Serial.println("--- PRE-DIVE VERIFICATION ---");
            Serial.print("Company ID: "); Serial.println(data.company_id);
            Serial.print("Pressure: "); Serial.print(data.pressure_kpa); Serial.println(" kPa");
            Serial.print("Depth: "); Serial.print(data.depth_m); Serial.println(" m");
            Serial.println("-----------------------------");
            
            predive_confirmed = true;
            Serial.println(">>> STEP 2: Pre-dive OK. Press 'Deploy' (Pin 15) to dive <<<");
        } 
        else {
            // PHASE 2: DATA RECOVERY (Teleplot Format for VSCode)
            // Logic: Meters to negative centimeters for depth-profile visualization
            Serial.print(">Depth_cm:");
            Serial.print(data.depth_m * -100.0f); 
            Serial.print("|u:");
            Serial.println(data.timestamp);        
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(DEPLOY_BTN, INPUT_PULLUP);
    pinMode(SEND_BTN, INPUT_PULLUP);
    pinMode(PREDIVE_BTN, INPUT_PULLUP);
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, floatMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    
    Serial.println("--- Control Station Ready ---");
    Serial.print("Active Company ID: "); Serial.println(my_company_id);
}

void loop() {
    // 1. Pre-dive Command (Uses global ID)
    if (digitalRead(PREDIVE_BTN) == LOW) {
        struct_command cmd;
        strcpy(cmd.cmd, "predive");
        strcpy(cmd.company_id, my_company_id); 
        esp_now_send(floatMac, (uint8_t *) &cmd, sizeof(cmd));
        delay(1000);
    }

    // 2. Deploy Command (Uses all global parameters)
    if (digitalRead(DEPLOY_BTN) == LOW && predive_confirmed) {
        struct_command cmd;
        strcpy(cmd.cmd, "deploy");
        strcpy(cmd.company_id, my_company_id);
        cmd.target_fd = my_target_fd; 
        cmd.target_sd = my_target_sd; 
        cmd.fdt       = my_fdt;       
        cmd.sdt       = my_sdt;       
        
        esp_now_send(floatMac, (uint8_t *) &cmd, sizeof(cmd));
        Serial.println(">>> Mission Config Sent to Float");
        Serial.printf("ID:%s | FD:%.2f | SD:%.2f | FDT:%d | SDT:%d\n", 
                      my_company_id, my_target_fd, my_target_sd, my_fdt, my_sdt);
        delay(1000);
    }

    // 3. Data Recovery Command
    if (digitalRead(SEND_BTN) == LOW) {
        struct_command cmd = {"send_now"};
        esp_now_send(floatMac, (uint8_t *) &cmd, sizeof(cmd));
        Serial.println(">>> Requesting Log Data...");
        delay(1000);
    }
}
