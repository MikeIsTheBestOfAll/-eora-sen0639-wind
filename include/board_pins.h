#pragma once

#include <Arduino.h>

namespace BoardPins {

// EoRa-S3-400TB onboard E22-400MM22S (SX1268) SPI bus.
constexpr int kRadioSclk = 5;
constexpr int kRadioMiso = 3;
constexpr int kRadioMosi = 6;
constexpr int kRadioCs = 7;
constexpr int kRadioReset = 8;
constexpr int kRadioDio1 = 33;
constexpr int kRadioBusy = 34;

// Onboard SSD1306 OLED I2C bus. Used only by the ground receiver.
constexpr int kOledSda = 18;
constexpr int kOledScl = 17;
constexpr uint8_t kOledAddress = 0x3C;

// External SEN0639 I2C bus. This is deliberately separate from the OLED bus.
constexpr int kSensorSda = 41;
constexpr int kSensorScl = 42;

constexpr int kLed = 37;
constexpr int kBatteryAdc = 1;

}  // namespace BoardPins

