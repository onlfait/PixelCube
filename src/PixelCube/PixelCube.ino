/*
  PixelCube — non-blocking 4-face LED animation with motion-triggered "ghost blink"

  Features
  --------
  - 4 faces (FRONT/RIGHT/BACK/LEFT), each on its own data pin, all sharing one LED buffer.
  - "Body" animation cycles non-blocking through frames {1,2} then {3,4}.
  - Per-face body colors rotate (1) whenever motion is detected and (2) every 30 s when idle.
  - Motion sensor on A0 (analog). ONE threshold to tune: MOTION_TH.
    * Hysteresis is derived automatically to avoid flicker.
    * On motion "rising" event, play a 3 s "ghost blink" on all faces, then go back to the body loop.
  - All timings are millis()-based and rollover-safe (signed comparisons).

  Wiring
  ------
  - LED strips: WS2813 (change the chipset in addLeds<> if needed)
    FRONT = D6, RIGHT = D8, BACK = D10, LEFT = D12
  - Motion sensor analog output -> A0 (GND and +5V as per your sensor)
  - LED color order: GRB (adjust COLOR_ORDER if your strips differ)

  Tuning knobs
  ------------
  - MOTION_TH: the only motion threshold you usually need to adjust.
  - BLINK_DURATION_MS: how long the "ghost blink" runs after a detection.
  - BODY_PERIOD_MS / BODY_REPS_PER_PAIR: speed/feel of the body animation.
  - COLOR_ROTATE_IDLE_MS: how often colors rotate with no motion.

  Notes
  -----
  - If your sensor is "active-low" (value DROPS when you approach), simply invert the two comparisons
    in the "Presence detection with hysteresis" block below (see comments there).
  - Memory: a single 256-LED buffer is reused across faces to reduce RAM usage (e.g., on Arduino Uno).
*/

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

// Cadences (ms)
#define BODY_PERIOD_MS          100   // body frames cadence (non-blocking)
#define BODY_REPS_PER_PAIR      5     // do 5 round-trips 1<->2, then 5 round-trips 3<->4
#define BLINK_PERIOD_MS         100   // ghost blink frame cadence
#define BLINK_STEPS_TOTAL       18    // two-frame ghost alternates; cycle length is 18 steps
#define BLINK_DURATION_MS       3000  // fixed 3 s ghost blink after each detection

// Motion / color-rotation
#define MOTION_PIN              A0
#define MOTION_TH               100   // <<< single threshold to tune (ON threshold)
#define COLOR_ROTATE_IDLE_MS    30000 // rotate colors when idle (no motion) every 30 s
// ============================================

// Single buffer reused for all faces (fits on Uno)
CRGB leds[NUM_LEDS_PER_FACE];
// One controller per face (all show from the same buffer)
CLEDController* ctrl[FACES];

// ---------------- helpers (PROGMEM-safe) ----------------
namespace fx {
  // Read a PROGMEM uint16_t from an index table
  template<size_t N>
  inline uint16_t readIdx(const uint16_t (&arr)[N], size_t i) {
    return pgm_read_word_near(&arr[i]);
  }

  // Paint a list of pixel indices with a color
  template<size_t N>
  inline void colorList(const uint16_t (&idx)[N], const CRGB& c) {
    for (size_t i = 0; i < N; ++i) {
      const uint16_t k = readIdx(idx, i);
      if (k < NUM_LEDS_PER_FACE) leds[k] = c;
    }
  }

  // Draw a frame composed of three index sets (A/B/C) with their colors
  template<size_t NA, size_t NB, size_t NC>
  inline void drawFrame(const uint16_t (&A)[NA], const CRGB& cA,
                        const uint16_t (&B)[NB], const CRGB& cB,
                        const uint16_t (&C)[NC], const CRGB& cC) {
    fill_solid(leds, NUM_LEDS_PER_FACE, CRGB::Black);
    colorList(A, cA);
    colorList(B, cB);
    colorList(C, cC);
  }

  // Push the buffer out through a given controller
  inline void showOn(CLEDController* c) {
    c->showLeds(FastLED.getBrightness());
  }
} // namespace fx

// -------------- DATA (PROGMEM) --------------
// (Body/eyes/ghost index tables; unchanged)
const uint16_t eye_white_1[] PROGMEM = { 35, 51, 55, 54, 64, 65, 66, 67, 68, 84, 83, 82, 125, 145, 144, 141, 154, 155, 156, 157, 158, 174, 173, 172 };
const uint16_t eye_white_2[] PROGMEM = { 65, 66, 67, 81, 82, 83, 84, 85, 94, 95,98, 114, 155, 156, 157, 171, 172, 173, 174, 175, 184, 185, 188, 204 };
const uint16_t eyes_1[] PROGMEM = { 36, 53, 126, 143, 37, 52, 127, 142 };
const uint16_t eyes_2[] PROGMEM = { 96, 113, 186, 203, 97, 112, 187, 202 };

const uint16_t body_1[] PROGMEM = {
  15,16,17,18,19,20,21,22,34,38,39,40,41,42,43,47,48,49,50,56,62,63,69,70,71,72,73,
  75,76,77,78,79,80,81,85,86,87,91,92,93,94,95,96,97,98,99,100,101,102,103,104,
  107,108,109,110,111,112,113,114,115,116,117,118,121,122,123,124,128,129,130,131,132,
  135,136,137,138,139,140,146,147,148,152,153,159,160,161,162,163,164,166,167,168,169,
  170,171,175,176,177,183,184,185,186,187,188,189,190,191,192,196,197,198,199,200,201,
  202,203,204,205,217,218,219,220,221,222,223,224
};
const uint16_t body_2[] PROGMEM = {
  16,17,18,19,20,21,22,34,38,39,40,41,42,43,44,45,46,47,48,49,50,56,62,63,69,70,71,72,73,
  77,78,79,80,81,85,86,87,91,92,93,94,95,96,97,98,99,100,101,102,103,105,106,107,108,109,
  110,111,112,113,114,115,116,117,118,121,122,123,124,128,129,130,131,132,133,134,136,137,
  138,139,140,146,147,148,152,153,159,160,161,162,166,167,168,169,170,171,175,176,177,183,
  184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,
  217,218,219,220,221,222,223
};
const uint16_t body_3[] PROGMEM = {
  15,16,17,18,19,20,21,22,34,35,36,37,38,39,40,41,42,43,47,48,49,50,51,52,53,54,55,56,
  62,63,64,68,69,70,71,72,73,75,76,77,78,79,80,86,87,91,92,93,99,100,101,102,103,104,
  107,108,109,110,111,115,116,117,118,121,122,123,124,125,126,127,128,129,130,131,132,
  135,136,137,138,139,140,141,142,143,144,145,146,147,148,152,153,154,158,159,160,161,
  162,163,164,166,167,168,169,170,176,177,183,189,190,191,192,196,197,198,199,200,201,
  205,217,218,219,220,221,222,223,224
};
const uint16_t body_4[] PROGMEM = {
  16,17,18,19,20,21,22,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,
  62,63,64,68,69,70,71,72,73,77,78,79,80,86,87,91,92,93,99,100,101,102,103,105,106,107,108,
  109,110,111,115,116,117,118,121,122,123,124,125,126,127,128,129,130,131,132,133,134,136,
  137,138,139,140,141,142,143,144,145,146,147,148,152,153,154,158,159,160,161,162,166,167,
  168,169,170,176,177,183,189,190,191,192,193,194,195,196,197,198,199,200,201,205,217,218,
  219,220,221,222,223
};

const uint16_t dead_fantom_1[] PROGMEM = { 15,16,17,18,19,20,21,22,34,35,36,37,38,39,40,42,43,47,48,50,51,52,53,54,55,56,
  62,63,64,65,66,67,68,69,71,72,73,75,76,77,79,80,81,84,85,86,87,91,92,93,94,95,98,99,100,103,104,107,108,110,111,
  112,113,114,115,116,117,118,121,122,123,124,125,126,127,128,129,131,132,135,136,137,139,140,141,144,145,146,147,148,152,
  153,154,155,158,159,160,162,163,164,166,167,168,170,171,172,173,174,175,176,177,184,185,186,187,188,189,190,191,192,
  196,197,199,200,201,202,203,204,205,217,218,219,220,221,222,223,224 };
const uint16_t dead_fantom_2[] PROGMEM = { 16,17,18,19,20,21,22,34,35,36,37,38,39,40,42,43,44,45,46,47,48,50,51,52,53,54,55,56,
  62,63,64,65,66,67,68,69,71,72,73,77,79,80,81,84,85,86,87,91,92,93,94,95,98,99,100,
  102,103,105,106,108,110,111,112,113,114,115,116,117,118,121,122,123,124,125,126,127,128,129,
  131,132,133,134,136,137,139,140,141,144,145,146,147,148,152,153,154,155,158,159,160,
  162,166,167,168,170,171,172,173,174,175,176,177,184,185,186,187,188,189,190,191,192,
  193,194,195,196,197,199,200,201,202,203,204,205,217,218,219,220,221,222,223 };
const uint16_t fantom_eyes_mouth[] PROGMEM = { 41, 49, 70, 78, 82, 83, 96, 97, 101, 109, 130, 138, 142, 143, 156, 157, 161, 169, 190, 198 };

const uint16_t dummy[] PROGMEM = { 0 };

// ---------------- Face names ----------------
enum Face : uint8_t { FRONT = 0, RIGHT = 1, BACK = 2, LEFT = 3 };

// ---- Rotating per-face colors (FRONT, RIGHT, BACK, LEFT) ----
CRGB bodyColors[FACES] = { CRGB::Red, CRGB::Magenta, CRGB::Orange, CRGB::Cyan };
uint32_t nextColorRotateAt = 0;

// ----------------- Animation state (non-blocking) -----------------
struct BodyState {
  uint8_t  pair = 0;       // 0 => frames 1<->2 (eyes set 1), 1 => frames 3<->4 (eyes set 2)
  uint8_t  stepInPair = 0; // toggles 0/1 at BODY_PERIOD_MS cadence
  uint8_t  repsInPair = 0; // how many round-trips completed in the current pair
  uint32_t nextAt = 0;     // next time to step (millis)
} body;

struct BlinkState {
  bool     active = false; // ghost blink currently displayed
  uint8_t  step = 0;       // 0..BLINK_STEPS_TOTAL-1
  uint32_t nextAt = 0;     // next time to toggle ghost frame
  uint32_t stopAt = 0;     // absolute time when the blink must stop
} blink;

// Presence latch (level): true = presence currently detected
bool presence = false;

// ----------------- Utilities -----------------
static inline void rotateColors() {
  // Rotate colors one face forward: LEFT -> FRONT -> RIGHT -> BACK -> LEFT
  CRGB tmp = bodyColors[FACES - 1];      // keep LEFT
  for (int i = FACES - 1; i > 0; --i) {  // shift RIGHT,BACK,LEFT downward
    bodyColors[i] = bodyColors[i - 1];
  }
  bodyColors[0] = tmp;                    // FRONT takes previous LEFT color
}

static inline void renderBodyFrameForFace(Face f, uint8_t globalFrameIndex) {
  // globalFrameIndex: 0->body_1, 1->body_2, 2->body_3, 3->body_4
  const CRGB eyesColor = CRGB::Blue;
  const CRGB white     = CRGB::White;

  switch (globalFrameIndex) {
    case 0: fx::drawFrame(body_1, bodyColors[f], eye_white_1, white, eyes_1, eyesColor); break;
    case 1: fx::drawFrame(body_2, bodyColors[f], eye_white_1, white, eyes_1, eyesColor); break;
    case 2: fx::drawFrame(body_3, bodyColors[f], eye_white_2, white, eyes_2, eyesColor); break;
    default: fx::drawFrame(body_4, bodyColors[f], eye_white_2, white, eyes_2, eyesColor); break;
  }
  fx::showOn(ctrl[f]);
}

static inline void renderBlinkFrameAllFaces(bool useAlt) {
  // Draw one of the two ghost frames to the buffer and push to all faces
  const CRGB eyesMouth = CRGB::Red;
  const CRGB body1     = CRGB::Blue;
  const CRGB body2     = CRGB::White;

  if (!useAlt) {
    fx::drawFrame(dead_fantom_1, body1, fantom_eyes_mouth, eyesMouth, dummy, CRGB::Black);
  } else {
    fx::drawFrame(dead_fantom_2, body2, fantom_eyes_mouth, eyesMouth, dummy, CRGB::Black);
  }
  fx::showOn(ctrl[FRONT]);
  fx::showOn(ctrl[RIGHT]);
  fx::showOn(ctrl[LEFT]);
  fx::showOn(ctrl[BACK]);
}

// ------------------ Setup & Loop (non-blocking) ------------------
void setup() {
  // Serial.begin(115200); // uncomment if you want to debug the sensor values

  // Each face gets its own controller; all share the same "leds" buffer.
  ctrl[FRONT] = &FastLED.addLeds<WS2813, PIN_FRONT, COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);
  ctrl[RIGHT] = &FastLED.addLeds<WS2813, PIN_RIGHT, COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);
  ctrl[BACK]  = &FastLED.addLeds<WS2813, PIN_BACK,  COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);
  ctrl[LEFT]  = &FastLED.addLeds<WS2813, PIN_LEFT,  COLOR_ORDER>(leds, NUM_LEDS_PER_FACE);

  FastLED.setBrightness(96);
  fill_solid(leds, NUM_LEDS_PER_FACE, CRGB::Black);
  FastLED.clear(true);

  const uint32_t now = millis();
  body.nextAt         = now + BODY_PERIOD_MS;
  nextColorRotateAt   = now + COLOR_ROTATE_IDLE_MS;
}

void loop() {
  const uint32_t now = millis();

  // ---------- Presence detection with hysteresis ----------
  // We derive a small OFF threshold (TH_OFF) from the single public knob MOTION_TH (TH_ON).
  // This prevents rapid toggling when the sensor sits around the threshold.
  const int TH_ON  = MOTION_TH;
  const int HYST   = max(4, TH_ON / 3);     // ~33% of TH_ON, minimum 4 ADC counts
  const int TH_OFF = max(0, TH_ON - HYST);  // OFF is lower than ON

  const int motionVal = analogRead(MOTION_PIN);
  // If your sensor is ACTIVE-LOW (value drops when you approach), invert these two lines:
  // if (!presence && motionVal <= TH_ON) presence = true;
  // else if (presence && motionVal >= TH_OFF) presence = false;

  bool wasPresent = presence;
  if (!presence && motionVal >= TH_ON) {
    presence = true;   // rising level (entered presence)
  } else if (presence && motionVal <= TH_OFF) {
    presence = false;  // falling level (left presence)
  }

  // ---------- On rising edge: rotate colors once + start a 3 s blink ----------
  if (!wasPresent && presence) {
    rotateColors();

    blink.active = true;
    blink.step   = 0;
    blink.nextAt = now;                         // trigger immediately
    blink.stopAt = now + BLINK_DURATION_MS;     // fixed-length blink

    nextColorRotateAt = now + COLOR_ROTATE_IDLE_MS; // reset idle rotation timer
  }

  // ---------- While the blink is active: show the ghost and preempt the body ----------
  if (blink.active) {
    if ((int32_t)(now - blink.nextAt) >= 0) {   // rollover-safe
      bool useAlt = (blink.step & 0x01);        // alternate 0/1
      renderBlinkFrameAllFaces(useAlt);

      // advance ghost step and wrap at BLINK_STEPS_TOTAL
      blink.step = (uint8_t)((blink.step + 1) % BLINK_STEPS_TOTAL);
      blink.nextAt = now + BLINK_PERIOD_MS;
    }

    // Stop after BLINK_DURATION_MS
    if ((int32_t)(now - blink.stopAt) < 0) {
      return; // during the 3 s window, we only show the ghost
    } else {
      blink.active = false; // fall through to the regular body animation
    }
  }

  // ---------- Idle color rotation (no motion) ----------
  if ((int32_t)(now - nextColorRotateAt) >= 0) { // rollover-safe
    rotateColors();
    nextColorRotateAt = now + COLOR_ROTATE_IDLE_MS;
  }

  // ---------- Body animation (non-blocking) ----------
  if ((int32_t)(now - body.nextAt) >= 0) {       // rollover-safe
    // Step the "pair" (0/1 toggles) at BODY_PERIOD_MS cadence
    body.stepInPair ^= 1;

    // Count round-trips: when we go 1->0 we completed one back-and-forth
    static uint8_t lastStep = 1;
    if (body.stepInPair == 0 && lastStep == 1) {
      body.repsInPair++;
      if (body.repsInPair >= BODY_REPS_PER_PAIR) {
        body.repsInPair = 0;
        body.pair ^= 1; // switch 1<->2 <-> 3<->4
      }
    }
    lastStep = body.stepInPair;

    // Map (pair,step) to a global frame index 0..3
    uint8_t globalFrameIndex;
    if (body.pair == 0) { // 1<->2
      globalFrameIndex = (body.stepInPair == 0) ? 0 : 1;
    } else {              // 3<->4
      globalFrameIndex = (body.stepInPair == 0) ? 2 : 3;
    }

    // Render per-face with its current color
    renderBodyFrameForFace(RIGHT, globalFrameIndex);
    renderBodyFrameForFace(LEFT,  globalFrameIndex);
    renderBodyFrameForFace(BACK,  globalFrameIndex);
    renderBodyFrameForFace(FRONT, globalFrameIndex);

    body.nextAt = now + BODY_PERIOD_MS;
  }
}
