# ESPHome Shelly Plus Add-On Component

Custom ESPHome component for the **Shelly Plus Add-On** sensor interface, enabling full support for:
- **DS18B20** temperature sensors (1-Wire)
- **Analog Input** (0-10V as duty cycle)
- **Digital Input**

This component was developed because ESPHome has no native support for the Shelly Plus Add-On's dual-GPIO 1-Wire interface, which uses galvanic isolation with separate TX and RX pins.

## Supported Devices

| Device | Chip | 1-Wire TX | 1-Wire RX | Analog IN | Digital IN |
|--------|------|-----------|-----------|-----------|------------|
| Shelly 2PM Gen3 | ESP32-C3 | GPIO9 | GPIO21 | GPIO20 | GPIO1 |
| Shelly 1PM Gen3 | ESP32-C3 | GPIO9 | GPIO21 | GPIO20 | GPIO1 |
| Shelly Plus 1/1PM | ESP32 | GPIO0 | GPIO1 | GPIO3 | GPIO19/22 |
| Shelly Plus 2PM | ESP32 | GPIO0 | GPIO1 | GPIO3 | GPIO22 |

> **Note:** GPIO assignments vary between device generations. Gen3 devices use different pins than Plus devices!

## Why This Component?

The Shelly Plus Add-On uses an **ISO7221A dual digital isolator** for galvanic isolation. This splits the bidirectional 1-Wire protocol into separate TX (output) and RX (input) paths. Standard ESPHome `dallas` or `one_wire` components don't support this dual-pin configuration.

### How It Works

```
ESP32                    ISO7221A                  DS18B20
┌──────┐                ┌────────┐                ┌───────┐
│ TX ──┼───► Isolator ──┼──► ────┼───► Data ◄────┤       │
│      │                │        │                │       │
│ RX ◄─┼──── Isolator ◄─┼──── ◄──┼──── Data ◄────┤       │
└──────┘                └────────┘                └───────┘
```

## Installation

### Option 1: External Component (Recommended)

Add to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/majoto85/esphome-shelly-plus-addon
      ref: main
    components: [shelly_dallas]
```

### Option 2: Local Component

Copy the `components/shelly_dallas` folder to your ESPHome configuration directory under `custom_components/` or `local_components/`.

```yaml
external_components:
  - source:
      type: local
      path: local_components
    components: [shelly_dallas]
```

## Configuration

### Complete Example for Shelly 2PM Gen3

```yaml
substitutions:
  device_name: shelly-2pm-gen3

esphome:
  name: ${device_name}

esp32:
  variant: esp32c3
  flash_size: 8MB
  framework:
    type: esp-idf
    version: recommended

# External component for Shelly Plus Add-On
external_components:
  - source:
      type: local
      path: local_components
    components: [shelly_dallas]

# I2C for ADE7953 power monitoring
i2c:
  sda: GPIO6
  scl: GPIO7

# 1-Wire bus for DS18B20 temperature sensors
shelly_dallas:
  pin_tx: GPIO9
  pin_rx: GPIO21
  update_interval: 30s

sensor:
  # DS18B20 Temperature Sensor
  - platform: shelly_dallas
    name: "Temperature"
    resolution: 12
    # Optional: specify address if multiple sensors
    # address: 0x28XXXXXXXXXXXX

  # Analog Input (0-10V as duty cycle)
  - platform: duty_cycle
    pin: GPIO20
    name: "Analog IN Duty"
    id: addon_analog_duty
    unit_of_measurement: "%"
    update_interval: 1s

  # Convert duty cycle to voltage
  - platform: template
    name: "Analog IN Voltage"
    unit_of_measurement: "V"
    lambda: 'return id(addon_analog_duty).state * 0.1;'

binary_sensor:
  # Digital Input
  - platform: gpio
    pin:
      number: GPIO1
      mode: input
    name: "Digital IN"
    filters:
      - delayed_on_off: 50ms
```

### DS18B20 Sensor Options

```yaml
sensor:
  - platform: shelly_dallas
    name: "My Temperature Sensor"

    # Resolution: 9-12 bits (higher = more accurate but slower)
    resolution: 12

    # For multiple sensors, specify the address
    address: 0x28FF1234567890AB

    # Or use index (0 = first sensor found)
    # index: 0

    # Standard ESPHome sensor options
    unit_of_measurement: "°C"
    accuracy_decimals: 2
    filters:
      - sliding_window_moving_average:
          window_size: 5
          send_every: 1
```

## GPIO Reference

### Shelly 2PM Gen3 (ESP32-C3)

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO0 | ADE7953 Reset | Power meter |
| GPIO1 | **Add-On Digital IN** | Directly usable (shared with ADE7953 IRQ) |
| GPIO3 | Relay 1 | Channel 1 output |
| GPIO4 | NTC Temperature | Internal device temperature |
| GPIO5 | Relay 2 | Channel 2 output |
| GPIO6 | I2C SDA | For ADE7953 |
| GPIO7 | I2C SCL | For ADE7953 |
| GPIO9 | **Add-On 1-Wire TX** | DS18B20 output |
| GPIO10 | Switch Input 1 | Physical input |
| GPIO18 | Switch Input 2 | Physical input |
| GPIO19 | Reset Button | - |
| GPIO20 | **Add-On Analog IN** | As PWM/duty cycle |
| GPIO21 | **Add-On 1-Wire RX** | DS18B20 input |

### Shelly Plus 1PM / Plus 2PM (ESP32)

| GPIO | Function |
|------|----------|
| GPIO0 | Add-On 1-Wire TX |
| GPIO1 | Add-On 1-Wire RX |
| GPIO3 | Add-On Analog IN |
| GPIO19/22 | Add-On Digital IN |

## Analog Input Details

The Shelly Plus Add-On converts the 0-10V analog input to a PWM signal due to galvanic isolation:

- Uses a sawtooth waveform generator and comparator
- Output duty cycle is proportional to input voltage
- 0V = 0% duty cycle
- 10V = 100% duty cycle

Read it with ESPHome's `duty_cycle` sensor and convert to voltage:

```yaml
sensor:
  - platform: duty_cycle
    pin: GPIO20
    name: "Analog Raw"
    id: analog_duty

  - platform: template
    name: "Analog Voltage"
    unit_of_measurement: "V"
    lambda: 'return id(analog_duty).state * 0.1;'
```

## Troubleshooting

### No sensors found

1. Check wiring: VCC, DATA, GND on the Add-On's 1-Wire terminals
2. Verify GPIO pins match your device (Gen3 vs Plus)
3. Check logs for "Bus reset result: DEVICE PRESENT"

### CRC errors

- Usually indicates timing issues or poor connections
- Try shorter wires or add a 4.7kΩ pull-up resistor on the data line

### Sensor shows NaN

- Conversion timeout - increase `update_interval`
- Check that only one component uses the 1-Wire GPIOs

## Technical Details

### 1-Wire Protocol Timing

Based on Tasmota's proven implementation:

| Operation | Duration |
|-----------|----------|
| Reset pulse | 480µs low |
| Presence detect | 70µs wait, sample |
| Write '1' | 10µs low, 55µs release |
| Write '0' | 65µs low, 5µs release |
| Read bit | 3µs low, sample at 12µs |

### Supported Sensor Families

- 0x28: DS18B20 (most common)
- 0x10: DS18S20
- 0x22: DS1822

## Credits

This component was developed with assistance from **Claude (Anthropic)** through collaborative debugging and research of:
- [Tasmota DS18x20 implementation](https://github.com/arendst/Tasmota)
- [persuader72's ESPHome components](https://github.com/persuader72/esphome-components)
- [ESPHome Feature Request #2034](https://github.com/esphome/feature-requests/issues/2034)

Special thanks to the Tasmota community for documenting the Shelly Gen3 GPIO pinouts.

## License

MIT License - See [LICENSE](LICENSE) file.

## Contributing

Contributions are welcome! Please:
1. Test on your hardware before submitting
2. Include your device model and GPIO configuration
3. Add documentation for new devices

## Links

- [Shelly Plus Add-On Product Page](https://www.shelly.com/products/shelly-plus-add-on)
- [Shelly Knowledge Base](https://kb.shelly.cloud/knowledge-base/shelly-plus-add-on)
- [ESPHome Documentation](https://esphome.io/)
