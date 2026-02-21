#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace shelly_dallas {

// DS18B20 Commands
static const uint8_t DALLAS_CMD_SEARCH_ROM = 0xF0;
static const uint8_t DALLAS_CMD_READ_ROM = 0x33;
static const uint8_t DALLAS_CMD_MATCH_ROM = 0x55;
static const uint8_t DALLAS_CMD_SKIP_ROM = 0xCC;
static const uint8_t DALLAS_CMD_CONVERT_T = 0x44;
static const uint8_t DALLAS_CMD_READ_SCRATCHPAD = 0xBE;
static const uint8_t DALLAS_CMD_WRITE_SCRATCHPAD = 0x4E;

class ShellyDallasTemperatureSensor;

class ShellyDallasComponent : public PollingComponent {
 public:
  void set_pin_tx(InternalGPIOPin *pin) { pin_tx_ = pin; }
  void set_pin_rx(InternalGPIOPin *pin) { pin_rx_ = pin; }

  void register_sensor(ShellyDallasTemperatureSensor *sensor) { sensors_.push_back(sensor); }

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

  // Search ROM
  void search_();

  InternalGPIOPin *pin_tx_{nullptr};
  InternalGPIOPin *pin_rx_{nullptr};
  std::vector<ShellyDallasTemperatureSensor *> sensors_;
  std::vector<uint64_t> found_sensors_;
};

class ShellyDallasTemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(ShellyDallasComponent *parent) { parent_ = parent; }
  void set_address(uint64_t address) { address_ = address; }
  void set_index(uint8_t index) { index_ = index; }
  void set_resolution(uint8_t resolution) { resolution_ = resolution; }
  void set_max_errors(uint8_t max_errors) { max_consecutive_errors_ = max_errors; }

  uint64_t get_address() const { return address_; }
  optional<uint8_t> get_index() const { return index_; }
  uint8_t get_resolution() const { return resolution_; }

  bool setup_sensor();
  bool read_temperature();

 protected:
  void handle_read_error_(const char *reason);

  ShellyDallasComponent *parent_{nullptr};
  uint64_t address_{0};
  optional<uint8_t> index_{};
  uint8_t resolution_{12};

  // Error tolerance: keep last valid value for a number of consecutive errors
  float last_valid_value_{NAN};
  uint8_t consecutive_errors_{0};
  uint8_t max_consecutive_errors_{5};  // Default: 5 errors before NAN
};

}  // namespace shelly_dallas
}  // namespace esphome
