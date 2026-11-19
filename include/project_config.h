#pragma once

#include <Arduino.h>

namespace ProjectConfig {

// Change this short identifier if several airborne nodes will be used.
constexpr char kNodeId[] = "KITE01";

// Both transmitter and receiver MUST use identical LoRa settings.
// 470.0 MHz is selected for this project. Confirm the antenna and local radio
// regulations before field operation; change both programs together if needed.
constexpr float kFrequencyMhz = 470.0F;
constexpr float kBandwidthKhz = 125.0F;
constexpr uint8_t kSpreadingFactor = 7;
constexpr uint8_t kCodingRateDenominator = 5;  // 5 means coding rate 4/5.
constexpr uint8_t kSyncWord = 0x12;            // RadioLib private LoRa sync word.
constexpr int8_t kTxPowerDbm = 10;
constexpr uint16_t kPreambleSymbols = 8;

// The official Ebyte example uses the RadioLib SX1268 default TCXO voltage.
constexpr float kTcxoVoltage = 1.6F;

// Final operating cycle: one report every 60 seconds. Each report averages
// 25 valid readings at 5 Hz, covering about five seconds of airflow.
constexpr uint32_t kReportPeriodMs = 60000UL;
constexpr uint32_t kSensorWarmupMs = 1500UL;
constexpr uint16_t kSampleIntervalMs = 200U;
constexpr uint16_t kFinalSampleCount = 25U;

// Fast commissioning mode: average 10 readings and report about every 2 s.
// This mode is used only for desktop/range testing and does not enter deep sleep.
constexpr uint16_t kTestSampleCount = 10U;
constexpr uint32_t kTestReportPeriodMs = 2000UL;

// Real-time demonstration mode: one real sensor reading per LoRa packet,
// with one packet sent about every 500 ms. This is not a low-power mode.
constexpr uint16_t kRealtimeSampleCount = 1U;
constexpr uint32_t kRealtimeReportPeriodMs = 500UL;

constexpr uint32_t kI2cClockHz = 100000UL;

}  // namespace ProjectConfig

