#include "receiver.h"

bool Receiver::begin() {
    pinMode(pins::sw0,OUTPUT);
    digitalWrite(pins::sw0,LOW);
    // begin configures the chip; it does not transmit. Lowest PA setting as defence in depth.
    error=radio.begin(433.92f,4.8f,25.4f,203.1f,-30,16);
    ready=error==RADIOLIB_ERR_NONE;
    if(ready) radio.standby();
    return ready;
}
bool Receiver::tune(unsigned band, uint32_t khz, float bw) {
    if(!ready || !rf::inBand(band,khz)) return false;
    error=radio.standby();
    if(!error) error=radio.setFrequency(khz/1000.0f);
    if(!error) error=radio.setRxBandwidth(bw);
    // Direct RX disables packet framing/sync and avoids RSSI freezing on a sync word.
    // No raw data is captured in v1.0. Never call transmitDirect/startTransmit/transmit.
    if(!error) error=radio.receiveDirectAsync();
    digitalWrite(pins::sw0,rf::bands[band].sw0 ? HIGH : LOW);
    // receiveDirect can remap GDO2; ALWAYS restore the Cap's RF_SW1 afterwards.
    const uint8_t map=RADIOLIB_CC1101_GDOX_HW_TO_0 |
        (rf::bands[band].sw1 ? RADIOLIB_CC1101_GDO2_INV : RADIOLIB_CC1101_GDO2_NORM);
    if(!error) error=radio.setDIOMapping(2,map);
    if(error) radio.standby();
    return error==0;
}
