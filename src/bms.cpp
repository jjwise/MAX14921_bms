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
// extern "C" {
// #include "user_interface.h"
// }

#define SHUNT_ADC_ADDR 0x4B

const float ISO_GAIN = 8.2;
const uint8_t MAX_CS1 = 26;
const uint8_t MAX_CS2 = 27;
const uint8_t MAX_EN1 = 3;
const uint8_t MAX_EN2 = 2;
const uint8_t IGNITION_PIN = 12; //also tdi
const uint8_t CHARGE_PIN = 13; //also tck
const uint8_t WIFI_RETRIES = 30;


AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");
Adafruit_ADS1115 shunt_adc;
//instance of max14921 supports two 15 cell packs
MAX14921 max14921(MAX_CS1, MAX_CS2, MAX_EN1, MAX_EN2);
bms_status_t bms_status;

const char *ssid = "Ute";
const char *password = NULL;
const int SHUNT_RESISTANCE = 125; //in u ohms
const size_t CAPACITY = JSON_ARRAY_SIZE(NUM_CELLS * 3);

enum state{DRIVING, CHARGING, STANDBY} STATE;

uint8_t current_ignition_state = 0;
uint8_t current_charge_state = 1; //inverse for off

bool wifi_off() {
    int conn_tries = 0;

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);

    while ((WiFi.status() == WL_CONNECTED)
        && (conn_tries++ < WIFI_RETRIES)) {
        delay(100);
        #ifdef DEBUG
            Serial.print('.');
        #endif
    }
    if (WiFi.status() != WL_CONNECTED) {
        return true;
    } else {
        return false;
    }
}


bool wifi_on() {
    int conn_tries = 0;

    WiFi.softAP(ssid, password);

    while ((WiFi.status() != WL_CONNECTED)
        && (conn_tries++ < WIFI_RETRIES)) {
        delay(100);
        #ifdef DEBUG
            Serial.print('.');
        #endif
    }
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    } else {
        return false;
    }
}

//polls ignition GPIO every 250ms, changes state from charging to driving 
//if change from low -> high and vice versa. Tried to do with interrupt but
//ignition is a bit bouncy and too tired to do debouncing.
void poll_ignition() {
    uint8_t ignition_state = digitalRead(IGNITION_PIN);
    if(!current_ignition_state && ignition_state) {
        Serial.printf("State change: %d -> ", STATE);
        STATE = DRIVING;
        max14921.wake();
        max14921.reset_balance();
        wifi_on();
        Serial.println(STATE);

    } else if(current_ignition_state && !ignition_state) {
        Serial.printf("State change: %d -> ", STATE);
        STATE = STANDBY;
        max14921.sleep();
        wifi_off();
        Serial.println(STATE);
    }

    current_ignition_state = ignition_state; 
}

void poll_charge() {
    uint8_t charge_state = digitalRead(CHARGE_PIN);
    //charge active pin driven low
    if(current_charge_state && !charge_state) {
        Serial.printf("State change: %d -> ", STATE);
        STATE = CHARGING;
        max14921.wake();
        wifi_on();
        Serial.println(STATE);

    } else if(!current_charge_state && charge_state) {
        Serial.printf("State change: %d -> ", STATE);
        STATE = STANDBY;
        max14921.sleep();
        wifi_off();
        Serial.println(STATE);
    }

    current_charge_state = charge_state; 
}

//j1772 charger is pluged in and on
// void charge_interrupt() {
//     uint8_t charge_state = digitalRead(CHARGE_PIN);

//     Serial.printf("State change: %d -> ", STATE);
//     if(!charge_state) {
//         STATE = CHARGING;
//         max14921.wake();
//         wifi_on();
//     } else {
//         STATE = STANDBY;
//         max14921.sleep();
//         wifi_off();
//     }
//     Serial.println(STATE);
// }

float measure_current() {
    //read adc voltage at current sense pin, convert to current
    //use instance of adc associated with corect address
    long adc_value = shunt_adc.readADC_Differential_2_3();
    float current = to_voltage(adc_value) / SHUNT_RESISTANCE * 1000000 / ISO_GAIN; // some maths

    return current;
}


void send_data_ws() {
    StaticJsonDocument<CAPACITY> doc;
    JsonArray cell_array = doc.to<JsonArray>();

    //populate array with cell voltages
    for(int j = 0; j < NUM_PACKS; j++) {
        for(int i = 0; i < NUM_CELLS; i++) {

            float cell_voltage = max14921.get_cell_voltage(j, i);
            char cell_voltage_buff[10];
            sprintf(cell_voltage_buff, "%.2f", cell_voltage);
            cell_array.add(cell_voltage_buff);
        }
    }

    float current = measure_current();
    char current_buff[10];
    sprintf(current_buff, "%.2f", current);
    cell_array.add(current_buff);

    float pack_voltage = max14921.get_pack_voltage();
    char pack_voltage_buff[10];
    sprintf(pack_voltage_buff, "%.2f", pack_voltage);
    cell_array.add(pack_voltage_buff);
    
    //don't actually need 300 chars
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
        #ifdef DEBUG
        Serial.println("Starting CAN failed!");
        #endif
        while (1);
    }

    CAN.setPins(CAN_RX, CAN_TX); //rx tx
    pinMode(CHARGE_PIN, INPUT);
    pinMode(IGNITION_PIN, INPUT);

    //interrupts on pins for state changes
    //attachInterrupt(IGNITION_PIN, ignition_interrupt, CHANGE);
    //attachInterrupt(CHARGE_PIN, charge_interrupt, CHANGE);

    ledcSetup(CHANNEL, FREQ, RESOLUTION);
    ledcAttachPin(GUAGE_PIN, CHANNEL);

    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    #ifdef DEBUG
    Serial.print("AP IP address: ");
    Serial.println(IP);
    #endif

    if (MDNS.begin("esp32")) {
        #ifdef DEBUG
        Serial.println("MDNS responder started");
        #endif
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

    //STATE = CHARGING;
    //STATE = DRIVING;
    STATE = STANDBY;
    max14921.sleep();
}

//put max14921 into smaple phase
//give caps some time to settle at cell voltage, 500ms is definitely too much
//read voltages and send to client over websockets
void loop() {
    //need a loop output going to evcc that checks for cell under/overvoltage
    float pack_voltage = 0;

    if (STATE == DRIVING || STATE == CHARGING) {
        pack_voltage = max14921.get_pack_voltage();
        max14921.record_cell_voltages();
        //send bms data to client through websocket
    }
    
    //is there a way to combine if above ^ with switch below??
    switch(STATE) {
        case DRIVING: {
            int battery_percent = voltage_to_percentage(pack_voltage);
            bool undervolt = max14921.under_voltage();
            //set battery guage to 0 if a single cell is under the lower threshold
            set_battery_guage(battery_percent * !undervolt);
            send_data_ws();
        } break;
        case CHARGING: {
            //balance cells to avoid individual cell overcharging
            max14921.balance_cells();
            set_bms_status(&bms_status, &max14921);
            //send can message at least once a second
            //send_can_evcc(&bms_status);
        } break;
        case STANDBY: {
            //low power mode
        } break;
    }

    poll_ignition();
    poll_charge();

    delay(250);
}