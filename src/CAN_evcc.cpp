/*
* The EVCC supports 250kbps CAN data rate and 29 bit identifiers
*/
#include <CAN.h>
#include <CAN_evcc.h>
#include <MAX14921.h>

void set_bms_status(bms_status_t *bms_status, MAX14921 *max14921) {
    //reset flags to zero and check battery pack for health conditions
    bms_status->bBMSStatusFlags = 0;
    bms_status->bBMSFault = 0;

    if(max14921->over_voltage()) {
        bms_status->bBMSStatusFlags |= BMS_STATUS_CELL_HVC_FLAG;
    }
    if(max14921->under_voltage()) {
        bms_status->bBMSStatusFlags |= BMS_STATUS_CELL_LVC_FLAG;
    }
    if(max14921->balancing()) {
        bms_status->bBMSStatusFlags |= BMS_STATUS_CELL_BVC_FLAG;
    }
    if(max14921->over_temp()) {
        bms_status->bBMSFault |= BMS_FAULT_OVERTEMP_FLAG;
    }
}

void send_can_evcc(bms_status_t *bms_status) {
    Serial.print("Sending extended packet ... ");

    //29 bit address
    CAN.beginExtendedPacket(BMS_EVCC_STATUS_IND);
    CAN.write(bms_status->bBMSStatusFlags);
    CAN.write(bms_status->bBmsId);
    CAN.write(bms_status->bBMSFault);
    CAN.write(bms_status->bReserved2);
    CAN.write(bms_status->bReserved3);
    CAN.endPacket();

    Serial.println("done");
}
