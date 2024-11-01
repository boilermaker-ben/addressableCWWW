#pragma once

#ifdef USE_ESP32

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <driver/gpio.h>
#include <driver/rmt.h>
#include <esp_err.h>
#include <map>

namespace esphome {
namespace esp32_rmt_led_strip_channels {

const char CHANNEL_RED = 'R';
const char CHANNEL_GREEN = 'G';
const char CHANNEL_BLUE = 'B';
const char CHANNEL_WHITE = 'W';
const char CHANNEL_COLD_WHITE = 'C';
const char CHANNEL_WARM_WHITE = 'W';
const char CHANNEL_NULL = 'N';  // Null channel

class ESP32RMTLEDStripChannelsLightOutput : public light::AddressableLight {
 public:
 
  void set_cold_white_temperature(float cold_white_temperature) { cold_white_temperature_ = cold_white_temperature; }
  void set_warm_white_temperature(float warm_white_temperature) { warm_white_temperature_ = warm_white_temperature; }
  bool has_cold_warm_white = this->has_channel(CHANNEL_COLD_WHITE) && this->has_channel(CHANNEL_WARM_WHITE);
  bool has_white = this->has_channel(CHANNEL_WHITE) && !has_cold_warm_white;
  bool has_rgb = this->has_channel(CHANNEL_RED) && this->has_channel(CHANNEL_GREEN) && this->has_channel(CHANNEL_BLUE);
  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;
  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (has_cold_warm_white) {
      traits.set_min_mireds(this->cold_white_temperature_);
      traits.set_max_mireds(this->warm_white_temperature_);
      if (has_rgb) {
        traits.set_supported_color_modes({light::ColorMode::RGB_COLOR_TEMPERATURE});
      } else {
        traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE}); // ColorMode::COLD_WARM_WHITE});
      }
    } else if (has_white) {
      if (has_rgb) {
        traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
      } else {
        traits.set_supported_color_modes({light::ColorMode::WHITE});
      }
    } else {
      traits.set_supported_color_modes({light::ColorMode::RGB});
    }
    return traits;
  }

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint16_t num_leds) { this->num_leds_ = num_leds; }
  void set_channels(const std::string &channels) { this->channels_ = channels; }
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high, uint32_t bit1_low);

  void set_rmt_channel(rmt_channel_t channel) { this->channel_ = channel; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

  void dump_config() override;

 private:
  std::map<char, int> channel_map_;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t get_buffer_size_() const { return this->num_leds_ * this->channels_.size(); }

  bool has_channel(char channel) const {
    return this->channels_.find(channel) != std::string::npos;
  }

  float cold_white_temperature_;
  float warm_white_temperature_;
  
  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
  rmt_item32_t *rmt_buf_{nullptr};

  uint8_t pin_;
  uint16_t num_leds_;
  std::string channels_;

  rmt_item32_t bit0_, bit1_;
  rmt_channel_t channel_;

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
};

}  // namespace esp32_rmt_led_strip_channels
}  // namespace esphome

#endif  // USE_ESP32
