#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Ruwan";
const char* password = "12345678";

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Pin definitions for 9 bulbs/LEDs
const int bulbPins[9] = {13,12,14,27,26,25,33,32,23};

// Bulb states (true = on, false = off)
bool bulbStates[9] = {true, true, true, true, true, true, true, true, true};

void setup() {
  Serial.begin(115200);
  
  // Initialize bulb pins as outputs
  for (int i = 0; i < 9; i++) {
    pinMode(bulbPins[i], OUTPUT);
    digitalWrite(bulbPins[i], HIGH); // Start with all bulbs ON (matching your HTML default)
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected to WiFi! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("WebSocket server started on port 81");
  Serial.println("Update your HTML file with this IP address:");
  Serial.print("ws://");
  Serial.print(WiFi.localIP());
  Serial.println(":81");
}

void loop() {
  webSocket.loop();
  
  // Optional: Add a heartbeat or status update every 30 seconds
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    sendStatusUpdate();
    lastHeartbeat = millis();
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client [%u] disconnected\n", num);
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("Client [%u] connected from %d.%d.%d.%d\n", 
                    num, ip[0], ip[1], ip[2], ip[3]);
      
      // Send current status to newly connected client
      sendStatusToClient(num);
      break;
    }
    
    case WStype_TEXT: {
      Serial.printf("Received from client [%u]: %s\n", num, payload);
      
      // Parse JSON message
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
      }
      
      // Extract sensor and action
      String sensor = doc["sensor"];
      String action = doc["action"];
      
      if (sensor.startsWith("bulb") && (action == "on" || action == "off")) {
        int bulbNumber = sensor.substring(4).toInt(); // Extract number from "bulb1", "bulb2", etc.
        
        if (bulbNumber >= 1 && bulbNumber <= 9) {
          int bulbIndex = bulbNumber - 1; // Convert to 0-based index
          bool newState = (action == "on");
          
          // Update bulb state and hardware
          bulbStates[bulbIndex] = newState;
          digitalWrite(bulbPins[bulbIndex], newState ? HIGH : LOW);
          
          Serial.printf("Bulb %d turned %s\n", bulbNumber, newState ? "ON" : "OFF");
          
          // Broadcast the state change to all connected clients
          broadcastBulbState(bulbNumber, newState);
        }
      }
      break;
    }
    
    case WStype_BIN:
      Serial.printf("Received binary data from client [%u]\n", num);
      break;
      
    default:
      break;
  }
}

void broadcastBulbState(int bulbNumber, bool state) {
  DynamicJsonDocument doc(200);
  doc["sensor"] = "bulb" + String(bulbNumber);
  doc["action"] = state ? "on" : "off";
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  
  webSocket.broadcastTXT(message);
}

void sendStatusToClient(uint8_t clientNum) {
  // Send current state of all bulbs to a specific client
  for (int i = 0; i < 9; i++) {
    DynamicJsonDocument doc(200);
    doc["sensor"] = "bulb" + String(i + 1);
    doc["action"] = bulbStates[i] ? "on" : "off";
    doc["timestamp"] = millis();
    
    String message;
    serializeJson(doc, message);
    
    webSocket.sendTXT(clientNum, message);
    delay(10); // Small delay between messages
  }
}

void sendStatusUpdate() {
  // Send periodic status update with system info
  DynamicJsonDocument doc(300);
  doc["type"] = "status";
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["connected_clients"] = webSocket.connectedClients();
  
  String message;
  serializeJson(doc, message);
  
  webSocket.broadcastTXT(message);
}

// Function to manually control bulbs (for testing or other triggers)
void setBulb(int bulbNumber, bool state) {
  if (bulbNumber >= 1 && bulbNumber <= 9) {
    int bulbIndex = bulbNumber - 1;
    bulbStates[bulbIndex] = state;
    digitalWrite(bulbPins[bulbIndex], state ? HIGH : LOW);
    broadcastBulbState(bulbNumber, state);
  }
}

// Function to turn all bulbs on or off
void setAllBulbs(bool state) {
  for (int i = 0; i < 9; i++) {
    bulbStates[i] = state;
    digitalWrite(bulbPins[i], state ? HIGH : LOW);
    broadcastBulbState(i + 1, state);
    delay(50); // Small delay for visual effect
  }
}