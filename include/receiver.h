#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include "core.h"

// Official Cap CC1101, SKU U219, on Cardputer ADV. Shared with SD and NFC.
namespace pins { constexpr int sck=40, mosi=14, miso=39, radioCS=5,
    gdo0=15, sw0=13, nfcCS=6, nfcIRQ=4, sdCS=12; }

class Receiver {
    Module module{pins::radioCS,pins::gdo0,RADIOLIB_NC,RADIOLIB_NC,SPI,
                  SPISettings(4000000,MSBFIRST,SPI_MODE0)};
    CC1101 radio{&module};
    bool ready=false;
    int16_t error=0;
public:
    int16_t lastError() const { return error; }
    bool available() const { return ready; }
    bool begin();
    bool tune(unsigned band, uint32_t khz, float bw);
    void stop() { if(ready) radio.standby(); }
    float rssi() { return radio.getRSSI(); }
};
