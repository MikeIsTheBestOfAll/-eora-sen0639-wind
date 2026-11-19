#pragma once

#include <Arduino.h>
#include <cstdio>
#include <cstring>

namespace WindProtocol {

constexpr char kVersion[] = "WV1";

enum StatusFlag : uint16_t {
  kStatusOk = 0,
  kStatusSensorNotFound = 1U << 0,
  kStatusNoValidSamples = 1U << 1,
  kStatusReadErrors = 1U << 2,
  kStatusSaturated = 1U << 3,
};

struct WindPacket {
  char nodeId[13] = {0};
  uint32_t sequence = 0;
  float averageMps = -1.0F;
  float minimumMps = -1.0F;
  float maximumMps = -1.0F;
  float standardDeviationMps = -1.0F;
  uint16_t validSamples = 0;
  uint16_t averageRaw = 0;
  uint16_t batteryMv = 0;
  uint16_t status = kStatusOk;
};

inline bool format(const WindPacket &packet, char *output, size_t outputSize) {
  const int written = snprintf(
      output, outputSize,
      "%s,%s,%lu,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u",
      kVersion,
      packet.nodeId,
      static_cast<unsigned long>(packet.sequence),
      packet.averageMps,
      packet.minimumMps,
      packet.maximumMps,
      packet.standardDeviationMps,
      static_cast<unsigned int>(packet.validSamples),
      static_cast<unsigned int>(packet.averageRaw),
      static_cast<unsigned int>(packet.batteryMv),
      static_cast<unsigned int>(packet.status));
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

inline bool parse(const char *input, WindPacket &packet) {
  char version[5] = {0};
  unsigned long sequence = 0;
  unsigned int validSamples = 0;
  unsigned int averageRaw = 0;
  unsigned int batteryMv = 0;
  unsigned int status = 0;

  const int matched = sscanf(
      input,
      "%4[^,],%12[^,],%lu,%f,%f,%f,%f,%u,%u,%u,%u",
      version,
      packet.nodeId,
      &sequence,
      &packet.averageMps,
      &packet.minimumMps,
      &packet.maximumMps,
      &packet.standardDeviationMps,
      &validSamples,
      &averageRaw,
      &batteryMv,
      &status);

  if (matched != 11 || strcmp(version, kVersion) != 0) {
    return false;
  }

  packet.sequence = static_cast<uint32_t>(sequence);
  packet.validSamples = static_cast<uint16_t>(validSamples);
  packet.averageRaw = static_cast<uint16_t>(averageRaw);
  packet.batteryMv = static_cast<uint16_t>(batteryMv);
  packet.status = static_cast<uint16_t>(status);
  return true;
}

}  // namespace WindProtocol

