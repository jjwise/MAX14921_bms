/*
Functions for using the oem Hilux fuel guage as a battery guage
*/

#include <Arduino.h>
#include <fuel_guage.h>

//recieves pack voltage and returns battery percentage
int voltage_to_percentage(float pack_voltage) {
    //could do an ekf, probs be more lame and do some basic curve fitting
    float percentage = (pack_voltage - MIN_PACK_VOLTAGE) / (float)(MAX_PACK_VOLTAGE - MIN_PACK_VOLTAGE) * 100; 
    return percentage;
}

//recieves battery percentage and sets the battery guage duty cycle.
void set_battery_guage(float percentage){
    //fuel guage percentage curve in the form y=aexp(bx)+cexp(dx)
    float guage_duty = (A * pow(E, (B * percentage)) + C * pow(E, (D * percentage))) * 2.55; //in range 0-255
    Serial.println(guage_duty);
    ledcWrite(CHANNEL, guage_duty);
}