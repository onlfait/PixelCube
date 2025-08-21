## Pixel Cube LED Animation with PIR (Arduino + FastLED)

![PixelCube printing](https://github.com/onlfait/PixelCube/blob/main/img/PixelCube.gif)

Animated Pac‑style ghost across four LED matrix faces, triggered by a PIR motion sensor. Designed for Arduino (AVR) with **FastLED** and four separate LED outputs that share a single buffer.

### Features

* Four faces (Front, Right, Back, Left), each **256 LEDs** (e.g., 16×16).
* **Shared frame buffer**: one CRGB\[256] reused for all faces to save RAM on AVR.
* **PIR interrupt** on D3 triggers a special **ghost blink** animation immediately.
* All animation frame data stored in **PROGMEM** to save RAM.

### Hardware

* **MCU**: Arduino Uno/Nano (or compatible AVR). 5V recommended for WS281x.
* **LEDs**: WS2813 (or WS2812B) strips/panels.
* **PIR sensor**: connected to **D3** (interrupt pin on AVR).
* **Power**: Adequate 5V supply for your total LED count (inject power as needed). Add a common ground between MCU, LEDs, and PIR.
* **Level shifting** (recommended): 5V data where possible, or a proper level shifter from 5V MCU to LED data.

### Default Pinout

* Front: **D6**
* Right: **D8**
* Back: **D10**
* Left: **D12**
* PIR: **D3** (RISING edge interrupt)

> Adjust these in the defines at the top of the sketch.

### Wiring Tips

* Put a **\~330–470 Ω resistor** in series with each LED data line.
* Add a **1000 µF** electrolytic capacitor across LED 5V and GND near the first LED.
* Keep data lines short and routed away from noisy power.

### Software Setup

1. Install the **FastLED** library (Library Manager → "FastLED").
2. Open the `.ino` sketch in `src/` with the Arduino IDE / Arduino CLI / PlatformIO.
3. Select your board and port, then **Upload**.

### How It Works

* All four faces are represented by distinct FastLED controllers that **share the same `leds` buffer**. The code draws a frame into `leds`, then calls `showOn()` for a specific face/controller to push that frame to just that face. Repeat per face.
* The **PIR ISR** sets a `volatile` flag. The `loop()` checks this flag frequently via `handlePir()` and `smartDelay()`; when set, it immediately runs `ghostBlink()` and returns to the animation.
* Frame data (indexes to light) reside in **PROGMEM** and are drawn with helpers in the `fx` namespace.

### Customization

* **Brightness**: `FastLED.setBrightness(96);`
* **Color order**: `COLOR_ORDER` (default `GRB`).
* **Timing**: edit `smartDelay(100);` and the internal delays in `ghostBlink()`.
* **Pins / Face mapping**: change the `#define` pin list and the `Face` enum.

### Memory & Performance Notes

* On AVR, RAM is tight. Using a single 256‑LED buffer keeps memory usage low while still driving four faces.
* PROGMEM reduces SRAM pressure. Keep only the animation frames you actually use.

### Licensing

This project follows Fab Lab–friendly open licensing:

* **Software (Arduino sketches, .ino/.cpp):** MIT License (`LICENSE`)
* **Hardware designs (schematics, PCB, CAD):** CERN-OHL-S v2.0 (`LICENSES/CERN-OHL-S-2.0.txt`)
* **Documentation & media (README, images):** CC BY-SA 4.0 (`LICENSES/CC-BY-SA-4.0.txt`)

See `NOTICE` for an overview.

### License Summary

* MIT: Free use, modification, commercial use permitted — attribution required.
* CERN-OHL-S: Hardware must stay open (strong reciprocity).
* CC BY-SA: Documentation/media can be reused if credited and shared alike.

### Acknowledgments

* [FastLED](https://fastled.io/) for the LED control library.
* Fab Labs & the [Fab Charter](https://fabfoundation.org/about/fab-charter/) for the open‑knowledge principles.

---

## NOTICE

```
Project:Pixel Cube LED Animation with PIR

Software (Arduino sketches, headers, .ino/.cpp): MIT License (see LICENSE)
Hardware design files (if present): CERN-OHL-S v2.0 (see LICENSES/CERN-OHL-S-2.0.txt)
Documentation & media (README, images): CC BY-SA 4.0 (see LICENSES/CC-BY-SA-4.0.txt)

This structure aligns with the Fab Lab Charter’s emphasis on openness and
knowledge sharing. If you fork, please keep this NOTICE and credit original
contributors in your README.
```
