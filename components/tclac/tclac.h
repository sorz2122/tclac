/**
* Erstellt von Miguel Ángel López am 20/07/19
* modifiziert von xaxexa
* Refactoring & Komponenten-Erstellung: Nachtigall mit Lötkolben 15.03.2024
**/

#ifndef TCL_ESP_TCL_H
#define TCL_ESP_TCL_H

#include "esphome.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace tclac {

#define SET_TEMP_MASK    0b00001111
#define MODE_POS         7
#define MODE_MASK        0b00111111
#define MODE_AUTO        0b00110101
#define MODE_COOL        0b00110001
#define MODE_DRY         0b00110011
#define MODE_FAN_ONLY    0b00110010
#define MODE_HEAT        0b00110100
#define FAN_SPEED_POS    8
#define FAN_QUIET_POS    33
#define FAN_SPEED_MASK   0b11110000
#define SWING_POS        10
#define SWING_MODE_MASK  0b01100000
#define SWING_OFF        0b00000000
#define SWING_HORIZONTAL 0b00100000
#define SWING_VERTICAL   0b01000000
#define SWING_BOTH       0b01100000

#define FAN_AUTO     0b00000000
#define FAN_LOW      0b00000001
#define FAN_MEDIUM   0b00000011
#define FAN_MIDDLE   0b00000110
#define FAN_HIGH     0b00000111
#define FAN_QUIET    0x80
#define FAN_FOCUS    0b00000101
#define FAN_DIFFUSE  0b01000000

using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateFanMode;
using climate::ClimateSwingMode;

enum class VerticalSwingDirection : uint8_t {
  UP_DOWN = 0,
  UPSIDE = 1,
  DOWNSIDE = 2,
};

enum class HorizontalSwingDirection : uint8_t {
  LEFT_RIGHT = 0,
  LEFTSIDE = 1,
  CENTER = 2,
  RIGHTSIDE = 3,
};

enum class AirflowVerticalDirection : uint8_t {
  LAST = 0,
  MAX_UP = 1,
  UP = 2,
  CENTER = 3,
  DOWN = 4,
  MAX_DOWN = 5,
};

enum class AirflowHorizontalDirection : uint8_t {
  LAST = 0,
  MAX_LEFT = 1,
  LEFT = 2,
  CENTER = 3,
  RIGHT = 4,
  MAX_RIGHT = 5,
};

class tclacClimate : public climate::Climate,
                     public uart::UARTDevice,
                     public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;

 protected:
  byte dataTX[38];
  byte dataRX[61];
  byte poll[8] = {0xBB, 0x00, 0x01, 0x04, 0x19, 0x03, 0x01, 0x00};

  bool beeper_status_{false};
  bool display_status_{false};
  bool force_mode_status_{false};
  bool module_display_status_{false};
  bool is_call_control{false};
  bool allow_take_control{false};

  ClimateMode switch_climate_mode;
  ClimateFanMode switch_fan_mode;
  ClimateSwingMode switch_swing_mode;
  ClimatePreset switch_preset;
  int target_temperature_set{16};

  AirflowVerticalDirection vertical_direction_{AirflowVerticalDirection::LAST};
  AirflowHorizontalDirection horizontal_direction_{AirflowHorizontalDirection::LAST};
  VerticalSwingDirection vertical_swing_direction_{VerticalSwingDirection::UP_DOWN};
  HorizontalSwingDirection horizontal_swing_direction_{HorizontalSwingDirection::LEFT_RIGHT};

  std::set<ClimateMode> supported_modes_;
  std::set<ClimateFanMode> supported_fan_modes_;
  std::set<ClimateSwingMode> supported_swing_modes_;
  std::set<ClimatePreset> supported_presets_;

  GPIOPin *rx_led_pin_{nullptr};
  GPIOPin *tx_led_pin_{nullptr};

  void readData();
  void takeControl();
  void sendData(byte *message, byte size);
  static String getHex(byte *message, byte size);
  static byte getChecksum(const byte *message, size_t size);
  void dataShow(bool flow, bool shine);

  // Config setter
  void set_beeper_state(bool state);
  void set_display_state(bool state);
  void set_force_mode_state(bool state);
  void set_module_display_state(bool state);
#ifdef CONF_RX_LED
  void set_rx_led_pin(GPIOPin *rx_led_pin);
#endif
#ifdef CONF_TX_LED
  void set_tx_led_pin(GPIOPin *tx_led_pin);
#endif
  void set_vertical_airflow(AirflowVerticalDirection direction);
  void set_horizontal_airflow(AirflowHorizontalDirection direction);
  void set_vertical_swing_direction(VerticalSwingDirection direction);
  void set_horizontal_swing_direction(HorizontalSwingDirection direction);
  void set_supported_modes(const std::set<ClimateMode> &modes);
  void set_supported_fan_modes(const std::set<ClimateFanMode> &modes);
  void set_supported_swing_modes(const std::set<ClimateSwingMode> &modes);
  void set_supported_presets(const std::set<ClimatePreset> &presets);
};

}  // namespace tclac
}  // namespace esphome

#endif
