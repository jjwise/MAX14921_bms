/*
Functions for using the oem Hilux fuel guage as a battery guage
*/

// ensure this library description is only included once
#ifndef FUEL_GUAGE_h
#define FUEL_GUAGE_h

#include <Arduino.h>

const float A = 86.77;
const float B = 0.001649;
const float C = -25.79;
const float D = -0.02403;
const float E = 2.71828;

const uint8_t GUAGE_PIN = 15; //currently using TMS pin
const uint8_t MIN_PACK_VOLTAGE = 110;
const uint8_t MAX_PACK_VOLTAGE = 123;

const uint8_t CHANNEL = 0;
const uint8_t RESOLUTION = 8;
const int FREQ = 5000;


//recieves pack voltage and returns battery percentage
int voltage_to_percentage(float pack_voltage);

//recieves battery percentage and sets the battery guage.
void set_battery_guage(float percentage);

#endif