# addressableCWWW
CURRENTLY IN DEVELOPMENT AND NOT FUNCTIONAL!

Custom ESPHome component to control an addressable CWWW LED strip (WS2811) that takes RGB input to control the warm and cold white channels.
This is the Amazon listing for the product:
https://www.amazon.com/BTF-LIGHTING-Addressable-Flexible-3000K-6000K-Decoration/dp/B0BNDRF325

Copy all files into an "addressableCWWW" folder within the "custom_components" ESPHome directory, then reference the component at the top of the ESPHome YAML config file as such:
```
esphome:
  # https://esphome.io/components/esphome
  name: ${device_name}
  includes:
    - custom_components/addressableCWWW/addressableCWWW.h
```

Then create a light with this format:
```
light:
  - platform: addressableCWWW
    name: ${device_name}_under_cabinet
    id: ${device_name}_under_cabinet
    type: CWN # C = Cold / W = Warm / N = Null (adjust mapping order as needed)
    variant: WS2811 # As far as I know this is the only chip that comes like this
    method: ESP32_I2S_0 # This is the only method I've tested, which works when converted to an RGB light, rather than WARM_COLD_WHITE
    cold_white_temperature: "6000 K"
    warm_white_temperature: "3000 K"
    pin: 25 # Set to the correct output pin on your ESP device
    num_leds: 22 # LEDs are in blocks of 24 cool white and 24 warm white (sections of 83mm length), therefore "chunks" of light are visible at a time and it doesn't have an individually addressable effect
    effects:
      - strobe
      - pulse
      - addressable_scan
```
