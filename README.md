# addressableCWWW
CURRENTLY IN DEVELOPMENT AND NOT FUNCTIONAL!

Custom ESPHome component to control an addressable CWWW LED strip (WS2811) that takes RGB input to control the warm and cold white channels. Attempting to make it work with the RMT platform.

This is the Amazon listing for the product:
https://www.amazon.com/BTF-LIGHTING-Addressable-Flexible-3000K-6000K-Decoration/dp/B0BNDRF325

Copy all files into an "esp32_rmt_led_strip_channels" folder within the "custom_components" ESPHome directory, then reference the component in the ESPHome YAML config file as such:
```
external_components:
  - source: custom_components/
```

Then create a light with this format:
```
light:
  - platform: esp32_rmt_led_strip_channels
    channels: CWN
    pin: 25
    num_leds: 22
    rmt_channel: 0
    chipset: ws2812
    name: ${device_name}_under_cabinet
    id: ${device_name}_under_cabinet
    # cold_white_color_temperature: "6000 K"
    # warm_white_color_temperature: "3000 K"
    # effects:
    #   - strobe
    #   - pulse
    #   - addressable_scan
```
