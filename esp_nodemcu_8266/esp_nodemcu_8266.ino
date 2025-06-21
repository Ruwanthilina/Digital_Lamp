#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Ruwan";
const char* password = "12345678";

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// NodeMCU labeled pins (D1-D8, D0)
const int bulbPins[9] = {D1, D2, D3, D4, D5, D6, D7, D8, D0}; // GPIOs: 5,4,0,2,14,12,13,15,16

// Bulb states (true = ON, false = OFF)
bool bulbStates[9] = {true, true, true, true, true, true, true, true, true};

void setup() {
  Serial.begin(115200);

  // Initialize pins
  for (int i = 0; i < 9; i++) {
    pinMode(bulbPins[i], OUTPUT);
    digitalWrite(bulbPins[i], HIGH); // Start ON
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
  Serial.print("ws://");
  Serial.print(WiFi.localIP());
  Serial.println(":81");
}

void loop() {
  webSocket.loop();

  // Optional heartbeat every 30s
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    sendStatusUpdate();
    lastHeartbeat = millis();
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client [%u] disconnected\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("Client [%u] connected from %d.%d.%d.%d\n", 
                    num, ip[0], ip[1], ip[2], ip[3]);
      sendStatusToClient(num);
      break;
    }

    case WStype_TEXT: {
      Serial.printf("Received from client [%u]: %s\n", num, payload);

      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
      }

      String sensor = doc["sensor"];
      String action = doc["action"];

      if (sensor.startsWith("bulb") && (action == "on" || action == "off")) {
        int bulbNumber = sensor.substring(4).toInt();

        if (bulbNumber >= 1 && bulbNumber <= 9) {
          int bulbIndex = bulbNumber - 1;
          bool newState = (action == "on");

          bulbStates[bulbIndex] = newState;
          digitalWrite(bulbPins[bulbIndex], newState ? HIGH : LOW);

          Serial.printf("Bulb %d turned %s\n", bulbNumber, newState ? "ON" : "OFF");

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
  for (int i = 0; i < 9; i++) {
    DynamicJsonDocument doc(200);
    doc["sensor"] = "bulb" + String(i + 1);
    doc["action"] = bulbStates[i] ? "on" : "off";
    doc["timestamp"] = millis();

    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(clientNum, message);
    delay(10); // Avoid congestion
  }
}

void sendStatusUpdate() {
  DynamicJsonDocument doc(300);
  doc["type"] = "status";
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["connected_clients"] = webSocket.connectedClients();

  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
}

void setBulb(int bulbNumber, bool state) {
  if (bulbNumber >= 1 && bulbNumber <= 9) {
    int bulbIndex = bulbNumber - 1;
    bulbStates[bulbIndex] = state;
    digitalWrite(bulbPins[bulbIndex], state ? HIGH : LOW);
    broadcastBulbState(bulbNumber, state);
  }
}

void setAllBulbs(bool state) {
  for (int i = 0; i < 9; i++) {
    bulbStates[i] = state;
    digitalWrite(bulbPins[i], state ? HIGH : LOW);
    broadcastBulbState(i + 1, state);
    delay(50);
  }
}
