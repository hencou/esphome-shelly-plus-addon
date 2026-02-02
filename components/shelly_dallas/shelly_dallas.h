/**
 * Shelly Plus Add-On 1-Wire Component for ESPHome
 *
 * This component implements the 1-Wire protocol with separate TX and RX pins
 * for the Shelly Plus Add-On's galvanically isolated interface.
 *
 * The Shelly Plus Add-On uses an ISO7221A dual digital isolator which splits
 * the bidirectional 1-Wire protocol into separate output (TX) and input (RX)
 * paths.
 *
 * Supported sensors:
 * - DS18B20 (family code 0x28)
 * - DS18S20 (family code 0x10)
 * - DS1822  (family code 0x22)
 *
 * Based on timing from Tasmota's proven implementation.
 *
 * License: MIT
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace shelly_dallas {

// DS18B20 1-Wire Commands
static const uint8_t DALLAS_CMD_SEARCH_ROM = 0xF0;
static const uint8_t DALLAS_CMD_READ_ROM = 0x33;
static const uint8_t DALLAS_CMD_MATCH_ROM = 0x55;
static const uint8_t DALLAS_CMD_SKIP_ROM = 0xCC;
static const uint8_t DALLAS_CMD_CONVERT_T = 0x44;
static const uint8_t DALLAS_CMD_READ_SCRATCHPAD = 0xBE;
static const uint8_t DALLAS_CMD_WRITE_SCRATCHPAD = 0x4E;

class ShellyDallasTemperatureSensor;

/**
 * Main component for the Shelly Plus Add-On 1-Wire bus.
 *
 * Manages the dual-pin 1-Wire communication and coordinates
 * temperature sensor readings.
 */
class ShellyDallasComponent : public PollingComponent {
 public:
  void set_pin_tx(InternalGPIOPin *pin) { pin_tx_ = pin; }
  void set_pin_rx(InternalGPIOPin *pin) { pin_rx_ = pin; }

  void register_sensor(ShellyDallasTemperatureSensor *sensor) {
    sensors_.push_back(sensor);
  }

  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  friend ShellyDallasTemperatureSensor;

  // 1-Wire protocol with separate TX/RX (optimized for Shelly Plus Add-On)
  bool reset_();
  void write_bit_(bool bit);
  bool read_bit_();
  void write_byte_(uint8_t data);
  uint8_t read_byte_();
  void select_(uint64_t address);

  // ROM Search algorithm
  void search_();

  InternalGPIOPin *pin_tx_{nullptr};
  InternalGPIOPin *pin_rx_{nullptr};
  std::vector<ShellyDallasTemperatureSensor *> sensors_;
  std::vector<uint64_t> found_sensors_;
};

/**
 * Temperature sensor class for DS18B20/DS18S20/DS1822 sensors.
 */
class ShellyDallasTemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(ShellyDallasComponent *parent) { parent_ = parent; }
  void set_address(uint64_t address) { address_ = address; }
  void set_index(uint8_t index) { index_ = index; }
  void set_resolution(uint8_t resolution) { resolution_ = resolution; }

  uint64_t get_address() const { return address_; }
  optional<uint8_t> get_index() const { return index_; }
  uint8_t get_resolution() const { return resolution_; }

  bool setup_sensor();
  bool read_temperature();

 protected:
  ShellyDallasComponent *parent_{nullptr};
  uint64_t address_{0};
  optional<uint8_t> index_{};
  uint8_t resolution_{12};
};

}  // namespace shelly_dallas
}  // namespace esphome
