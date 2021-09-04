#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <ShiftRegister74HC595.h>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <fuel_guage.h>
#include <MAX14921.h>

#define AMP_GAIN 80
#define MAX_CS1 27
#define MAX_CS2 26
#define MAX_EN1 2
#define MAX_EN2 3
#define SHUNT_ADC_ADDR 0x4B

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");
Adafruit_ADS1115 shunt_adc;
MAX14921 max14921(MAX_CS1, MAX_CS2, MAX_EN1, MAX_EN2);

const char *ssid = "Ute";
const char *password = NULL;
const int SHUNT_RESISTANCE = 150; //in u ohms
const size_t CAPACITY = JSON_ARRAY_SIZE(NUM_CELLS * 2 + 1);

enum {DRIVING, CHARGING, STANDBY};

float measure_current() {
    //read adc voltage at current sense pin, convert to current
    //use instance of adc associated with corect address
    long adc_value = shunt_adc.readADC_Differential_2_3();
    Serial.println(adc_value);
    float current = to_voltage(adc_value) / SHUNT_RESISTANCE * 1000000 / AMP_GAIN; // some maths

    return current;
}


void sendMessage() {
    StaticJsonDocument<CAPACITY> doc;
    JsonArray cell_array = doc.to<JsonArray>();

    //populate array with cell voltages
    for(int j = 0; j < NUM_PACKS; j++) {
        for(int i = 0; i < NUM_CELLS; i++) {
            cell_array.add(max14921.get_cell_voltage(j, i));
        }
    }

    float current = measure_current();
    cell_array.add(current);

    //don't actually need 200 chars, more like 128
    char buff[200];
    serializeJson(doc, buff);
    
    //send char array over websockets
    ws.textAll(buff);
    Serial.println(buff);
}


void webSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void *arg, uint8_t *payload, size_t len) {
    //not really doing anything with ws callback atm
    switch(type) {
        case WS_EVT_DISCONNECT:
            Serial.printf("[%u] Disconnected\n", client->id());
            break;
        case WS_EVT_CONNECT:
            Serial.printf("[%u] Connected", client->id());
            client->ping();
            break;
        case WS_EVT_ERROR:
            //error was received from the other end
            Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)payload);
            break;
        case WS_EVT_PONG:
            //pong message was received (in response to a ping request maybe)
            Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)payload:"");
            //sendMessage();
            break;
        case WS_EVT_DATA:
            Serial.println("request recieved");
            //sendMessage();
    }
}


void setup() {
    Serial.begin(115200); 
    SPIFFS.begin();
    SPI.begin();//sets cs to output and pull high
    max14921.begin();
    shunt_adc.begin(SHUNT_ADC_ADDR);

    ledcSetup(CHANNEL, FREQ, RESOLUTION);
    ledcAttachPin(GUAGE_PIN, CHANNEL);

    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started");
    }

    MDNS.addService("http", "tcp", 80);
    ws.onEvent(webSocketEvent);
    server.addHandler(&ws);
    server.addHandler(&events);

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("esp32_html.html");

    server.on("/jquery-1.11.3.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/jquery-1.11.3.min.js", "text/javascript");
    });

    server.on("/jquery.mobile-1.4.5.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/jquery.mobile-1.4.5.min.js", "text/javascript");
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404);
    });

    server.begin(); 
}


//put max14921 into smaple phase
//give caps some time to settle at cell voltage, 500ms is definitely too much
//read voltages and send to client over websockets
void loop() {
    max14921.record_cell_voltages();
    float pack_voltage = max14921.get_pack_voltage();
    Serial.println(pack_voltage);

    int battery_percent = voltage_to_percentage(pack_voltage);
    set_battery_guage(battery_percent);

    sendMessage();
    
    delay(2000);
}