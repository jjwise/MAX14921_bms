/*
* The EVCC supports 250kbps CAN data rate and 29 bit identifiers
*/
#ifndef CAN_EVCC_H
#define CAN_EVCC_H

#include <MAX14921.h>

#define CAN_RATE 250E3
#define CAN_RX 4
#define CAN_TX 5
/*
* BMS->EVCC message Identifier
*/
#define BMS_EVCC_STATUS_IND 0x01dd0001
/* bBMSStatusFlag bits */
#define BMS_STATUS_CELL_HVC_FLAG 0x01 /* set if a cell is > HVC */
#define BMS_STATUS_CELL_LVC_FLAG 0x02 /* set if a cell is < LVC */
#define BMS_STATUS_CELL_BVC_FLAG 0x04 /* set if a cell is > BVC */
/* bBMSFault bits */
#define BMS_FAULT_OVERTEMP_FLAG 0x04 /* set for thermistor overtemp */
/*
* BMS->EVCC message body
*/
typedef struct {
    uint8_t bBMSStatusFlags; /* see bit definitions above */
    uint8_t bBmsId; /* reserved, set to 0 */
    uint8_t bBMSFault;
    uint8_t bReserved2; /* reserved, set to 0 */
    uint8_t bReserved3; /* reserved, set to 0 */ 
} bms_status_t;

//send data in bms_status struct
void send_can_evcc(bms_status_t *bms_status);

//set the status bytes in bms_status struct
void set_bms_status(bms_status_t *bms_status, MAX14921 *max14921);

#endif