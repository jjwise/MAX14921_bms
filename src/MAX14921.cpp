/*
Allows creation of a max14921 object. Associated methods for recording the individual cell
and pack voltages, interfacing with the max14921 over spi and balancing cells
*/

#include <Arduino.h>
#include <MAX14921.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include <ShiftRegister74HC595.h>
#include <CircularBuffer.h>

float to_voltage(int adc_val) {
    return (float)adc_val * ADC_CONST;
}


MAX14921::MAX14921(uint8_t cs1, uint8_t cs2, uint8_t en1, uint8_t en2) {
    pack_data[0].cs = cs1;
    pack_data[1].cs = cs2;
    pack_data[0].en = en1;
    pack_data[1].en = en2;
    average_pack_voltage = 0;
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


void MAX14921::sleep() {
    for(int i = 0; i < NUM_PACKS; i++){
        sr.set(pack_data[i].en, LOW);
        spiTransfer24(pack_data[i].cs, 0x00, 0x00, LOW_POWER);
    }
}


void MAX14921::wake() {
    for(int i = 0; i < NUM_PACKS; i++){
        sr.set(pack_data[i].en, HIGH);
        spiTransfer24(pack_data[i].cs, 0x00, 0x00, 0x00);
    }
}


void MAX14921::update_cell_average() {
    //moving average filter is the best filter imo.
    //maybe could do a cheaky digital low pass filter?
    for(int i = 0; i < NUM_PACKS; i++) {
        for(int j = 0; j < NUM_CELLS; j++) {
            float cell_average = 0;
            int len = pack_data[i].cell_voltages[j].size();
            for(int m = 0; m < len; m++) {
                cell_average += pack_data[i].cell_voltages[j][m];
            }
            cell_average /= len;
            pack_data[i].cell_average_voltages[j] = cell_average;
        }
    }
}


void MAX14921::update_pack_average() {
    //moving average filter is the best filter imo.
    //maybe could do a cheaky digital low pass filter?
    average_pack_voltage = 0;
    int len = total_pack_voltages.size();
    for(int i = 0; i < len; i++) {
        average_pack_voltage += total_pack_voltages[i];
    }
    average_pack_voltage /= len;
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
    float total_pack_voltage = 0;

    for(int i = 0; i < NUM_PACKS; i++) {
        spiTransfer24(pack_data[i].cs, pack_data[i].balance_byte1, pack_data[i].balance_byte2, 3 << 3);
        int16_t adc_val = ads1115[0].readADC_SingleEnded(0); 
        total_pack_voltage += to_voltage(adc_val) * 16;
    }

    #ifdef DEBUG
    Serial.print("Pack voltage: ");
    Serial.println(total_pack_voltage);
    #endif

    total_pack_voltages.push(total_pack_voltage);
    update_pack_average();

    return average_pack_voltage;
}


//checks if any cell in pack is over the thresh voltage
uint8_t MAX14921::over_voltage() {
    for(int i = 0; i < NUM_PACKS; i++) {
        for(int j = 0; j < NUM_CELLS; j++) {
            if (pack_data[i].cell_average_voltages[j] > CELL_THRESH_UPPER){
                return 1;
            }
        }
    }
    return 0;
}


//checks if any cell in pack is under the thresh voltage
uint8_t MAX14921::under_voltage() {
    for(int i = 0; i < NUM_PACKS; i++) {
        for(int j = 0; j < NUM_CELLS; j++) {
            if (pack_data[i].cell_average_voltages[j] < CELL_THRESH_LOWER){
                return 1;
            }
        }
    }
    return 0;
}


//checks to see if any cell in pack needs balancing
uint8_t MAX14921::balancing() {
    for(int i = 0; i < NUM_PACKS; i++) {
        if (pack_data[i].balance_byte1 || pack_data[i].balance_byte2){
            return 1;
        }
    }
    return 0;
}

////checks if any of the thermistor readings are over temperature
uint8_t MAX14921::over_temp() {
    //add content when temp sensors added
    return 0;
}


void MAX14921::balance_cells() {
    //balances cells if a cell crosses the set threshold voltage (4.15V)
    //uses previously calculated cell voltages to determine if a particular cell needs to be blanced
    for(int i = 0; i < NUM_PACKS; i++) {
        for(uint8_t j = 0; j < NUM_CELLS; j++) {
            if (pack_data[i].cell_average_voltages[j] >= CELL_THRESH_UPPER) {
                pack_data[i].cell_balancing[j] = 1;
            } else {
                pack_data[i].cell_balancing[j] = 0;
            }
        }
        pack_data[i].balance_byte1 = 0;
        pack_data[i].balance_byte2 = 0;
        for(uint8_t j = 0; j < NUM_CELLS; j++) {
            if(j / 8) {
                pack_data[i].balance_byte2 |= pack_data[i].cell_balancing[j] << (7 - (j % 8));
            } else {
                pack_data[i].balance_byte1 |= pack_data[i].cell_balancing[j] << (7 - (j % 8));
            }
        }
        //set balance to true if either pack needs balancing
        spiTransfer24(pack_data[i].cs, pack_data[i].balance_byte1, pack_data[i].balance_byte2, 0x00);
    }
}

//read the voltage of each cell and store in array, returns the array
void MAX14921::record_cell_voltages() {
    //sample bit set to hold, 100000
    for(int i = 0; i < NUM_PACKS; i++) {
        //put max14921 into sample phase for 60ms
        spiTransfer24(pack_data[i].cs, pack_data[i].balance_byte1, pack_data[i].balance_byte2, 0x00);
        delay(CELL_SETTLING);

        for(uint8_t cell_num = NUM_CELLS - 1; cell_num >= 0; cell_num--){
            byte sample_cell = CELL_SELECT | cell_num << 3 | SAMPLB;
            
            //hold cell voltage for reading
            long return_data = spiTransfer24(pack_data[i].cs, pack_data[i].balance_byte1, pack_data[i].balance_byte2, sample_cell);
            delayMicroseconds(HOLD_SETTLING);
            int16_t adc_val = ads1115[i].readADC_SingleEnded(0);
            //delay for a bit
            delayMicroseconds(10);

            #ifdef DEBUG
            if(return_data & 0xFF) {
                Serial.println("Cell above or below threshold voltage");
            }
            #endif DEBUG

            //turn float voltage into truncated char array
            float cell_voltage = to_voltage(adc_val);
            pack_data[i].cell_voltages[cell_num].push(cell_voltage);
        }
        //put back in sample phase
        spiTransfer24(pack_data[i].cs, pack_data[i].balance_byte1, pack_data[i].balance_byte2, 0x00);
    }

    

    //update values in cell average array
    update_cell_average();
}

float MAX14921::get_cell_voltage(uint8_t pack, uint8_t cell) {
    return pack_data[pack].cell_average_voltages[cell];
}