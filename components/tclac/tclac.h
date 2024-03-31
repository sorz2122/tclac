/**
* Create by Miguel Ángel López on 20/07/19
* and modify by xaxexa
* Refactoring & component making
* Соловей с паяльником 15.03.2024
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

#define MODE_POS        7
#define MODE_MASK		0b00111111

#define MODE_AUTO      	0b00110101
#define MODE_COOL       0b00110001
#define MODE_DRY      	0b00110011
#define MODE_FAN_ONLY   0b00110010
#define MODE_HEAT       0b00110100

#define FAN_SPEED_POS	8
#define FAN_QUIET_POS	33


#define FAN_AUTO 		0b10000000	//auto
#define FAN_QUIET 		0x80		//silent
#define FAN_LOW 		0b10010000	//	|
#define FAN_MIDDLE 		0b11000000	//	||
#define FAN_MEDIUM  	0b10100000	//	|||
#define FAN_HIGH  		0b11010000	//	||||
#define FAN_FOCUS  		0b10110000	//	|||||
#define FAN_DIFFUSE		0b10000000	//	POWER [7]
#define FAN_SPEED_MASK	0b11110000	//FAN SPEED MASK


#define SWING_POS			10
#define SWING_OFF       	0b00000000
#define SWING_HORIZONTAL    0b00100000
#define SWING_VERTICAL      0b01000000
#define SWING_BOTH      	0b01100000
#define SWING_MODE_MASK    	0b01100000

#define STEP_TEMPERATURE 1
#define MIN_SET_TEMPERATURE 16
#define MAX_SET_TEMPERATURE 31

using climate::ClimateCall;
using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateTraits;
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

class tclacClimate : public climate::Climate, public esphome::uart::UARTDevice, public PollingComponent {

	private:
		byte checksum;
		// dataTX с управлением состоит из 38 байт
		byte dataTX[38];
		// А dataRX по прежнему из 61 байта
		byte dataRX[61];
		// Команда запроса состояния
		byte poll[8] = {0xBB,0x00,0x01,0x04,0x02,0x01,0x00,0xBD};
		// Инициализация и начальное наполнение переменных состоянй переключателей
		bool beeper_status_;
		bool display_status_;
		bool module_display_status_;
		
	public:

		tclacClimate() : PollingComponent(5 * 1000) {
			checksum = 0;
		}

		void setup() override;
		void loop() override;
		void update() override;
		void readData();
		void dataShow(bool flow, bool shine);
		void sendData(byte * message, byte size);
		static String getHex(byte *message, byte size);
		static byte getChecksum(const byte * message, size_t size);
		void control(const ClimateCall &call) override; // Climate control
		
		// Заготовки функций запроса состояния, может пригодиться в будущем, если делать обратную связь. Очень не хочется, будет очень костыльно.
		
		//bool get_beeper_state() const;
		//bool get_display_state() const;
		//bool tclacClimate::get_module_display_state() const;
		//AirflowVerticalDirection get_vertical_airflow() const;
		//AirflowHorizontalDirection get_horizontal_airflow() const;
		//VerticalSwingDirection tclacClimate::get_vertical_swing_direction() const;
		//HorizontalSwingDirection tclacClimate::get_horizontal_swing_direction() const;

		void set_beeper_state(bool state);		
		void set_display_state(bool state);
		void set_rx_led_pin(GPIOPin *rx_led_pin);
		void set_tx_led_pin(GPIOPin *tx_led_pin);
		void set_module_display_state(bool state);
		void set_vertical_airflow(AirflowVerticalDirection direction);
		void set_horizontal_airflow(AirflowHorizontalDirection direction);
		void set_vertical_swing_direction(VerticalSwingDirection direction);
		void set_horizontal_swing_direction(HorizontalSwingDirection direction);
		void set_supported_modes(const std::set<esphome::climate::ClimateMode> &modes);
		void set_supported_fan_modes(const std::set<esphome::climate::ClimateFanMode> &modes);
		void set_supported_swing_modes(const std::set<esphome::climate::ClimateSwingMode> &modes);
		
		
	protected:
		GPIOPin *rx_led_pin_;
		GPIOPin *tx_led_pin_;
		ClimateTraits traits() override;
//		std::set<ClimateMode> supported_modes_{};
		AirflowVerticalDirection vertical_direction_;
		AirflowHorizontalDirection horizontal_direction_;
		VerticalSwingDirection vertical_swing_direction_;
//		std::set<ClimateSwingMode> supported_swing_modes_{};
		HorizontalSwingDirection horizontal_swing_direction_;
};

}
}

#endif //TCL_ESP_TCL_H
