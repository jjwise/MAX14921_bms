/*
Allows creation of a max14921 object. Associated methods for recording the individual cell
and pack voltages, interfacing with the max14921 over spi and balancing cells
*/

#ifndef MAX14921_h
#define MAX14921_h

#include <Adafruit_ADS1X15.h>
#include <ShiftRegister74HC595.h>

#define MOSI 11 
#define MISO 23    
#define SPICLK 18
#define SAMPLB_PIN 13
#define SRLATCH 32
#define SRCLOCK 33
#define SRDATA 2
#define DATA_ORDER LSBFIRST
#define SPI_MODE SPI_MODE0
#define ADC_CONST 6.144 * 2 / pow(2, 16)
#define NUM_PACKS 2

const int ADC_ADDR[NUM_PACKS] = {0x48, 0x48};
const float CELL_THRESH_UPPER = 4.15;
const int SPI_MAX_RATE = 1000000;
const int8_t NUM_CELLS = 15;
const int8_t CELL_SETTLING = 60;

typedef struct {
    float cell_voltages[NUM_CELLS];
    int cell_balancing[NUM_CELLS];
    int cell_balance = 0;
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
    private:
        Adafruit_ADS1115 ads1115[NUM_PACKS];
        ShiftRegister74HC595<1> sr = ShiftRegister74HC595<1>(SRLATCH, SRCLOCK, SRDATA);
        float total_pack_voltage;
        pack_data_t pack_data[NUM_PACKS];
        long spiTransfer24(int cs_pin, uint8_t byte1, uint8_t byte2, uint8_t byte3);
};

float to_voltage(int adc_val);

#endif