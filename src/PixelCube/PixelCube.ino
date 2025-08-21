#include <FastLED.h>
#include <avr/pgmspace.h>

// ================== CONFIG ==================
#define FACES 4
#define NUM_LEDS_PER_FACE 256

// Data pins per face (FRONT, RIGHT, BACK, LEFT)
#define PIN_FRONT 6
#define PIN_RIGHT 8
#define PIN_BACK 10
#define PIN_LEFT 12

#define COLOR_ORDER GRB

// PIR sensor (must be an interrupt-capable pin on AVR)
#define PIR_SENSOR 3

// ========= PIR flag & helpers ==========
volatile bool pirFlag = false;  // Set in ISR when motion is detected

// ISR — keep it ultra short
void onPirRise() {
  pirFlag = true;
}

// Delay that exits early if PIR triggers
inline void smartDelay(uint16_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    if (pirFlag) return;  // exit as soon as movement is detected
    delay(1);
  }
}

// Call often to launch the special animation when motion is detected
inline void handlePir();

// ============================================

// Single buffer reused for all faces (fits on Uno)
CRGB leds[NUM_LEDS_PER_FACE];
// One controller per face, all using the same buffer pointer
CLEDController* ctrl[FACES];

// -------- helpers in a namespace to avoid Arduino's prototype generator --------
namespace fx {
  template<size_t N>
  inline uint16_t readIdx(const uint16_t (&arr)[N], size_t i) {
    return pgm_read_word_near(&arr[i]);
  }

  template<size_t N>
  inline void colorList(const uint16_t (&idx)[N], const CRGB& c) {
    for (size_t i = 0; i < N; ++i) {
      const uint16_t k = readIdx(idx, i);
      if (k < NUM_LEDS_PER_FACE) leds[k] = c;
    }
  }

  inline void colorRange(uint16_t a, uint16_t b, const CRGB& c) {
    if (a > b) { const uint16_t t = a; a = b; b = t; }
    for (uint16_t i = a; i <= b && i < NUM_LEDS_PER_FACE; ++i) leds[i] = c;
  }

  template<size_t NA, size_t NB, size_t NC>
  inline void drawFrame(const uint16_t (&A)[NA], const CRGB& cA,
                        const uint16_t (&B)[NB], const CRGB& cB,
                        const uint16_t (&C)[NC], const CRGB& cC) {
    // start from black so no leftovers
    fill_solid(leds, NUM_LEDS_PER_FACE, CRGB::Black);
    colorList(A, cA);
    colorList(B, cB);
    colorList(C, cC);
  }

  // Send current buffer to a specific controller
  inline void showOn(CLEDController* c) {
    // respect global brightness
    c->showLeds(FastLED.getBrightness());
  }
}  // namespace fx

// -------------- DATA (PROGMEM) --------------
const uint16_t eye_white_1[] PROGMEM = { 35, 51, 55, 54, 64, 65, 66, 67, 68, 84, 83, 82, 125, 145, 144, 141, 154, 155, 156, 157, 158, 174, 173, 172 };
const uint16_t eye_white_2[] PROGMEM = { 65, 66, 67, 81, 82, 83, 84, 85, 94, 95, 114, 155, 156, 157, 171, 172, 173, 174, 175, 184, 185, 188, 204 };
const uint16_t eyes_1[] PROGMEM = { 36, 53, 126, 143, 37, 52, 127, 142 };
const uint16_t eyes_2[] PROGMEM = { 96, 113, 186, 203, 97, 112, 187, 202 };

const uint16_t body_1[] PROGMEM = {
  15, 16, 17, 18, 19, 20, 21, 22, 34, 38, 39, 40, 41, 42, 43, 47, 48, 49, 50, 56, 62, 63, 69, 70, 71, 72, 73,
  75, 76, 77, 78, 79, 80, 81, 85, 86, 87, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104,
  107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 121, 122, 123, 124, 128, 129, 130, 131, 132,
  135, 136, 137, 138, 139, 140, 146, 147, 148, 152, 153, 159, 160, 161, 162, 163, 164, 166, 167, 168, 169,
  170, 171, 175, 176, 177, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 196, 197, 198, 199, 200, 201,
  202, 203, 204, 205, 217, 218, 219, 220, 221, 222, 223, 224
};
const uint16_t body_2[] PROGMEM = {
  16, 17, 18, 19, 20, 21, 22, 34, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 56, 62, 63, 69, 70, 71, 72, 73,
  77, 78, 79, 80, 81, 85, 86, 87, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 105, 106, 107, 108, 109,
  110, 111, 112, 113, 114, 115, 116, 117, 118, 121, 122, 123, 124, 128, 129, 130, 131, 132, 133, 134, 136, 137,
  138, 139, 140, 146, 147, 148, 152, 153, 159, 160, 161, 162, 166, 167, 168, 169, 170, 171, 175, 176, 177, 183,
  184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205,
  217, 218, 219, 220, 221, 222, 223
};
const uint16_t body_3[] PROGMEM = {
  15, 16, 17, 18, 19, 20, 21, 22, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
  62, 63, 64, 68, 69, 70, 71, 72, 73, 75, 76, 77, 78, 79, 80, 86, 87, 91, 92, 93, 99, 100, 101, 102, 103, 104,
  107, 108, 109, 110, 111, 115, 116, 117, 118, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 152, 153, 154, 158, 159, 160, 161,
  162, 163, 164, 166, 167, 168, 169, 170, 176, 177, 183, 189, 190, 191, 192, 196, 197, 198, 199, 200, 201,
  205, 217, 218, 219, 220, 221, 222, 223, 224
};
const uint16_t body_4[] PROGMEM = {
  16, 17, 18, 19, 20, 21, 22, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
  62, 63, 64, 68, 69, 70, 71, 72, 73, 77, 78, 79, 80, 86, 87, 91, 92, 93, 99, 100, 101, 102, 103, 105, 106, 107, 108,
  109, 110, 111, 115, 116, 117, 118, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 136,
  137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 152, 153, 154, 158, 159, 160, 161, 162, 166, 167,
  168, 169, 170, 176, 177, 183, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 205, 217, 218,
  219, 220, 221, 222, 223
};

const uint16_t dead_fantom_1[] PROGMEM = { 15, 16, 17, 18, 19, 20, 21, 22, 34, 35, 36, 37, 38, 39, 40, 42, 43, 47, 48, 50, 51, 52, 53, 54, 55, 56,
                                           62, 63, 64, 65, 66, 67, 68, 69, 71, 72, 73, 75, 76, 77, 79, 80, 81, 84, 85, 86, 87, 91, 92, 93, 94, 95, 98, 99, 100, 103, 104, 107, 108, 110, 111,
                                           112, 113, 114, 115, 116, 117, 118, 121, 122, 123, 124, 125, 126, 127, 128, 129, 131, 132, 135, 136, 137, 139, 140, 141, 144, 145, 146, 147, 148, 152,
                                           153, 154, 155, 158, 159, 160, 162, 163, 164, 166, 167, 168, 170, 171, 172, 173, 174, 175, 176, 177, 184, 185, 186, 187, 188, 189, 190, 191, 192,
                                           196, 197, 199, 200, 201, 202, 203, 204, 205, 217, 218, 219, 220, 221, 222, 223, 224 };
const uint16_t dead_fantom_2[] PROGMEM = { 16, 17, 18, 19, 20, 21, 22, 34, 35, 36, 37, 38, 39, 40, 42, 43, 44, 45, 46, 47, 48, 50, 51, 52, 53, 54, 55, 56,
                                           62, 63, 64, 65, 66, 67, 68, 69, 71, 72, 73, 77, 79, 80, 81, 84, 85, 86, 87, 91, 92, 93, 94, 95, 98, 99, 100,
                                           102, 103, 105, 106, 108, 110, 111, 112, 113, 114, 115, 116, 117, 118, 121, 122, 123, 124, 125, 126, 127, 128, 129,
                                           131, 132, 133, 134, 136, 137, 139, 140, 141, 144, 145, 146, 147, 148, 152, 153, 154, 155, 158, 159, 160,
                                           162, 166, 167, 168, 170, 171, 172, 173, 174, 175, 176, 177, 184, 185, 186, 187, 188, 189, 190, 191, 192,
                                           193, 194, 195, 196, 197, 199, 200, 201, 202, 203, 204, 205, 217, 218, 219, 220, 221, 222, 223 };

const uint16_t fantom_eyes_mouth[] PROGMEM = { 41, 49, 70, 78, 82, 83, 96, 97, 101, 109, 130, 138, 142, 143, 156, 157, 161, 169, 190, 198 };

const uint16_t dummy[] PROGMEM = { 0 };

// -----------------------------------------------

// Face names
enum Face : uint8_t { FRONT = 0, RIGHT = 1, BACK = 2, LEFT = 3 };

void ghostBlink() {
  for (int i = 0; i <= 8; i++) {
    fx::drawFrame(dead_fantom_1, CRGB::Blue, fantom_eyes_mouth, CRGB::Red, dummy, CRGB::Black);
    fx::showOn(ctrl[FRONT]);
    fx::showOn(ctrl[RIGHT]);
    fx::showOn(ctrl[LEFT]);
    fx::showOn(ctrl[BACK]);
    delay(100);

    fx::drawFrame(dead_fantom_2, CRGB::White, fantom_eyes_mouth, CRGB::Red, dummy, CRGB::Black);
    fx::showOn(ctrl[FRONT]);
    fx::showOn(ctrl[RIGHT]);
    fx::showOn(ctrl[LEFT]);
    fx::showOn(ctrl[BACK]);
    delay(100);
  }
}

inline void handlePir() {
  if (!pirFlag) return;
  pirFlag = false;
  ghostBlink();
  // small debounce / de-saturation (optional)
  delay(50);
}

void setup() {
  pinMode(PIR_SENSOR, INPUT);
  // Rising-edge interrupt when PIR output goes HIGH
  attachInterrupt(digitalPinToInterrupt(PIR_SENSOR), onPirRise, RISING);

  // All controllers share the same buffer "leds"
  ctrl[FRONT] = &FastLED.addLeds<WS2813, PIN_FRONT, COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);
  ctrl[RIGHT] = &FastLED.addLeds<WS2813, PIN_RIGHT, COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);
  ctrl[BACK]  = &FastLED.addLeds<WS2813, PIN_BACK,  COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);
  ctrl[LEFT]  = &FastLED.addLeds<WS2813, PIN_LEFT,  COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);

  FastLED.setBrightness(96);
  fill_solid(leds, NUM_LEDS_PER_FACE, CRGB::Black);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  for (int i = 0; i <= 4; i++) {
    // --- frame 1
    fx::drawFrame(body_1, CRGB::Magenta, eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[RIGHT]);  handlePir(); if (pirFlag) return;
    fx::drawFrame(body_1, CRGB::Cyan,    eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[LEFT]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_1, CRGB::Orange,  eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[BACK]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_1, CRGB::Red,     eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[FRONT]);  handlePir(); if (pirFlag) return;
    smartDelay(100); if (pirFlag) { handlePir(); return; }

    // --- frame 2
    fx::drawFrame(body_2, CRGB::Magenta, eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[RIGHT]);  handlePir(); if (pirFlag) return;
    fx::drawFrame(body_2, CRGB::Cyan,    eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[LEFT]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_2, CRGB::Orange,  eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[BACK]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_2, CRGB::Red,     eye_white_1, CRGB::White, eyes_1, CRGB::Blue);
    fx::showOn(ctrl[FRONT]);  handlePir(); if (pirFlag) return;
    smartDelay(100); if (pirFlag) { handlePir(); return; }
  }

  for (int i = 0; i <= 4; i++) {
    // --- frame 3
    fx::drawFrame(body_3, CRGB::Magenta, eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[RIGHT]);  handlePir(); if (pirFlag) return;
    fx::drawFrame(body_3, CRGB::Cyan,    eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[LEFT]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_3, CRGB::Orange,  eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[BACK]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_3, CRGB::Red,     eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[FRONT]);  handlePir(); if (pirFlag) return;
    smartDelay(100); if (pirFlag) { handlePir(); return; }

    // --- frame 4
    fx::drawFrame(body_4, CRGB::Magenta, eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[RIGHT]);  handlePir(); if (pirFlag) return;
    fx::drawFrame(body_4, CRGB::Cyan,    eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[LEFT]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_4, CRGB::Orange,  eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[BACK]);   handlePir(); if (pirFlag) return;
    fx::drawFrame(body_4, CRGB::Red,     eye_white_2, CRGB::White, eyes_2, CRGB::Blue);
    fx::showOn(ctrl[FRONT]);  handlePir(); if (pirFlag) return;
    smartDelay(100); if (pirFlag) { handlePir(); return; }
  }

}
