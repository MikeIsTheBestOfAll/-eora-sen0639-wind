#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "board_pins.h"
#include "project_config.h"
#include "wind_packet.h"

SX1268 radio = new Module(BoardPins::kRadioCs,
                          BoardPins::kRadioDio1,
                          BoardPins::kRadioReset,
                          BoardPins::kRadioBusy);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0, U8X8_PIN_NONE);

volatile bool packetReceived = false;
bool displayReady = false;

void IRAM_ATTR onPacketReceived() {
  packetReceived = true;
}

namespace {

void showStartupMessage(const char *line1, const char *line2 = nullptr) {
  if (!displayReady) {
    return;
  }
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tr);
  display.drawStr(0, 16, line1);
  if (line2 != nullptr) {
    display.drawStr(0, 32, line2);
  }
  display.sendBuffer();
}

bool initializeDisplay() {
  Wire.begin(BoardPins::kOledSda, BoardPins::kOledScl, 400000UL);
  Wire.beginTransmission(BoardPins::kOledAddress);
  if (Wire.endTransmission() != 0) {
    Serial.println("OLED_NOT_FOUND_0x3C");
    return false;
  }
  display.begin();
  display.setFontMode(1);
  display.setDrawColor(1);
  display.setFontDirection(0);
  Serial.println("OLED_FOUND_0x3C");
  return true;
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
  Serial.printf("RADIO_INIT=%d freq=%.3f BW=%.1f SF=%u CR=4/%u\n",
                state,
                ProjectConfig::kFrequencyMhz,
                ProjectConfig::kBandwidthKhz,
                ProjectConfig::kSpreadingFactor,
                ProjectConfig::kCodingRateDenominator);
  return state == RADIOLIB_ERR_NONE;
}

void showPacket(const WindProtocol::WindPacket &packet,
                float rssi,
                float snr) {
  if (!displayReady) {
    return;
  }

  char line[32];
  display.clearBuffer();
  display.setFont(u8g2_font_5x8_tr);

  snprintf(line, sizeof(line), "%s  #%lu", packet.nodeId,
           static_cast<unsigned long>(packet.sequence));
  display.drawStr(0, 8, line);

  snprintf(line, sizeof(line), "Wind: %.2f m/s", packet.averageMps);
  display.drawStr(0, 18, line);

  snprintf(line, sizeof(line), "Min %.2f Max %.2f",
           packet.minimumMps, packet.maximumMps);
  display.drawStr(0, 28, line);

  snprintf(line, sizeof(line), "SD %.2f N %u Raw %u",
           packet.standardDeviationMps,
           static_cast<unsigned int>(packet.validSamples),
           static_cast<unsigned int>(packet.averageRaw));
  display.drawStr(0, 38, line);

  snprintf(line, sizeof(line), "Bat %.3fV Status %u",
           packet.batteryMv / 1000.0F,
           static_cast<unsigned int>(packet.status));
  display.drawStr(0, 48, line);

  snprintf(line, sizeof(line), "RSSI %.0f SNR %.1f", rssi, snr);
  display.drawStr(0, 60, line);
  display.sendBuffer();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("EoRa SEN0639 ground receiver");

  pinMode(BoardPins::kLed, OUTPUT);
  digitalWrite(BoardPins::kLed, LOW);

  displayReady = initializeDisplay();
  showStartupMessage("Wind receiver", "Initializing radio");

  if (!initializeRadio()) {
    showStartupMessage("RADIO INIT FAIL", "Check board/power");
    while (true) {
      digitalWrite(BoardPins::kLed, !digitalRead(BoardPins::kLed));
      delay(300);
    }
  }

  radio.setDio1Action(onPacketReceived);
  const int16_t state = radio.startReceive();
  Serial.printf("RADIO_START_RX=%d\n", state);
  if (state != RADIOLIB_ERR_NONE) {
    showStartupMessage("START RX FAIL");
    while (true) {
      delay(1000);
    }
  }

  showStartupMessage("Waiting for WIND", "USB serial 115200");
  Serial.println("protocol,node,seq,avg,min,max,sd,n,raw,batt_mv,status,rssi_dbm,snr_db");
}

void loop() {
  if (!packetReceived) {
    delay(5);
    return;
  }

  packetReceived = false;
  String payload;
  const int16_t state = radio.readData(payload);
  const float rssi = radio.getRSSI();
  const float snr = radio.getSNR();

  if (state == RADIOLIB_ERR_NONE) {
    WindProtocol::WindPacket packet;
    if (WindProtocol::parse(payload.c_str(), packet)) {
      digitalWrite(BoardPins::kLed, HIGH);
      Serial.printf("%s,%.1f,%.1f\n", payload.c_str(), rssi, snr);
      showPacket(packet, rssi, snr);
      delay(40);
      digitalWrite(BoardPins::kLed, LOW);
    } else {
      Serial.printf("PACKET_PARSE_ERROR,%s,RSSI=%.1f,SNR=%.1f\n",
                    payload.c_str(), rssi, snr);
      showStartupMessage("PACKET FORMAT ERR", payload.substring(0, 20).c_str());
    }
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println("RADIO_CRC_ERROR");
  } else {
    Serial.printf("RADIO_READ_ERROR=%d\n", state);
  }

  const int16_t restartState = radio.startReceive();
  if (restartState != RADIOLIB_ERR_NONE) {
    Serial.printf("RADIO_RESTART_RX_ERROR=%d\n", restartState);
  }
}

