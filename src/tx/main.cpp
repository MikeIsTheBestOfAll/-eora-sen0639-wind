#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <cmath>
#include <cstring>

#include "PAV3000Sensor.h"
#include "board_pins.h"
#include "project_config.h"
#include "wind_packet.h"

#ifndef COMMISSIONING_MODE
#define COMMISSIONING_MODE 0
#endif

#ifndef REALTIME_MODE
#define REALTIME_MODE 0
#endif

SX1268 radio = new Module(BoardPins::kRadioCs,
                          BoardPins::kRadioDio1,
                          BoardPins::kRadioReset,
                          BoardPins::kRadioBusy);

TwoWire sensorWire(1);
PAV3000Sensor sensor(sensorWire);
bool sensorReady = false;
bool radioReady = false;

RTC_DATA_ATTR uint32_t retainedSequence = 0;

namespace {

struct SampleStatistics {
  uint16_t validCount = 0;
  uint16_t errorCount = 0;
  uint16_t averageRaw = 0;
  bool saturated = false;
  float average = -1.0F;
  float minimum = -1.0F;
  float maximum = -1.0F;
  float standardDeviation = -1.0F;
};

uint16_t readSupplyMillivolts() {
  analogReadResolution(12);
  analogSetPinAttenuation(BoardPins::kBatteryAdc, ADC_11db);

  uint32_t sumMv = 0;
  constexpr uint8_t kReads = 8;
  for (uint8_t i = 0; i < kReads; ++i) {
    sumMv += static_cast<uint32_t>(analogReadMilliVolts(BoardPins::kBatteryAdc));
    delay(2);
  }

  // The official Ebyte factory example uses a 1:1 divider and multiplies the
  // ADC pin voltage by two. Verify/calibrate this value with a multimeter.
  return static_cast<uint16_t>((sumMv / kReads) * 2U);
}

bool initializeRadio() {
  pinMode(BoardPins::kRadioCs, OUTPUT);
  digitalWrite(BoardPins::kRadioCs, HIGH);
  SPI.begin(BoardPins::kRadioSclk,
            BoardPins::kRadioMiso,
            BoardPins::kRadioMosi,
            BoardPins::kRadioCs);

  const int16_t state = radio.begin(
      ProjectConfig::kFrequencyMhz,
      ProjectConfig::kBandwidthKhz,
      ProjectConfig::kSpreadingFactor,
      ProjectConfig::kCodingRateDenominator,
      ProjectConfig::kSyncWord,
      ProjectConfig::kTxPowerDbm,
      ProjectConfig::kPreambleSymbols,
      ProjectConfig::kTcxoVoltage,
      false);

  Serial.printf("RADIO_INIT=%d freq=%.3f BW=%.1f SF=%u CR=4/%u P=%d dBm\n",
                state,
                ProjectConfig::kFrequencyMhz,
                ProjectConfig::kBandwidthKhz,
                ProjectConfig::kSpreadingFactor,
                ProjectConfig::kCodingRateDenominator,
                ProjectConfig::kTxPowerDbm);
  return state == RADIOLIB_ERR_NONE;
}

SampleStatistics collectSamples(uint16_t targetCount) {
  SampleStatistics stats;
  if (!sensorReady) {
    return stats;
  }

  float mean = 0.0F;
  float m2 = 0.0F;
  uint32_t rawSum = 0;

  for (uint16_t i = 0; i < targetCount; ++i) {
    const uint32_t sampleStarted = millis();
    PAV3000Sensor::Reading reading;
    PAV3000Sensor::ReadResult result = PAV3000Sensor::ReadResult::kBusError;

    // One short retry improves resilience to a single disturbed I2C frame.
    for (uint8_t attempt = 0; attempt < 2; ++attempt) {
      result = sensor.read(reading);
      if (result == PAV3000Sensor::ReadResult::kOk) {
        break;
      }
      delay(10);
    }

    if (result == PAV3000Sensor::ReadResult::kOk) {
      ++stats.validCount;
      rawSum += reading.raw;
      stats.saturated = stats.saturated || reading.saturated;

      if (stats.validCount == 1) {
        stats.minimum = reading.metersPerSecond;
        stats.maximum = reading.metersPerSecond;
      } else {
        stats.minimum = min(stats.minimum, reading.metersPerSecond);
        stats.maximum = max(stats.maximum, reading.metersPerSecond);
      }

      const float delta = reading.metersPerSecond - mean;
      mean += delta / static_cast<float>(stats.validCount);
      const float delta2 = reading.metersPerSecond - mean;
      m2 += delta * delta2;
    } else {
      ++stats.errorCount;
    }

    const uint32_t elapsed = millis() - sampleStarted;
    if (elapsed < ProjectConfig::kSampleIntervalMs) {
      delay(ProjectConfig::kSampleIntervalMs - elapsed);
    }
  }

  if (stats.validCount > 0) {
    stats.average = mean;
    stats.averageRaw = static_cast<uint16_t>(rawSum / stats.validCount);
    stats.standardDeviation = sqrtf(m2 / static_cast<float>(stats.validCount));
  }
  return stats;
}

void sleepUntilNextMinute(uint32_t cycleStartedMs) {
  radio.sleep(true);
  digitalWrite(BoardPins::kLed, LOW);
  sensorWire.end();
  SPI.end();

  const uint32_t elapsedMs = millis() - cycleStartedMs;
  const uint64_t sleepUs =
      elapsedMs < ProjectConfig::kReportPeriodMs
          ? static_cast<uint64_t>(ProjectConfig::kReportPeriodMs - elapsedMs) * 1000ULL
          : 1000000ULL;

  Serial.printf("SLEEP_US=%llu\n", static_cast<unsigned long long>(sleepUs));
  Serial.flush();
  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

void runMeasurementAndTransmit() {
  const uint32_t cycleStarted = millis();
  const uint16_t targetCount =
      REALTIME_MODE
          ? ProjectConfig::kRealtimeSampleCount
          : (COMMISSIONING_MODE ? ProjectConfig::kTestSampleCount
                                : ProjectConfig::kFinalSampleCount);

  if (!sensorReady) {
    sensorReady = sensor.probe();
  }
  const SampleStatistics stats = collectSamples(targetCount);

  WindProtocol::WindPacket packet;
  strncpy(packet.nodeId, ProjectConfig::kNodeId, sizeof(packet.nodeId) - 1);
  packet.sequence = ++retainedSequence;
  packet.averageMps = stats.average;
  packet.minimumMps = stats.minimum;
  packet.maximumMps = stats.maximum;
  packet.standardDeviationMps = stats.standardDeviation;
  packet.validSamples = stats.validCount;
  packet.averageRaw = stats.averageRaw;
  packet.batteryMv = readSupplyMillivolts();

  if (!sensorReady) {
    packet.status |= WindProtocol::kStatusSensorNotFound;
  }
  if (stats.validCount == 0) {
    packet.status |= WindProtocol::kStatusNoValidSamples;
  }
  if (stats.errorCount > 0) {
    packet.status |= WindProtocol::kStatusReadErrors;
  }
  if (stats.saturated) {
    packet.status |= WindProtocol::kStatusSaturated;
  }

  char payload[160] = {0};
  if (!WindProtocol::format(packet, payload, sizeof(payload))) {
    Serial.println("PACKET_FORMAT_ERROR");
  } else if (!radioReady) {
    Serial.printf("RADIO_NOT_READY payload=%s\n", payload);
  } else {
    digitalWrite(BoardPins::kLed, HIGH);
    const int16_t state = radio.transmit(payload);
    digitalWrite(BoardPins::kLed, LOW);
    Serial.printf("TX state=%d payload=%s errors=%u\n",
                  state,
                  payload,
                  static_cast<unsigned int>(stats.errorCount));
  }

#if REALTIME_MODE
  const uint32_t elapsedMs = millis() - cycleStarted;
  if (elapsedMs < ProjectConfig::kRealtimeReportPeriodMs) {
    delay(ProjectConfig::kRealtimeReportPeriodMs - elapsedMs);
  }
#elif COMMISSIONING_MODE
  const uint32_t elapsedMs = millis() - cycleStarted;
  if (elapsedMs < ProjectConfig::kTestReportPeriodMs) {
    delay(ProjectConfig::kTestReportPeriodMs - elapsedMs);
  }
#else
  sleepUntilNextMinute(cycleStarted);
#endif
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.printf("EoRa SEN0639 transmitter mode=%s wake=%d\n",
                REALTIME_MODE
                    ? "REALTIME_500MS"
                    : (COMMISSIONING_MODE ? "COMMISSIONING_2S" : "FINAL_60S"),
                static_cast<int>(esp_sleep_get_wakeup_cause()));

  WiFi.mode(WIFI_OFF);
  btStop();

  pinMode(BoardPins::kLed, OUTPUT);
  digitalWrite(BoardPins::kLed, LOW);

  sensorReady = sensor.begin(BoardPins::kSensorSda,
                             BoardPins::kSensorScl,
                             ProjectConfig::kI2cClockHz);
  Serial.println(sensorReady ? "SENSOR_FOUND_0x28" : "SENSOR_NOT_FOUND_0x28");
  delay(ProjectConfig::kSensorWarmupMs);

  radioReady = initializeRadio();
}

void loop() {
  runMeasurementAndTransmit();
}

