/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-websocket-server-arduino/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <Arduino.h>
#include "LittleFS.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "credentials.h"

// Replace with your network credentials
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";

uint32_t elapsed = 0;
uint32_t timer = 0;

#define COUNTDOWN_TIMEOUT_MS 5000
uint32_t countdown = 0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ----------------------------------------------------------------------------
// Definition of the LED component
// ----------------------------------------------------------------------------

struct Led {
    // state variables
    uint8_t pin;
    bool    on;

    // methods
    void update() {
        digitalWrite(pin, on ? HIGH : LOW);
    }
};

// ----------------------------------------------------------------------------
// LittleFS initialization
// ----------------------------------------------------------------------------

Led    onboard_led = { LED_BUILTIN, false };
Led relay_1 = { 15, false };
Led relay_2 = { 12, false };

void initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("Cannot mount LittleFS volume...");
    while (1) {
        onboard_led.on = millis() % 200 < 50;
        onboard_led.update();
    }
  }
  Serial.println("LittleFS volume mounted...");
}

void notifyClients() {
  Serial.println("notifying clients");

  char buffer[40];
  sprintf(
    buffer, 
    "{\"LED\":%s,\"RELAY1\":%s,\"RELAY2\":%s}",
    onboard_led.on ? "true" : "false",
    relay_1.on ? "true" : "false",
    relay_2.on ? "true" : "false"
    );
  ws.textAll(buffer);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  Serial.printf("handle web socket message: %s\n", data);
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    bool notify = false;
    if (strcmp((char*)data, "toggle") == 0) {
      onboard_led.on = !onboard_led.on;
      onboard_led.update();
      notify = true;
    }

    if (relay_1.on || relay_2.on) {
      // skip updates
    }
    else if (strcmp((char*)data, "relay1toggle") == 0) {
      relay_1.on = !relay_1.on;
      relay_1.update();
      notify = true;
      countdown = millis();
    }
    else if (strcmp((char*)data, "relay2toggle") == 0) {
      relay_2.on = !relay_2.on;
      relay_2.update();
      notify = true;
      countdown = millis();
    }

    if (notify) {
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    Serial.println("on event");
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void initWebSocket() {
  Serial.println("init web socket");
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Trying to connect [%s] ", WiFi.macAddress().c_str());
  while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
  }
  Serial.printf(" %s\n", WiFi.localIP().toString().c_str());
}

String processor(const String& var){
  Serial.println(var);

  if(var == "STATE"){
    if (onboard_led.on){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  if (var == "RELAY1STATE") {
    return relay_1.on ? "ON" : "OFF";
  }
  if (var == "RELAY2STATE") {
    return relay_2.on ? "ON" : "OFF";
  }
  if (var == "RELAY1DISABLED") {
    return relay_2.on ? "disabled" : "";
  }
  if (var == "RELAY2DISABLED") {
    return relay_1.on ? "disabled" : "";
  }

  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(onboard_led.pin, OUTPUT);
  onboard_led.on = LOW;
  onboard_led.update();
  //pinMode(ledPin, OUTPUT);
  //digitalWrite(ledPin, LOW);
  
  initLittleFS();
  // Connect to Wi-Fi
  initWiFi();

  initWebSocket();

// Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("on /");
    request->send(LittleFS, "/index.html", "text/html",false,processor);
  });
  server.serveStatic("/", LittleFS, "/");

  // Start server
  Serial.println("starting server");
  server.begin();
}

void loop() {

  elapsed = millis();
  if ((elapsed - timer) > 5000) {
    onboard_led.on = !onboard_led.on;
    timer = elapsed;
    notifyClients();
  }

  if ((elapsed - countdown) > COUNTDOWN_TIMEOUT_MS && countdown != 0) {
    relay_1.on = false;
    relay_2.on = false;
    countdown = 0;
    notifyClients();
    relay_1.update();
    relay_2.update();
  }

  ws.cleanupClients();
  onboard_led.update();
}
