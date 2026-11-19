#include "PAV3000Sensor.h"

// Conversion points are taken from DFRobot's MIT-licensed
// DFRobot_PAV3000 v1.0.0 library for the PAV3000/PAV3015 15 m/s range.
namespace {

constexpr uint16_t kRawPoints[] = {
    409, 1203, 1597, 1908, 2187, 2400, 2629,
    2801, 3006, 3178, 3309, 3563, 3686};

constexpr float kVelocityPoints[] = {
    0.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F,
    8.0F, 9.0F, 10.0F, 11.0F, 13.0F, 15.0F};

constexpr size_t kPointCount = sizeof(kRawPoints) / sizeof(kRawPoints[0]);

}  // namespace

bool PAV3000Sensor::begin(int sdaPin, int sclPin, uint32_t clockHz) {
  if (!wire_.begin(sdaPin, sclPin, clockHz)) {
    return false;
  }
  delay(10);
  return probe();
}

bool PAV3000Sensor::probe() {
  wire_.beginTransmission(kAddress);
  return wire_.endTransmission() == 0;
}

PAV3000Sensor::ReadResult PAV3000Sensor::read(Reading &reading) {
  while (wire_.available()) {
    wire_.read();
  }

  constexpr size_t kFrameSize = 5;
  uint8_t frame[kFrameSize] = {0};
  const size_t received = wire_.requestFrom(
      static_cast<uint8_t>(kAddress), static_cast<uint8_t>(kFrameSize));

  if (received != kFrameSize || wire_.available() < kFrameSize) {
    while (wire_.available()) {
      wire_.read();
    }
    return ReadResult::kBusError;
  }

  uint16_t checksum = 0;
  bool allZero = true;
  bool allOnes = true;
  for (size_t i = 0; i < kFrameSize; ++i) {
    frame[i] = static_cast<uint8_t>(wire_.read());
    checksum += frame[i];
    allZero = allZero && frame[i] == 0x00;
    allOnes = allOnes && frame[i] == 0xFF;
  }

  if (allZero || allOnes) {
    return ReadResult::kInvalidFrame;
  }
  if ((checksum & 0xFFU) != 0U) {
    return ReadResult::kChecksumError;
  }

  reading.raw = (static_cast<uint16_t>(frame[1]) << 8U) | frame[2];
  reading.metersPerSecond = rawToMetersPerSecond(reading.raw, reading.saturated);
  return ReadResult::kOk;
}

float PAV3000Sensor::rawToMetersPerSecond(uint16_t raw, bool &saturated) {
  saturated = false;

  if (raw <= kRawPoints[0]) {
    return 0.0F;
  }
  if (raw >= kRawPoints[kPointCount - 1]) {
    saturated = true;
    return kVelocityPoints[kPointCount - 1];
  }

  for (size_t i = 0; i < kPointCount - 1; ++i) {
    if (raw <= kRawPoints[i + 1]) {
      const float fraction =
          static_cast<float>(raw - kRawPoints[i]) /
          static_cast<float>(kRawPoints[i + 1] - kRawPoints[i]);
      return kVelocityPoints[i] +
             fraction * (kVelocityPoints[i + 1] - kVelocityPoints[i]);
    }
  }

  saturated = true;
  return 15.0F;
}

