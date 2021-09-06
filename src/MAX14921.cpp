/*
Allows creation of a max14921 object. Associated methods for recording the individual cell
and pack voltages, interfacing with the max14921 over spi and balancing cells
*/

#include <Arduino.h>
#include <MAX14921.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include <ShiftRegister74HC595.h>

float to_voltage(int adc_val) {
    return (float)adc_val * ADC_CONST;
}

MAX14921::MAX14921(uint8_t cs1, uint8_t cs2, uint8_t en1, uint8_t en2) {
    pack_data[0].cs = cs1;
    pack_data[1].cs = cs2;
    pack_data[0].en = en1;
    pack_data[1].en = en2;
    total_pack_voltage = 0;
}

void MAX14921::begin() {
    sr.setAllLow();

    for(int i = 0; i < NUM_PACKS; i++) {
        ads1115[i].begin(ADC_ADDR[i]);

        ads1115[i].setGain(GAIN_TWOTHIRDS);

        pinMode(pack_data[i].cs, OUTPUT);
        digitalWrite(pack_data[i].cs, HIGH);

        sr.set(pack_data[i].en, HIGH);
    }
}

long MAX14921::spiTransfer24(int cs_pin, uint8_t byte1, uint8_t byte2, uint8_t byte3) {
    //transfer 24 bits to MAX14921 over spi
    long return_data = 0;
    byte spi_byte = 0;

    SPI.beginTransaction(SPISettings(SPI_MAX_RATE, DATA_ORDER, SPI_MODE));

    digitalWrite(cs_pin, LOW);

    spi_byte = SPI.transfer(byte1);
    return_data = spi_byte;

    spi_byte = SPI.transfer(byte2);
    return_data |= spi_byte << 8;

    spi_byte = SPI.transfer(byte3);
    return_data |= spi_byte << 16;

    digitalWrite(cs_pin, HIGH);

    SPI.endTransaction();

    return(return_data);
}

//finds average voltage for each cell, then multiplies by the number of cells
//in the pack
float MAX14921::get_pack_voltage() {
    total_pack_voltage = 0;

    for(int i = 0; i < NUM_PACKS; i++) {
        spiTransfer24(pack_data[i].cs, pack_data[i].cell_balance, 0x00, 3 << 3);

        int16_t adc_val = ads1115[0].readADC_SingleEnded(0);
        total_pack_voltage += to_voltage(adc_val) * 16;
    }

    Serial.print("Pack voltage: ");
    Serial.println(total_pack_voltage);

    return total_pack_voltage;
}

uint8_t MAX14921::over_voltage() {
    for(int i = 0; i < NUM_PACKS; i++) {
        for(int j = 0; j < NUM_CELLS; j++) {
            if (pack_data[i].cell_voltages[j] > CELL_THRESH_UPPER){
                return 1;
            }
        }
    }
    return 0;
}

uint8_t MAX14921::under_voltage() {
    for(int i = 0; i < NUM_PACKS; i++) {
        for(int j = 0; j < NUM_CELLS; j++) {
            if (pack_data[i].cell_voltages[j] < CELL_THRESH_LOWER){
                return 1;
            }
        }
    }
    return 0;
}


uint8_t MAX14921::balancing() {
    for(int i = 0; i < NUM_PACKS; i++) {
        if (pack_data[i].cell_balance > 0){
            return 1;
        }
    }
}


uint8_t MAX14921::over_temp() {
    //add content when temp sensors added
    return 0;
}


void MAX14921::balance_cells() {
    //balances cells if a cell crosses the set threshold voltage (4.15V)
    //uses previously calculated cell voltages to determine if a particular cell needs to be blanced
    for(int i = 0; i < NUM_PACKS; i++) {
        for(int j = 0; j < NUM_CELLS; j++) {
            if (pack_data[i].cell_voltages[j] >= CELL_THRESH_UPPER) {
                pack_data[i].cell_balancing[j] = 1;
            } else {
                pack_data[i].cell_balancing[j] = 0;
            }
        }
        pack_data[i].cell_balance = 0;
        for(int j = 0; j < NUM_CELLS; j++) {
            pack_data[i].cell_balance |= pack_data[i].cell_balancing[j] << j;
        }
        //set balance to true if either pack needs balancing
        spiTransfer24(pack_data[i].cs, pack_data[i].cell_balance, 0x00, 0x00);
    }
}

//read the voltage of each cell and store in array, returns the array
void MAX14921::record_cell_voltages() {
    //sample bit set to hold, 100000
    for(int i = 0; i < NUM_PACKS; i++) {
        spiTransfer24(pack_data[i].cs, pack_data[i].cell_balance, 0x00, 0x00);
        delay(CELL_SETTLING);
        //digitalWrite(SAMPLB_PIN, LOW);

        byte cell_select = 0;
        cell_select |= 1 << 5;
        spiTransfer24(pack_data[i].cs, pack_data[i].cell_balance, 0x00, cell_select);

        delayMicroseconds(50);
        delay(100);

        //spi: 0x00, 0x80, 0

        for(int cell_num = 0; cell_num < NUM_CELLS; cell_num++){
            byte cell_select = 1;
            cell_select |= cell_num << 1;
            cell_select |= 1 << 5;
            
            long return_data = spiTransfer24(pack_data[i].cs, pack_data[i].cell_balance, 0x00, cell_select);
            delayMicroseconds(10);
            int16_t aout = ads1115[0].readADC_SingleEnded(0);
            //Serial.println(return_data);

            if(return_data & 0xFF) {
                Serial.println("Cell above or below threshold voltage");
            }

            //turn float voltage into truncated char array
            float cell_voltage = to_voltage(aout);
            pack_data[i].cell_voltages[cell_num] = cell_voltage;
        }
    }
}

float MAX14921::get_cell_voltage(uint8_t pack, uint8_t cell) {
    return pack_data[pack].cell_voltages[cell];
}