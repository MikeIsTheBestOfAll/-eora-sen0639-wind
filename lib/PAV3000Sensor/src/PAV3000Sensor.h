#pragma once

#include <Arduino.h>
#include <Wire.h>

class PAV3000Sensor {
 public:
  static constexpr uint8_t kAddress = 0x28;

  enum class ReadResult : uint8_t {
    kOk = 0,
    kBusError,
    kChecksumError,
    kInvalidFrame,
  };

  struct Reading {
    uint16_t raw = 0;
    float metersPerSecond = 0.0F;
    bool saturated = false;
  };

  explicit PAV3000Sensor(TwoWire &wire) : wire_(wire) {}

  bool begin(int sdaPin, int sclPin, uint32_t clockHz = 100000UL);
  bool probe();
  ReadResult read(Reading &reading);

 private:
  static float rawToMetersPerSecond(uint16_t raw, bool &saturated);

  TwoWire &wire_;
};

