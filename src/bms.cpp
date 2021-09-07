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
#include <CAN.h>
#include <CAN_evcc.h>

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
//instance of max14921 supports two 15 cell packs
MAX14921 max14921(MAX_CS1, MAX_CS2, MAX_EN1, MAX_EN2);
bms_status_t bms_status;


const char *ssid = "Ute";
const char *password = NULL;
const int SHUNT_RESISTANCE = 150; //in u ohms
const size_t CAPACITY = JSON_ARRAY_SIZE(NUM_CELLS * 3);
const int8_t STATE = 0;

enum {DRIVING = 1, CHARGING = 2, STANDBY = 3};

float measure_current() {
    //read adc voltage at current sense pin, convert to current
    //use instance of adc associated with corect address
    long adc_value = shunt_adc.readADC_Differential_2_3();
    float current = to_voltage(adc_value) / SHUNT_RESISTANCE * 1000000 / AMP_GAIN; // some maths

    return current;
}

//truncates float to specified width and dp, returns char pointer
char * truncate_float(size_t width, uint8_t dp, float num) {
    //use malloc cuase I can, static memory allocation is lame
    float rounded_num = round(num * pow(10, dp)) / pow(10, dp);
    char* temp_buff = (char *)malloc(width + 1); //allow for null byte
    snprintf(temp_buff, width + 1, "%*f", width, rounded_num); 
    return temp_buff;
}


void send_data_ws() {
    StaticJsonDocument<CAPACITY> doc;
    JsonArray cell_array = doc.to<JsonArray>();

    //populate array with cell voltages
    for(int j = 0; j < NUM_PACKS; j++) {
        for(int i = 0; i < NUM_CELLS; i++) {

            float cell_voltage = max14921.get_cell_voltage(j, i);
            char *truncated_cell_voltage = truncate_float(4, 2, cell_voltage); 
            cell_array.add(truncated_cell_voltage);

            free(truncated_cell_voltage);
        }
    }

    float current = measure_current();
    char *truncated_current = truncate_float(7, 2, current); 
    cell_array.add(truncated_current);
    free(truncated_current);
    
    //don't actually need 200 chars, more like 128
    char buff[300];
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
            //send_data_ws();
            break;
        case WS_EVT_DATA:
            Serial.println("request recieved");
            //send_data_ws();
    }
}


void setup() {
    Serial.begin(115200); 
    SPIFFS.begin();
    SPI.begin();//sets cs to output and pull high
    max14921.begin();
    shunt_adc.begin(SHUNT_ADC_ADDR);

    if (!CAN.begin(CAN_RATE)) {
        Serial.println("Starting CAN failed!");
        while (1);
    }

    CAN.setPins(CAN_RX, CAN_TX); //rx tx

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

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("bms_data.html");


    server.on("/styleSheet.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/styleSheet.css", "text/css");
    });

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
    if(STATE == DRIVING) {

    } else if (STATE == CHARGING) {

    } else if (STATE == STANDBY) {

    }
    max14921.record_cell_voltages();
    float pack_voltage = max14921.get_pack_voltage();

    int battery_percent = voltage_to_percentage(pack_voltage);
    set_battery_guage(battery_percent);

    //balance cells to avoid individual cell overcharging
    max14921.balance_cells();

    //send bms data to client through websocket
    send_data_ws();

    //send can message at least once a second, do it twice to be sure
    set_bms_status(&bms_status, &max14921);
    send_can_evcc(&bms_status);
    
    delay(500);
}