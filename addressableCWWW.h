// BASED ON NEOPIXELBUS CODE
#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/macros.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/color.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/addressable_light.h"

#include "NeoPixelBus.h"

namespace esphome {
namespace neopixelbusCWWW {

enum class ESPCWWWNeoPixelOrder {
  NCW = 0b10000111,
  NWC = 0b01001011,
  CNW = 0b10010011,
  WNC = 0b00011011,
  CWN = 0b01100011,
  WCN = 0b00100111,
};

template<typename T_METHOD, typename T_COLOR_FEATURE>
class NeoPixelBusLightOutputBase : public light::AddressableLight {
 public:
  NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *get_controller() const { return this->controller_; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

  /// Add some LEDS, can only be called once.
  void add_leds(uint16_t count_pixels, uint8_t pin) {
    this->add_leds(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(count_pixels, pin));
  }
  void add_leds(uint16_t count_pixels, uint8_t pin_clock, uint8_t pin_data) {
    this->add_leds(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(count_pixels, pin_clock, pin_data));
  }
  void add_leds(uint16_t count_pixels) { this->add_leds(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(count_pixels)); }
  void add_leds(NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *controller) {
    this->controller_ = controller;
    // controller gets initialised in setup() - avoid calling twice (crashes with RMT)
    // this->controller_->Begin();
  }

  // ========== INTERNAL METHODS ==========
  void setup() override {
    for (int i = 0; i < this->size(); i++) {
      (*this)[i] = Color(0, 0, 0);
    }

    this->effect_data_ = new uint8_t[this->size()];  // NOLINT
    this->controller_->Begin();
  }

  void write_state(light::LightState *state) override {
    this->mark_shown_();
    this->controller_->Dirty();

    this->controller_->Show();
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  int32_t size() const override { return this->controller_->PixelCount(); }

  void set_pixel_order(ESPCWWWNeoPixelOrder order) {
    uint8_t u_order = static_cast<uint8_t>(order);
    this->rgb_offsets_[0] = (u_order >> 6) & 0b11;
    this->rgb_offsets_[1] = (u_order >> 4) & 0b11;
    this->rgb_offsets_[2] = (u_order >> 2) & 0b11;
  }

 protected:
  NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *controller_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint8_t rgb_offsets_[4]{0, 1, 2, 3};
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbFeature>
class NeoPixelRGBLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE> {

// private:
//   float temperature_cw_{6000.0f};  // default cold white temperature
//   float temperature_ww_{3000.0f};  // default warm white temperature
public:
  void set_temperature_cw(float temperature_cw) { temperature_cw_ = temperature_cw; }
  void set_temperature_ww(float temperature_ww) { temperature_ww_ = temperature_ww; }
    
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLD_WARM_WHITE});
    traits.set_min_mireds(this->temperature_cw_); // MIN_MIREDS
    traits.set_max_mireds(this->temperature_ww_); // MAX_MIREDS
    return traits;
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {  // NOLINT
    uint8_t *base = this->controller_->Pixels() + 3ULL * index;
    return light::ESPColorView(base + this->rgb_offsets_[0], base + this->rgb_offsets_[1], base + this->rgb_offsets_[2],
                               nullptr, this->effect_data_ + index, &this->correction_);
  }
  float temperature_cw_;
  float temperature_ww_;  
};

}  // namespace neopixelbus
}  // namespace esphome

#endif  // USE_ARDUINO
