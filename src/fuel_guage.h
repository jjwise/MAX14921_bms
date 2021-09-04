/*
Functions for using the oem Hilux fuel guage as a battery guage
*/

// ensure this library description is only included once
#ifndef FUEL_GUAGE_h
#define FUEL_GUAGE_h

#include <Arduino.h>

#define A 86.77
#define B 0.001649
#define C -25.79
#define D -0.02403
#define GUAGE_PIN 10 //change

const int FREQ = 5000;
const int CHANNEL = 0;
const int RESOLUTION = 8;

//recieves pack voltage and returns battery percentage
int voltage_to_percentage(float pack_voltage);

//recieves battery percentage and sets the battery guage.
void set_battery_guage(float percentage);

#endif