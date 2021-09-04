/*
Functions for using the oem Hilux fuel guage as a battery guage
*/

#include <Arduino.h>
#include <fuel_guage.h>

//recieves pack voltage and returns battery percentage
int voltage_to_percentage(float pack_voltage) {
    //could do an ekf, probs be more lame and do some basic curve fitting
    return 100;
}

//recieves battery percentage and sets the battery guage duty cycle.
void set_battery_guage(float percentage){
    //fuel guage percentage curve in the form y=aexp(bx)+cexp(dx)
    float guage_duty = A * exp(B * percentage) + C * exp(D * percentage) * 2.55; //in range 0-255
    ledcWrite(CHANNEL, guage_duty);
}