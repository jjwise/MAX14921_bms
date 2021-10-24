/*
Allows creation of a max14921 object. Associated methods for recording the individual cell
and pack voltages, interfacing with the max14921 over spi and balancing cells
*/

#ifndef MAX14921_h
#define MAX14921_h

#include <Adafruit_ADS1X15.h>
#include <ShiftRegister74HC595.h>
#include <CircularBuffer.h>

#define DEBUG

#define DATA_ORDER LSBFIRST
#define SPI_MODE SPI_MODE0
#define CELL_SELECT 0x01
#define SAMPLB 0x20
#define LOW_POWER 0x80
#define ADC_CONST 6.144 * 2 / pow(2, 16)

const uint8_t MOSI_PIN = 11;
const uint8_t MISO_PIN = 23;    
const uint8_t SPICLK = 18;
const uint8_t SRLATCH = 32;
const uint8_t SRCLOCK = 33;
const uint8_t SRDATA = 2;

const uint8_t HOLD_SETTLING = 10; //in useconds
const uint8_t LEVEL_SHIFT_DELAY = 50; // in useconds

const uint8_t NUM_PACKS = 2;
const uint8_t CIRC_BUFF_LEN = 20;
const uint8_t AOUT_SETTLING = 5; // in useconds
const uint8_t PACK_VOLTAGE_SETTLING = 50; // in useconds

const int ADC_ADDR[NUM_PACKS] = {0x48, 0x49};
const float CELL_THRESH_UPPER = 4.1;
const float CELL_THRESH_LOWER = 3.4;
const int SPI_MAX_RATE = 50000;
const uint8_t NUM_CELLS = 15;
const uint8_t CELL_SETTLING = 60;

typedef struct {
    float cell_average_voltages[NUM_CELLS];
    CircularBuffer<float, CIRC_BUFF_LEN> cell_voltages[NUM_CELLS];
    int cell_balancing[NUM_CELLS];
    uint8_t balance_byte1 = 0;
    uint8_t balance_byte2 = 0;
    uint8_t en;
    uint8_t cs;
} pack_data_t;

class MAX14921 {
    public:
        MAX14921(uint8_t cs1, uint8_t cs2, uint8_t en1, uint8_t en2);
        void begin();
        void balance_cells();
        void record_cell_voltages();
        float get_pack_voltage();
        float get_cell_voltage(uint8_t pack, uint8_t cell);
        uint8_t over_voltage();
        uint8_t under_voltage();
        uint8_t balancing();
        uint8_t over_temp();
        void sleep();
        void wake();
        void reset_balance();
    private:
        Adafruit_ADS1115 ads1115[NUM_PACKS];
        ShiftRegister74HC595<1> sr = ShiftRegister74HC595<1>(SRDATA, SRCLOCK, SRLATCH);
        CircularBuffer<float, CIRC_BUFF_LEN> total_pack_voltages;
        float average_pack_voltage;
        pack_data_t pack_data[NUM_PACKS];
        long spiTransfer24(int cs_pin, uint8_t byte1, uint8_t byte2, uint8_t byte3);
        void update_cell_average();
        void update_pack_average();
};

float to_voltage(int adc_val);

#endif