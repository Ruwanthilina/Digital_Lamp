#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Lightbulb GPIO pins (change these according to your wiring)
const int bulbPins[9] = {2, 4, 5, 18, 19, 21, 22, 23, 25};

// Bulb states (true = ON, false = OFF)
bool bulbStates[9] = {false, false, false, false, false, false, false, false, false};

// Built-in LED for connection status
const int statusLED = 2;

void setup() {
  Serial.begin(115200);
  
  // Initialize bulb pins as outputs
  for (int i = 0; i < 9; i++) {
    pinMode(bulbPins[i], OUTPUT);
    digitalWrite(bulbPins[i], LOW); // Start with all bulbs OFF
  }
  
  // Initialize status LED
  pinMode(statusLED, OUTPUT);
  digitalWrite(statusLED, LOW);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Blink status LED while connecting
    digitalWrite(statusLED, !digitalRead(statusLED));
  }
  
  Serial.println();
  Serial.print("WiFi connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Turn on status LED when connected
  digitalWrite(statusLED, HIGH);
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("WebSocket server started on port 81");
  Serial.println("Ready to receive commands!");
  
  // Print pin configuration
  Serial.println("\n--- Pin Configuration ---");
  for (int i = 0; i < 9; i++) {
    Serial.printf("Bulb %d: GPIO %d\n", i + 1, bulbPins[i]);
  }
  Serial.println("------------------------\n");
}

void loop() {
  webSocket.loop();
  
  // Keep WiFi connection alive
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
    digitalWrite(statusLED, LOW);
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      digitalWrite(statusLED, !digitalRead(statusLED));
    }
    
    Serial.println("\nWiFi reconnected!");
    digitalWrite(statusLED, HIGH);
  }
  
  delay(10); // Small delay to prevent watchdog issues
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client %u disconnected\n", num);
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("Client %u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      
      // Send current bulb states to newly connected client
      sendAllBulbStates(num);
      break;
    }
    
    case WStype_TEXT: {
      Serial.printf("Received from client %u: %s\n", num, payload);
      
      // Parse JSON message
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
      }
      
      // Handle bulb commands
      if (doc.containsKey("sensor") && doc.containsKey("action")) {
        String sensor = doc["sensor"];
        String action = doc["action"];
        
        if (sensor.startsWith("bulb")) {
          int bulbNumber = sensor.substring(4).toInt(); // Extract number from "bulb1", "bulb2", etc.
          
          if (bulbNumber >= 1 && bulbNumber <= 9) {
            handleBulbCommand(bulbNumber, action, num);
          } else {
            Serial.printf("Invalid bulb number: %d\n", bulbNumber);
          }
        }
      }
      
      // Handle other sensor commands (from your original code)
      else if (doc.containsKey("sensor")) {
        String sensor = doc["sensor"];
        handleOtherSensors(sensor, num);
      }
      
      break;
    }
    
    case WStype_BIN:
      Serial.printf("Received binary data from client %u\n", num);
      break;
      
    default:
      break;
  }
}

void handleBulbCommand(int bulbNumber, String action, uint8_t clientNum) {
  int bulbIndex = bulbNumber - 1; // Convert to 0-based index
  
  if (action == "on") {
    digitalWrite(bulbPins[bulbIndex], HIGH);
    bulbStates[bulbIndex] = true;
    Serial.printf("Bulb %d turned ON (GPIO %d)\n", bulbNumber, bulbPins[bulbIndex]);
  } 
  else if (action == "off") {
    digitalWrite(bulbPins[bulbIndex], LOW);
    bulbStates[bulbIndex] = false;
    Serial.printf("Bulb %d turned OFF (GPIO %d)\n", bulbNumber, bulbPins[bulbIndex]);
  }
  
  // Send confirmation back to all connected clients
  DynamicJsonDocument response(200);
  response["sensor"] = "bulb" + String(bulbNumber);
  response["action"] = action;
  response["status"] = "success";
  
  String responseStr;
  serializeJson(response, responseStr);
  webSocket.broadcastTXT(responseStr);
}

void handleOtherSensors(String sensor, uint8_t clientNum) {
  // Handle other sensors like RGB, buzzer, etc.
  Serial.printf("Handling sensor: %s\n", sensor.c_str());
  
  // Example: RGB LED control
  if (sensor == "red" || sensor == "green" || sensor == "blue") {
    Serial.printf("RGB command received: %s\n", sensor.c_str());
    // Add your RGB LED control code here
  }
  
  // Add more sensor handling as needed
}

void sendAllBulbStates(uint8_t clientNum) {
  for (int i = 0; i < 9; i++) {
    DynamicJsonDocument doc(200);
    doc["sensor"] = "bulb" + String(i + 1);
    doc["action"] = bulbStates[i] ? "on" : "off";
    doc["status"] = "current_state";
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(clientNum, message);
    
    delay(10); // Small delay between messages
  }
}

// Function to control all bulbs at once
void controlAllBulbs(bool state) {
  for (int i = 0; i < 9; i++) {
    digitalWrite(bulbPins[i], state ? HIGH : LOW);
    bulbStates[i] = state;
  }
  
  // Broadcast state to all clients
  DynamicJsonDocument response(200);
  response["sensor"] = "all_bulbs";
  response["action"] = state ? "on" : "off";
  response["status"] = "success";
  
  String responseStr;
  serializeJson(response, responseStr);
  webSocket.broadcastTXT(responseStr);
  
  Serial.printf("All bulbs turned %s\n", state ? "ON" : "OFF");
}

// Function to create lighting patterns
void lightingPattern(int pattern) {
  switch (pattern) {
    case 1: // Sequential on
      for (int i = 0; i < 9; i++) {
        digitalWrite(bulbPins[i], HIGH);
        bulbStates[i] = true;
        delay(200);
        
        // Send update to clients
        DynamicJsonDocument doc(200);
        doc["sensor"] = "bulb" + String(i + 1);
        doc["action"] = "on";
        doc["status"] = "pattern";
        
        String message;
        serializeJson(doc, message);
        webSocket.broadcastTXT(message);
      }
      break;
      
    case 2: // Blink all
      for (int j = 0; j < 3; j++) {
        controlAllBulbs(true);
        delay(500);
        controlAllBulbs(false);
        delay(500);
      }
      break;
      
    case 3: // Checkerboard pattern
      // Pattern 1
      for (int i = 0; i < 9; i++) {
        bool state = (i % 2 == 0);
        digitalWrite(bulbPins[i], state ? HIGH : LOW);
        bulbStates[i] = state;
      }
      delay(1000);
      
      // Pattern 2 (inverse)
      for (int i = 0; i < 9; i++) {
        bool state = (i % 2 == 1);
        digitalWrite(bulbPins[i], state ? HIGH : LOW);
        bulbStates[i] = state;
      }
      delay(1000);
      
      // Turn all off
      controlAllBulbs(false);
      break;
  }
}

// You can call this function for testing
void testAllBulbs() {
  Serial.println("Testing all bulbs...");
  
  for (int i = 0; i < 9; i++) {
    Serial.printf("Testing bulb %d (GPIO %d)\n", i + 1, bulbPins[i]);
    digitalWrite(bulbPins[i], HIGH);
    delay(300);
    digitalWrite(bulbPins[i], LOW);
    delay(100);
  }
  
  Serial.println("Bulb test completed!");
}