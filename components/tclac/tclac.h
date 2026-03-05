/**
* Erstellt von Miguel Ángel López am 20/07/19
* modifiziert von xaxexa
* Refactoring & Komponenten-Erstellung: Nachtigall mit Lötkolben 15.03.2024
* Matter-Fix (min_temp 16→14°C): Perplexity AI 05.03.2026
**/

#ifndef TCL_ESP_TCL_H
#define TCL_ESP_TCL_H

#include "esphome.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace tclac {

#define SET_TEMP_MASK	0b00001111
#define MODE_POS		7
#define MODE_MASK		0b00111111
#define MODE_AUTO		0b00110101
#define MODE_COOL		0b00110001
#define MODE_DRY		0b00110011
#define MODE_FAN_ONLY	0b00110010
#define MODE_HEAT		0b00110100
#define FAN_SPEED_POS	8
#define FAN_QUIET_POS	33
#define FAN_AUTO		0b10000000
#define FAN_QUIET		0x80
#define FAN_LOW			0b10010000
#define FAN_MIDDLE		0b11000000
#define FAN_MEDIUM		0b10100000
#define FAN_HIGH		0b11010000
#define FAN_FOCUS		0b10110000
#define FAN_DIFFUSE		0b10000000
#define FAN_SPEED_MASK	0b11110000
#define SWING_POS			10
#define SWING_OFF			0b00000000
#define SWING_HORIZONTAL	0b00100000
#define SWING_VERTICAL		0b01000000
#define SWING_BOTH			0b01100000
#define SWING_MODE_MASK		0b01100000

using climate::ClimateCall;
using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateFanMode;
using climate::ClimateSwingMode;

enum class VerticalSwingDirection : uint8_t {
	UP_DOWN = 0,   // Oben-unten
	UPSIDE = 1,    // Obere Hälfte
	DOWNSIDE = 2,  // Untere Hälfte
};

enum class HorizontalSwingDirection : uint8_t {
	LEFT_RIGHT = 0,   // Links-rechts
	LEFTSIDE = 1,     // Linke Seite
	CENTER = 2,       // Zentrum
	RIGHTSIDE = 3,    // Rechte Seite
};

enum class AirflowVerticalDirection : uint8_t {
	LAST = 0,     // Letzte Position
	MAX_UP = 1,   // Ganz oben
	UP = 2,       // Obere Hälfte
	CENTER = 3,   // Mitte
	DOWN = 4,     // Untere Hälfte
	MAX_DOWN = 5, // Ganz unten
};

enum class AirflowHorizontalDirection : uint8_t {
	LAST = 0,      // Letzte Position
	MAX_LEFT = 1,  // Ganz links
	LEFT = 2,      // Linke Hälfte
	CENTER = 3,    // Mitte
	RIGHT = 4,     // Rechte Hälfte
	MAX_RIGHT = 5, // Ganz rechts
};

class tclacClimate : public climate::Climate, 
                     public esphome::uart::UARTDevice, 
                     public PollingComponent {

	private:
		uint8_t dataTX[38];
		uint8_t dataRX[61];
		uint8_t poll[8] = {0xBB,0x00,0x01,0x04,0x02,0x01,0x00,0xBD};
		
		bool beeper_status_;
		bool display_status_;
		bool force_mode_status_;
		bool module_display_status_;
		bool is_call_control;
		bool allow_take_control;
		
		uint8_t switch_climate_mode;
		uint8_t switch_fan_mode;
		uint8_t switch_swing_mode;
		uint8_t switch_preset;
		int target_temperature_set;
		
		AirflowVerticalDirection vertical_direction_;
		AirflowHorizontalDirection horizontal_direction_;
		VerticalSwingDirection vertical_swing_direction_;
		HorizontalSwingDirection horizontal_swing_direction_;
		
		std::set<ClimateMode> supported_modes_;
		std::set<ClimatePreset> supported_presets_;
		std::set<ClimateFanMode> supported_fan_modes_;
		std::set<ClimateSwingMode> supported_swing_modes_;
		
		GPIOPin *rx_led_pin_ = {};
		GPIOPin *tx_led_pin_ = {};
		
	public:
		tclacClimate() : PollingComponent(5 * 1000) {}
		
		void setup() override;
		void loop() override;
		void update() override;
		void control(const ClimateCall &call) override;
		ClimateTraits traits() override;
		
		void takeControl();
		void readData();
		void sendData(byte * message, byte size);  // ✅ FIX: byte statt uint8_t
		static String getHex(byte *message, byte size);  // ✅ Optional: String statt std::string
		static byte getChecksum(const byte * message, size_t size);
		void dataShow(bool flow, bool shine);
		
		// Config Setter
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

#endif //TCL_ESP_TCL_H
