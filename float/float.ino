// Pin definitions
const int stepPin = 5;
const int dirPin = 2; 
const int enPin = 6;  

// Button and Switch Pins
const int forwardBtn = 3;   
const int backwardBtn = 4;  
const int stepper_switch = 7; 


// State variable: 0 = Only Forward allowed, 1 = Only Backward allowed
bool stepper_state = 0; 

void setup() {
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT); 
  pinMode(enPin, OUTPUT);  
  
  pinMode(forwardBtn, INPUT_PULLUP); 
  pinMode(backwardBtn, INPUT_PULLUP);
  pinMode(stepper_switch, INPUT_PULLUP);

  digitalWrite(enPin, LOW); // Enable motor
  delay(100);

  
  // Homing: Run forward until stepper_switch is pressed (connects to GND)
  digitalWrite(dirPin, HIGH); 
  while (digitalRead(stepper_switch) == HIGH) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500);     
    digitalWrite(stepPin, LOW); 
    delayMicroseconds(500);
    // Safety check: if it reads LOW, wait 5ms and check again to confirm
    if (digitalRead(stepper_switch) == LOW) {
      delay(5); 
      if (digitalRead(stepper_switch) == LOW) {
        break; // 
      }
    }       
  }
  // Power up logic: After hitting switch, set state to 1 (Backward move required next)
  stepper_state = 1; 
  delay(1000);
  runSequence(LOW);
  stepper_state = 0;
  delay(1000); 
 }

void loop() {
  // Logic 3: When state is 1, ONLY backward movement is permitted
  if (stepper_state == 1) {
    if (digitalRead(backwardBtn) == LOW) {
      runSequence(LOW);    // Run 800 steps Backward
      stepper_state = 0;   // FLIP STATE: Now only forward is allowed
    }
  } 
  // Logic 4: When state is 0, ONLY forward movement is permitted
  else if (stepper_state == 0) {
    if (digitalRead(forwardBtn) == LOW) {
      runSequence(HIGH);   // Run 800 steps Forward
      stepper_state = 1;   // FLIP STATE: Now only backward is allowed
    }
  }
}

// Function to handle the 800-step movement
void runSequence(int direction) {
  digitalWrite(dirPin, direction);
  
  for (int x = 0; x < 1800; x++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500);     
    digitalWrite(stepPin, LOW); 
    delayMicroseconds(500);     
  }
  
  delay(200); // Debounce to prevent multiple triggers from one press
}