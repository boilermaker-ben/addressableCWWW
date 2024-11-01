#include <cinttypes>
#include "led_strip.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_attr.h>
#include <map>

namespace esphome {
namespace esp32_rmt_led_strip_channels {

static const char *const TAG = "esp32_rmt_led_strip_channels";

static const uint32_t RMT_CLK_FREQ = 80000000;
static const uint8_t RMT_CLK_DIV = 2;

void ESP32RMTLEDStripChannelsLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 LED Strip...");

  size_t buffer_size = this->get_buffer_size_();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buf_ = allocator.allocate(buffer_size);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    this->mark_failed();
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    this->mark_failed();
    return;
  }

  ExternalRAMAllocator<rmt_item32_t> rmt_allocator(ExternalRAMAllocator<rmt_item32_t>::ALLOW_FAILURE);
  this->rmt_buf_ = rmt_allocator.allocate(buffer_size * 8);  // 8 bits per byte, 1 rmt_item32_t per bit

  rmt_config_t config;
  memset(&config, 0, sizeof(config));
  config.channel = this->channel_;
  config.rmt_mode = RMT_MODE_TX;
  config.gpio_num = gpio_num_t(this->pin_);
  config.mem_block_num = 1;
  config.clk_div = RMT_CLK_DIV;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.idle_output_en = true;

  if (rmt_config(&config) != ESP_OK) {
    ESP_LOGE(TAG, "Cannot initialize RMT!");
    this->mark_failed();
    return;
  }
  if (rmt_driver_install(config.channel, 0, 0) != ESP_OK) {
    ESP_LOGE(TAG, "Cannot install RMT driver!");
    this->mark_failed();
    return;
  }

  int channel_index = 0;
  for (size_t i = 0; i < this->channels_.length(); ++i) {
    char channel = this->channels_[i];
    switch (channel) {
      case 'R':
        this->channel_map_[CHANNEL_RED] = channel_index++;
        break;
      case 'G':
        this->channel_map_[CHANNEL_GREEN] = channel_index++;
        break;
      case 'B':
        this->channel_map_[CHANNEL_BLUE] = channel_index++;
        break;
      case 'N':
        this->channel_map_[CHANNEL_NULL] = channel_index++;
        break;
      case 'C':
        this->channel_map_[CHANNEL_COLD_WHITE] = channel_index++;
        break;
      case 'W':
        if (this->channels_.find('C') != std::string::npos) {
          this->channel_map_[CHANNEL_WARM_WHITE] = channel_index++;
        } else {
          // Otherwise, this 'W' is just white
          this->channel_map_[CHANNEL_WHITE] = channel_index++;
        }
        break;
    }
  }
}

void ESP32RMTLEDStripChannelsLightOutput::set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high,
                                                 uint32_t bit1_low) {
  float ratio = (float) RMT_CLK_FREQ / RMT_CLK_DIV / 1e09f;

  // 0-bit
  this->bit0_.duration0 = (uint32_t) (ratio * bit0_high);
  this->bit0_.level0 = 1;
  this->bit0_.duration1 = (uint32_t) (ratio * bit0_low);
  this->bit0_.level1 = 0;
  // 1-bit
  this->bit1_.duration0 = (uint32_t) (ratio * bit1_high);
  this->bit1_.level0 = 1;
  this->bit1_.duration1 = (uint32_t) (ratio * bit1_low);
  this->bit1_.level1 = 0;
}

void ESP32RMTLEDStripChannelsLightOutput::write_state(light::LightState *state) {
  // protect from refreshing too often
  uint32_t now = micros();
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    // try again next loop iteration, so that this change won't get lost
    this->schedule_show();
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing channel values to bus...");

  if (rmt_wait_tx_done(this->channel_, pdMS_TO_TICKS(1000)) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->status_set_warning();
    return;
  }
  delayMicroseconds(50);

  size_t buffer_size = this->get_buffer_size_();

  size_t size = 0;
  size_t len = 0;
  uint8_t *psrc = this->buf_;
  rmt_item32_t *pdest = this->rmt_buf_;
  while (size < buffer_size) {
    uint8_t b = *psrc;
    for (int i = 0; i < 8; i++) {
      pdest->val = b & (1 << (7 - i)) ? this->bit1_.val : this->bit0_.val;
      pdest++;
      len++;
    }
    size++;
    psrc++;
  }

  if (rmt_write_items(this->channel_, this->rmt_buf_, len, false) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX error");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
}

light::ESPColorView ESP32RMTLEDStripChannelsLightOutput::get_view_internal(int32_t index) const {
  int32_t r = 0, g = 0, b = 0, w = 0, c = 0;
  auto get_channel = [&](char channel) -> int32_t {
    if (this->channel_map_.find(channel) != this->channel_map_.end() && channel != CHANNEL_NULL) {
      return reinterpret_cast<int32_t>(this->buf_ + (index * this->channel_map_.size()) + this->channel_map_.at(channel));
    }
    return -1; // Or another suitable default value indicating no valid pointer.
  };
  ESP_LOGVV(TAG, "Fetched channel map");
  uint8_t white = 0;
  
  r = get_channel(CHANNEL_RED);
  g = get_channel(CHANNEL_GREEN);
  b = get_channel(CHANNEL_BLUE);
  if (get_channel(CHANNEL_WHITE) == -1 && (get_channel(CHANNEL_COLD_WHITE) != -1 && get_channel(CHANNEL_WARM_WHITE) != -1)) {
    c = get_channel(CHANNEL_COLD_WHITE);
    w = get_channel(CHANNEL_WARM_WHITE);
    white = 1;
  } else if (get_channel(CHANNEL_WHITE) != -1) {
    w = get_channel(CHANNEL_WHITE);
    white = 1;
  }
  
  uint8_t multiplier = this->channels_.size();
  
  // If both cold and warm white are present, return them. Otherwise, return the generic white channel.
  if (c != -1 && w != -1) {
    return {
      this->buf_ + (index * multiplier) + w,
      this->buf_ + (index * multiplier) + c,
      nullptr,
      nullptr,
      &this->effect_data_[index],
      &this->correction_
    };
  } else {
    return {
        this->buf_ + (index * multiplier) + r + white,
        this->buf_ + (index * multiplier) + g + white,
        this->buf_ + (index * multiplier) + b + white,
        white == 1 ? this->buf_ + (index * multiplier) + w : nullptr,
        &this->effect_data_[index],
        &this->correction_};
    };
}

// void ESP32RMTLEDStripChannelsLightOutput::set_channels(const std::string &channels) {
//   this->channels_ = channels;
// }

void ESP32RMTLEDStripChannelsLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 RMT LED Strip:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);

  // Log the channels_ string directly
  ESP_LOGCONFIG(TAG, "  Channels: %s", this->channels_.c_str());

  ESP_LOGCONFIG(TAG, "  Max refresh rate: %" PRIu32, *this->max_refresh_rate_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %u", this->num_leds_);
}

float ESP32RMTLEDStripChannelsLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esp32_rmt_led_strip_channels
}  // namespace esphome

#endif  // USE_ESP32
