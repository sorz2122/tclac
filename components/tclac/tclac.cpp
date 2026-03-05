/**
* Create by Miguel Ángel López on 20/07/19
* and modify by xaxexa
* Refactoring & component making:
* Соловей с паяльником 15.03.2024
**/
#include "esphome.h"
#include "esphome/core/defines.h"
#include "tclac.h"

namespace esphome{
namespace tclac{

ClimateTraits tclacClimate::traits() {
	auto traits = climate::ClimateTraits();

	traits.set_supports_action(false);
	traits.set_supports_current_temperature(true);
	traits.set_supports_two_point_target_temperature(false);

	traits.set_visual_min_for_temperature(16.0);
	traits.set_visual_max_for_temperature(31.0);
	
	// KORRIGIERT: Nur supported_* verwenden, keine add_*
	traits.set_supported_modes(this->supported_modes_);
	traits.set_supported_presets(this->supported_presets_);
	traits.set_supported_fan_modes(this->supported_fan_modes_);
	traits.set_supported_swing_modes(this->supported_swing_modes_);

	return traits;
}

void tclacClimate::setup() {
#ifdef CONF_RX_LED
	this->rx_led_pin_->setup();
	this->rx_led_pin_->digital_write(false);
#endif
#ifdef CONF_TX_LED
	this->tx_led_pin_->setup();
	this->tx_led_pin_->digital_write(false);
#endif
}

void tclacClimate::loop()  {
	if (esphome::uart::UARTDevice::available() > 0) {
		dataShow(0, true);
		dataRX[0] = esphome::uart::UARTDevice::read();
		
		if (dataRX[0] != 0xBB) {
			ESP_LOGD("TCL", "Wrong byte");
			dataShow(0,0);
			return;
		}
		
		delay(5);
		dataRX[1] = esphome::uart::UARTDevice::read();
		delay(5);
		dataRX[2] = esphome::uart::UARTDevice::read();
		delay(5);
		dataRX[3] = esphome::uart::UARTDevice::read();
		delay(5);
		dataRX[4] = esphome::uart::UARTDevice::read();

		// KORRIGIERT: Korrekte Paketlänge berechnen
		int full_len = 5 + dataRX[4] + 1;  // Header(5) + Daten + CRC(1)
		esphome::uart::UARTDevice::read_array(dataRX+5, dataRX[4]+1);

		byte check = getChecksum(dataRX, full_len);
		if (check != dataRX[full_len-1]) {
			ESP_LOGD("TCL", "Invalid checksum %x != %x (len=%d)", check, dataRX[full_len-1], full_len);
			dataShow(0,0);
			return;
		}
		ESP_LOGD("TCL", "checksum OK %x (len=%d)", check, full_len);
		
		dataShow(0,0);
		tclacClimate::readData();
	}
}

void tclacClimate::update() {
	tclacClimate::dataShow(1,1);
	this->esphome::uart::UARTDevice::write_array(poll, sizeof(poll));
	tclacClimate::dataShow(1,0);
}

void tclacClimate::readData() {
	current_temperature = float((( (dataRX[17] << 8) | dataRX[18] ) / 374 - 32)/1.8);
	target_temperature = (dataRX[FAN_SPEED_POS] & SET_TEMP_MASK) + 16;

	ESP_LOGD("TCL", "TEMP: %.1f°C / Target: %.1f°C", current_temperature, target_temperature);

	if (dataRX[MODE_POS] & ( 1 << 4)) {
		uint8_t modeswitch = MODE_MASK & dataRX[MODE_POS];
		uint8_t fanspeedswitch = FAN_SPEED_MASK & dataRX[FAN_SPEED_POS];
		uint8_t swingmodeswitch = SWING_MODE_MASK & dataRX[SWING_POS];

		switch (modeswitch) {
			case MODE_AUTO:    mode = climate::CLIMATE_MODE_AUTO; break;
			case MODE_COOL:    mode = climate::CLIMATE_MODE_COOL; break;
			case MODE_DRY:     mode = climate::CLIMATE_MODE_DRY; break;
			case MODE_FAN_ONLY: mode = climate::CLIMATE_MODE_FAN_ONLY; break;
			case MODE_HEAT:    mode = climate::CLIMATE_MODE_HEAT; break;
			default:           mode = climate::CLIMATE_MODE_AUTO;
		}

		if ( dataRX[FAN_QUIET_POS] & FAN_QUIET) {
			fan_mode = climate::CLIMATE_FAN_QUIET;
		} else if (dataRX[MODE_POS] & FAN_DIFFUSE){
			fan_mode = climate::CLIMATE_FAN_DIFFUSE;
		} else {
			switch (fanspeedswitch) {
				case FAN_AUTO:   fan_mode = climate::CLIMATE_FAN_AUTO; break;
				case FAN_LOW:    fan_mode = climate::CLIMATE_FAN_LOW; break;
				case FAN_MIDDLE: fan_mode = climate::CLIMATE_FAN_MIDDLE; break;
				case FAN_MEDIUM: fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
				case FAN_HIGH:   fan_mode = climate::CLIMATE_FAN_HIGH; break;
				case FAN_FOCUS:  fan_mode = climate::CLIMATE_FAN_FOCUS; break;
				default:         fan_mode = climate::CLIMATE_FAN_AUTO;
			}
		}

		switch (swingmodeswitch) {
			case SWING_OFF:      swing_mode = climate::CLIMATE_SWING_OFF; break;
			case SWING_HORIZONTAL: swing_mode = climate::CLIMATE_SWING_HORIZONTAL; break;
			case SWING_VERTICAL:   swing_mode = climate::CLIMATE_SWING_VERTICAL; break;
			case SWING_BOTH:       swing_mode = climate::CLIMATE_SWING_BOTH; break;
		}
		
		preset = ClimatePreset::CLIMATE_PRESET_NONE;
		if (dataRX[7] & (1 << 6)) preset = ClimatePreset::CLIMATE_PRESET_ECO;
		else if (dataRX[9] & (1 << 2)) preset = ClimatePreset::CLIMATE_PRESET_COMFORT;
		else if (dataRX[19] & (1 << 0)) preset = ClimatePreset::CLIMATE_PRESET_SLEEP;
		
	} else {
		mode = climate::CLIMATE_MODE_OFF;
		swing_mode = climate::CLIMATE_SWING_OFF;
		preset = ClimatePreset::CLIMATE_PRESET_NONE;
	}
	
	this->publish_state();
	allow_take_control = true;
}

void tclacClimate::control(const ClimateCall &call) {
	// KORRIGIERT: Sichere Preset/Fan-Handling
	if (call.get_mode().has_value()){
		switch_climate_mode = call.get_mode().value();
		ESP_LOGD("TCL", "MODE from HA: %d", (int)switch_climate_mode);
	} else {
		switch_climate_mode = mode;
	}
	
	if (call.get_preset().has_value()){
		switch_preset = call.get_preset().value();
	} else {
		switch_preset = ClimatePreset::CLIMATE_PRESET_NONE;  // SAFE!
	}
	
	if (call.get_fan_mode().has_value()){
		switch_fan_mode = call.get_fan_mode().value();
	} else {
		switch_fan_mode = climate::CLIMATE_FAN_AUTO;  // SAFE!
	}
	
	if (call.get_swing_mode().has_value()){
		switch_swing_mode = call.get_swing_mode().value();
	} else {
		switch_swing_mode = swing_mode;
	}
	
	if (call.get_target_temperature().has_value()) {
		target_temperature_set = 31 - (int)std::round(call.get_target_temperature().value());
		ESP_LOGD("TCL", "Target temp set: %.1f°C -> %d", call.get_target_temperature().value(), target_temperature_set);
	} else {
		target_temperature_set = 31 - (int)target_temperature;
	}
	
	is_call_control = true;
	takeControl();
	allow_take_control = true;
}

void tclacClimate::takeControl() {
	dataTX[7]  = 0b00000000;
	dataTX[8]  = 0b00000000;
	dataTX[9]  = 0b00000000;
	dataTX[10] = 0b00000000;
	dataTX[11] = 0b00000000;
	dataTX[19] = 0b00000000;
	dataTX[32] = 0b00000000;
	dataTX[33] = 0b00000000;
	
	if (!is_call_control){
		switch_climate_mode = mode;
		switch_preset = ClimatePreset::CLIMATE_PRESET_NONE;
		switch_fan_mode = climate::CLIMATE_FAN_AUTO;
		switch_swing_mode = swing_mode;
		target_temperature_set = 31-(int)target_temperature;
	}
	
	// Beeper
	if (beeper_status_){
		dataTX[7] += 0b00100000;
	}
	
	// Display (nur bei ON)
	if (display_status_ && switch_climate_mode != climate::CLIMATE_MODE_OFF){
		dataTX[7] += 0b01000000;
	}
	
	// Climate Mode
	switch (switch_climate_mode) {
		case climate::CLIMATE_MODE_OFF:    break;
		case climate::CLIMATE_MODE_AUTO:   dataTX[7] += 0b00000100; dataTX[8] += 0b00001000; break;
		case climate::CLIMATE_MODE_COOL:   dataTX[7] += 0b00000100; dataTX[8] += 0b00000011; break;
		case climate::CLIMATE_MODE_DRY:    dataTX[7] += 0b00000100; dataTX[8] += 0b00000010; break;
		case climate::CLIMATE_MODE_FAN_ONLY: dataTX[7] += 0b00000100; dataTX[8] += 0b00000111; break;
		case climate::CLIMATE_MODE_HEAT:   dataTX[7] += 0b00000100; dataTX[8] += 0b00000001; break;
	}

	// Fan Mode
	switch(switch_fan_mode) {
		case climate::CLIMATE_FAN_AUTO:   dataTX[8] += 0b00000000; dataTX[10] += 0b00000000; break;
		case climate::CLIMATE_FAN_QUIET:  dataTX[8] += 0b10000000; dataTX[10] += 0b00000000; break;
		case climate::CLIMATE_FAN_LOW:    dataTX[8] += 0b00000000; dataTX[10] += 0b00000001; break;
		case climate::CLIMATE_FAN_MIDDLE: dataTX[8] += 0b00000000; dataTX[10] += 0b00000110; break;
		case climate::CLIMATE_FAN_MEDIUM: dataTX[8] += 0b00000000; dataTX[10] += 0b00000011; break;
		case climate::CLIMATE_FAN_HIGH:   dataTX[8] += 0b00000000; dataTX[10] += 0b00000111; break;
		case climate::CLIMATE_FAN_FOCUS:  dataTX[8] += 0b00000000; dataTX[10] += 0b00000101; break;
		case climate::CLIMATE_FAN_DIFFUSE: dataTX[8] += 0b01000000; dataTX[10] += 0b00000000; break;
	}
	
	// Swing Mode
	switch(switch_swing_mode) {
		case climate::CLIMATE_SWING_OFF:      dataTX[10] += 0b00000000; dataTX[11] += 0b00000000; break;
		case climate::CLIMATE_SWING_VERTICAL: dataTX[10] += 0b00111000; dataTX[11] += 0b00000000; break;
		case climate::CLIMATE_SWING_HORIZONTAL: dataTX[10] += 0b00000000; dataTX[11] += 0b00001000; break;
		case climate::CLIMATE_SWING_BOTH:     dataTX[10] += 0b00111000; dataTX[11] += 0b00001000; break;
	}
	
	// Presets
	switch(switch_preset) {
		case ClimatePreset::CLIMATE_PRESET_ECO:     dataTX[7]  += 0b10000000; break;
		case ClimatePreset::CLIMATE_PRESET_SLEEP:   dataTX[19] += 0b00000001; break;
		case ClimatePreset::CLIMATE_PRESET_COMFORT: dataTX[8]  += 0b00010000; break;
	}

	// Airflow Directions (rest unverändert)
	switch(vertical_swing_direction_) {
		case VerticalSwingDirection::UP_DOWN:  dataTX[32] += 0b00001000; break;
		case VerticalSwingDirection::UPSIDE:   dataTX[32] += 0b00010000; break;
		case VerticalSwingDirection::DOWNSIDE: dataTX[32] += 0b00011000; break;
	}
	switch(horizontal_swing_direction_) {
		case HorizontalSwingDirection::LEFT_RIGHT: dataTX[33] += 0b00001000; break;
		case HorizontalSwingDirection::LEFTSIDE:   dataTX[33] += 0b00010000; break;
		case HorizontalSwingDirection::CENTER:     dataTX[33] += 0b00011000; break;
		case HorizontalSwingDirection::RIGHTSIDE:  dataTX[33] += 0b00100000; break;
	}
	switch(vertical_direction_) {
		case AirflowVerticalDirection::LAST:    dataTX[32] += 0b00000000; break;
		case AirflowVerticalDirection::MAX_UP:  dataTX[32] += 0b00000001; break;
		case AirflowVerticalDirection::UP:      dataTX[32] += 0b00000010; break;
		case AirflowVerticalDirection::CENTER:  dataTX[32] += 0b00000011; break;
		case AirflowVerticalDirection::DOWN:    dataTX[32] += 0b00000100; break;
		case AirflowVerticalDirection::MAX_DOWN: dataTX[32] += 0b00000101; break;
	}
	switch(horizontal_direction_) {
		case AirflowHorizontalDirection::LAST:     dataTX[33] += 0b00000000; break;
		case AirflowHorizontalDirection::MAX_LEFT: dataTX[33] += 0b00000001; break;
		case AirflowHorizontalDirection::LEFT:     dataTX[33] += 0b00000010; break;
		case AirflowHorizontalDirection::CENTER:   dataTX[33] += 0b00000011; break;
		case AirflowHorizontalDirection::RIGHT:    dataTX[33] += 0b00000100; break;
		case AirflowHorizontalDirection::MAX_RIGHT: dataTX[33] += 0b00000101; break;
	}

	dataTX[9] = target_temperature_set;
		
	// TX Packet Header
	dataTX[0] = 0xBB; dataTX[1] = 0x00; dataTX[2] = 0x01; dataTX[3] = 0x03; dataTX[4] = 0x20;
	dataTX[5] = 0x03; dataTX[6] = 0x01;
	dataTX[12] = 0x00; dataTX[13] = 0x01; dataTX[14] = 0x00;
	dataTX[15] = dataTX[16] = dataTX[17] = dataTX[18] = 0x00;
	dataTX[20] = dataTX[21] = dataTX[22] = dataTX[23] = dataTX[24] = 0x00;
	dataTX[25] = dataTX[26] = dataTX[27] = dataTX[28] = 0x00;
	dataTX[30] = dataTX[31] = 0x00; dataTX[34] = dataTX[35] = dataTX[36] = 0x00;
	
	// KORRIGIERT: Korrekte Checksum-Länge
	int tx_len = 38;  // Korrekte TX-Paketlänge
	dataTX[37] = getChecksum(dataTX, tx_len);

	ESP_LOGD("TCL", "TX: Mode=%d Fan=%d Temp=%d", (int)switch_climate_mode, (int)switch_fan_mode, target_temperature_set);
	tclacClimate::sendData(dataTX, tx_len);
	allow_take_control = false;
	is_call_control = false;
}

void tclacClimate::sendData(byte * message, byte size) {
	tclacClimate::dataShow(1,1);
	this->esphome::uart::UARTDevice::write_array(message, size);
	ESP_LOGD("TCL", "TX sent (%d bytes)", size);
	tclacClimate::dataShow(1,0);
}

String tclacClimate::getHex(byte *message, byte size) {
	String raw;
	for (int i = 0; i < size; i++) {
		if (message[i] < 0x10) raw += "0";
		raw += String(message[i], HEX);
		raw += " ";
	}
	return raw;
}

byte tclacClimate::getChecksum(const byte * message, size_t size) {
	byte crc = 0;
	for (size_t i = 0; i < size-1; i++) {
		crc ^= message[i];
	}
	return crc;
}

void tclacClimate::dataShow(bool flow, bool shine) {
	if (module_display_status_){
		if (flow == 0){
			if (shine == 1){
#ifdef CONF_RX_LED
				this->rx_led_pin_->digital_write(true);
#endif
			} else {
#ifdef CONF_RX_LED
				this->rx_led_pin_->digital_write(false);
#endif
			}
		}
		if (flow == 1) {
			if (shine == 1){
#ifdef CONF_TX_LED
				this->tx_led_pin_->digital_write(true);
#endif
			} else {
#ifdef CONF_TX_LED
				this->tx_led_pin_->digital_write(false);
#endif
			}
		}
	}
}

// Config Setter (unverändert)
void tclacClimate::set_beeper_state(bool state) {
	this->beeper_status_ = state;
	if (force_mode_status_ && allow_take_control) tclacClimate::takeControl();
}
void tclacClimate::set_display_state(bool state) {
	this->display_status_ = state;
	if (force_mode_status_ && allow_take_control) tclacClimate::takeControl();
}
void tclacClimate::set_force_mode_state(bool state) { this->force_mode_status_ = state; }
#ifdef CONF_RX_LED
void tclacClimate::set_rx_led_pin(GPIOPin *rx_led_pin) { this->rx_led_pin_ = rx_led_pin; }
#endif
#ifdef CONF_TX_LED
void tclacClimate::set_tx_led_pin(GPIOPin *tx_led_pin) { this->tx_led_pin_ = tx_led_pin; }
#endif
void tclacClimate::set_module_display_state(bool state) { this->module_display_status_ = state; }
void tclacClimate::set_vertical_airflow(AirflowVerticalDirection direction) {
	this->vertical_direction_ = direction;
	if (force_mode_status_ && allow_take_control) tclacClimate::takeControl();
}
void tclacClimate::set_horizontal_airflow(AirflowHorizontalDirection direction) {
	this->horizontal_direction_ = direction;
	if (force_mode_status_ && allow_take_control) tclacClimate::takeControl();
}
void tclacClimate::set_vertical_swing_direction(VerticalSwingDirection direction) {
	this->vertical_swing_direction_ = direction;
	if (force_mode_status_ && allow_take_control) tclacClimate::takeControl();
}
void tclacClimate::set_horizontal_swing_direction(HorizontalSwingDirection direction) {
	this->horizontal_swing_direction_ = direction;
	if (force_mode_status_ && allow_take_control) tclacClimate::takeControl();
}
void tclacClimate::set_supported_modes(const std::set<climate::ClimateMode> &modes) { this->supported_modes_ = modes; }
void tclacClimate::set_supported_fan_modes(const std::set<climate::ClimateFanMode> &modes) { this->supported_fan_modes_ = modes; }
void tclacClimate::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) { this->supported_swing_modes_ = modes; }
void tclacClimate::set_supported_presets(const std::set<climate::ClimatePreset> &presets) { this->supported_presets_ = presets; }

}
}
