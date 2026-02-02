/**
 * Shelly Plus Add-On 1-Wire Component for ESPHome
 *
 * Implementation of the dual-pin 1-Wire protocol for the Shelly Plus Add-On.
 * Timing values based on Tasmota's proven implementation.
 *
 * License: MIT
 */

#include "shelly_dallas.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace shelly_dallas {

static const char *const TAG = "shelly_dallas";

void ShellyDallasComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Shelly Dallas...");

  // Explicitly set pin modes (critical for Shelly Plus Add-On)
  pin_rx_->pin_mode(gpio::FLAG_INPUT);
  pin_tx_->pin_mode(gpio::FLAG_OUTPUT);
  pin_tx_->digital_write(true);  // Idle high

  ESP_LOGD(TAG, "TX pin (GPIO%d) set as OUTPUT, HIGH", pin_tx_->get_pin());
  ESP_LOGD(TAG, "RX pin (GPIO%d) set as INPUT", pin_rx_->get_pin());

  // Small delay to let bus stabilize
  delay(10);

  // Test: Read RX pin state
  bool rx_state = pin_rx_->digital_read();
  ESP_LOGI(TAG, "Initial RX pin state: %s", rx_state ? "HIGH" : "LOW");

  // Test reset
  ESP_LOGD(TAG, "Attempting bus reset...");
  bool presence = reset_();
  ESP_LOGI(TAG, "Bus reset result: %s", presence ? "DEVICE PRESENT" : "NO DEVICE");

  // Search for sensors on the bus
  search_();

  // Setup each registered sensor
  for (auto *sensor : sensors_) {
    sensor->setup_sensor();
  }
}

void ShellyDallasComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Shelly Dallas:");
  ESP_LOGCONFIG(TAG, "  TX Pin: GPIO%d", pin_tx_->get_pin());
  ESP_LOGCONFIG(TAG, "  RX Pin: GPIO%d", pin_rx_->get_pin());
  ESP_LOGCONFIG(TAG, "  Found %d sensor(s)", found_sensors_.size());

  for (auto *sensor : sensors_) {
    ESP_LOGCONFIG(TAG, "  Sensor '%s':", sensor->get_name().c_str());
    if (sensor->get_address() != 0) {
      ESP_LOGCONFIG(TAG, "    Address: 0x%016llX", sensor->get_address());
    } else if (sensor->get_index().has_value()) {
      ESP_LOGCONFIG(TAG, "    Index: %d", sensor->get_index().value());
    }
    ESP_LOGCONFIG(TAG, "    Resolution: %d bits", sensor->get_resolution());
  }
}

void ShellyDallasComponent::update() {
  // Start temperature conversion on all sensors
  if (!reset_()) {
    ESP_LOGW(TAG, "No devices found on 1-Wire bus");
    for (auto *sensor : sensors_) {
      sensor->publish_state(NAN);
    }
    return;
  }

  write_byte_(DALLAS_CMD_SKIP_ROM);
  write_byte_(DALLAS_CMD_CONVERT_T);

  // Wait for conversion (750ms for 12-bit resolution)
  // Use set_timeout to avoid blocking
  this->set_timeout("convert", 750, [this]() {
    for (auto *sensor : sensors_) {
      sensor->read_temperature();
    }
  });
}

bool HOT IRAM_ATTR ShellyDallasComponent::reset_() {
  // Wait for bus to be HIGH (idle state)
  // This is critical for reliable operation
  uint8_t retries = 125;
  do {
    if (--retries == 0) {
      ESP_LOGW(TAG, "Bus never went HIGH during reset");
      return false;
    }
    delayMicroseconds(2);
  } while (!pin_rx_->digital_read());

  // Send 480µs LOW reset pulse
  pin_tx_->digital_write(false);
  delayMicroseconds(480);
  pin_tx_->digital_write(true);

  // Wait 70µs for device response
  delayMicroseconds(70);

  // Sample: LOW = device present
  bool presence = !pin_rx_->digital_read();

  // Complete reset timing (410µs)
  delayMicroseconds(410);

  return presence;
}

void HOT IRAM_ATTR ShellyDallasComponent::write_bit_(bool bit) {
  // Pull low
  pin_tx_->digital_write(false);

  if (bit) {
    // Write 1: pull low for 10µs, then release for 55µs
    delayMicroseconds(10);
    pin_tx_->digital_write(true);
    delayMicroseconds(55);
  } else {
    // Write 0: pull low for 65µs, then release for 5µs
    delayMicroseconds(65);
    pin_tx_->digital_write(true);
    delayMicroseconds(5);
  }
}

bool HOT IRAM_ATTR ShellyDallasComponent::read_bit_() {
  // Initiate read by pulling low
  pin_tx_->digital_write(false);

  // Record start time for precise timing
  uint32_t start = micros();

  // Hold low for 3µs
  delayMicroseconds(3);

  // Release line
  pin_tx_->digital_write(true);

  // Wait until 12µs (ESP32 timing constant)
  // This ensures we sample at the optimal time
  while (micros() - start < 12)
    ;

  // Sample the bus
  bool r = pin_rx_->digital_read();

  // Ensure we complete the 60µs time slot
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return r;
}

void ShellyDallasComponent::write_byte_(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    write_bit_((data >> i) & 1);
  }
}

uint8_t ShellyDallasComponent::read_byte_() {
  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (read_bit_()) {
      data |= (1 << i);
    }
  }
  return data;
}

void ShellyDallasComponent::select_(uint64_t address) {
  write_byte_(DALLAS_CMD_MATCH_ROM);
  for (uint8_t i = 0; i < 8; i++) {
    write_byte_((address >> (i * 8)) & 0xFF);
  }
}

void ShellyDallasComponent::search_() {
  found_sensors_.clear();

  uint64_t address = 0;
  uint8_t last_discrepancy = 0;
  bool search_done = false;

  ESP_LOGD(TAG, "Starting ROM search...");

  while (!search_done) {
    if (!reset_()) {
      ESP_LOGW(TAG, "Bus reset failed during search");
      return;
    }

    write_byte_(DALLAS_CMD_SEARCH_ROM);

    uint8_t last_zero = 0;

    for (uint8_t bit_position = 0; bit_position < 64; bit_position++) {
      bool bit = read_bit_();
      bool complement = read_bit_();

      if (bit && complement) {
        // No devices responding
        ESP_LOGD(TAG, "No devices at bit %d", bit_position);
        return;
      }

      bool take_bit;
      if (!bit && !complement) {
        // Discrepancy - both 0 and 1 present
        if (bit_position < last_discrepancy) {
          take_bit = (address >> bit_position) & 1;
        } else if (bit_position == last_discrepancy) {
          take_bit = true;
        } else {
          take_bit = false;
        }
        if (!take_bit) {
          last_zero = bit_position;
        }
      } else {
        take_bit = bit;
      }

      if (take_bit) {
        address |= (1ULL << bit_position);
      } else {
        address &= ~(1ULL << bit_position);
      }

      write_bit_(take_bit);
    }

    // Verify CRC (last byte should make CRC = 0)
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 8; i++) {
      uint8_t byte = (address >> (i * 8)) & 0xFF;
      for (uint8_t j = 0; j < 8; j++) {
        uint8_t mix = (crc ^ byte) & 0x01;
        crc >>= 1;
        if (mix)
          crc ^= 0x8C;
        byte >>= 1;
      }
    }

    if (crc == 0) {
      // Check if it's a supported sensor family
      uint8_t family = address & 0xFF;
      if (family == 0x28 || family == 0x10 || family == 0x22) {
        found_sensors_.push_back(address);
        const char *type = (family == 0x28) ? "DS18B20" :
                          (family == 0x10) ? "DS18S20" : "DS1822";
        ESP_LOGI(TAG, "Found %s: 0x%016llX", type, address);
      } else {
        ESP_LOGW(TAG, "Unknown device family 0x%02X at 0x%016llX", family, address);
      }
    } else {
      ESP_LOGW(TAG, "CRC error for address 0x%016llX", address);
    }

    last_discrepancy = last_zero;
    if (last_discrepancy == 0) {
      search_done = true;
    }
  }

  ESP_LOGI(TAG, "Search complete, found %d sensor(s)", found_sensors_.size());
}

bool ShellyDallasTemperatureSensor::setup_sensor() {
  if (address_ == 0 && index_.has_value()) {
    // Resolve index to address
    uint8_t idx = index_.value();
    if (idx < parent_->found_sensors_.size()) {
      address_ = parent_->found_sensors_[idx];
      ESP_LOGI(TAG, "Resolved index %d to address 0x%016llX", idx, address_);
    } else {
      ESP_LOGW(TAG, "Index %d out of range (found %d sensors)",
               idx, parent_->found_sensors_.size());
      return false;
    }
  } else if (address_ == 0 && parent_->found_sensors_.size() == 1) {
    // Auto-select if only one sensor found
    address_ = parent_->found_sensors_[0];
    ESP_LOGI(TAG, "Auto-selected single sensor: 0x%016llX", address_);
  }

  if (address_ == 0) {
    ESP_LOGW(TAG, "No address configured and multiple sensors found");
    return false;
  }

  // Set resolution
  if (!parent_->reset_()) return false;
  parent_->select_(address_);
  parent_->write_byte_(DALLAS_CMD_WRITE_SCRATCHPAD);
  parent_->write_byte_(0x00);  // TH (alarm high)
  parent_->write_byte_(0x00);  // TL (alarm low)
  parent_->write_byte_(((resolution_ - 9) << 5) | 0x1F);  // Config register

  return true;
}

bool ShellyDallasTemperatureSensor::read_temperature() {
  if (address_ == 0) {
    publish_state(NAN);
    return false;
  }

  if (!parent_->reset_()) {
    ESP_LOGW(TAG, "Bus reset failed");
    publish_state(NAN);
    return false;
  }

  parent_->select_(address_);
  parent_->write_byte_(DALLAS_CMD_READ_SCRATCHPAD);

  uint8_t scratchpad[9];
  for (uint8_t i = 0; i < 9; i++) {
    scratchpad[i] = parent_->read_byte_();
  }

  // Verify CRC
  uint8_t crc = 0;
  for (uint8_t i = 0; i < 9; i++) {
    uint8_t byte = scratchpad[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t mix = (crc ^ byte) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      byte >>= 1;
    }
  }

  if (crc != 0) {
    ESP_LOGW(TAG, "CRC error reading scratchpad");
    publish_state(NAN);
    return false;
  }

  // Calculate temperature
  int16_t raw = (scratchpad[1] << 8) | scratchpad[0];

  // Handle different sensor types
  if ((address_ & 0xFF) == 0x10) {
    // DS18S20 - 9-bit resolution, different format
    raw = raw << 3;
    if (scratchpad[7] == 0x10) {
      raw = (raw & 0xFFF0) + 12 - scratchpad[6];
    }
  } else {
    // DS18B20 / DS1822 - variable resolution
    uint8_t cfg = scratchpad[4] & 0x60;
    if (cfg == 0x00) raw &= ~7;       // 9-bit
    else if (cfg == 0x20) raw &= ~3;  // 10-bit
    else if (cfg == 0x40) raw &= ~1;  // 11-bit
    // 12-bit: no masking needed
  }

  float temperature = raw / 16.0f;

  ESP_LOGD(TAG, "Temperature: %.2f °C (raw: %d)", temperature, raw);
  publish_state(temperature);

  return true;
}

}  // namespace shelly_dallas
}  // namespace esphome
