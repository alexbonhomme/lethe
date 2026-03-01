//DRONE
//three sine oscillators - very close - creates modulations in amplitude

#include <ResponsiveAnalogRead.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// SH1106 I2C: default 0x3C, try 0x3D if display does not respond
#define DISPLAY_I2C_ADDR 0x3C
#define DISPLAY_W 128
#define DISPLAY_H 64

// GUItool: begin automatically generated code
AudioSynthNoisePink      noise;
AudioSynthWaveform       waveform1;
AudioSynthWaveform       waveform2;
AudioSynthWaveform       waveform3;
AudioFilterStateVariable filter_noise;
AudioMixer4              mixer;
AudioAmplifier           amp;
AudioAmplifier           amp_out;
AudioEffectWaveshaper    waveshaper;
AudioRecordQueue         queue;
AudioOutputI2S2          i2s2;
AudioConnection          patchCord1(noise, 0, filter_noise, 0);
AudioConnection          patchCord2(noise, 0, filter_noise, 1);
AudioConnection          patchCord3(waveform1, 0, mixer, 0);
AudioConnection          patchCord4(waveform2, 0, mixer, 1);
AudioConnection          patchCord5(waveform3, 0, mixer, 2);
AudioConnection          patchCord6(filter_noise, 0, mixer, 3);
AudioConnection          patchCord7(mixer, 0, amp, 0);
AudioConnection          patchCord8(amp, 0, waveshaper, 0);
AudioConnection          patchCord9(waveshaper, 0, amp_out, 0);
AudioConnection          patchCord10(waveshaper, 0, queue, 0);
AudioConnection          patchCord11(amp_out, 0, i2s2, 0);
AudioConnection          patchCord12(amp_out, 0, i2s2, 1);
// GUItool: end automatically generated code

// SH1106 128x64 I2C (Teensy 18=SDA, 19=SCL). Slower I2C clock for Teensy compatibility.
Adafruit_SH1106G display(DISPLAY_W, DISPLAY_H, &Wire, -1, 200000, 200000);

// Throttle display updates so the loop drains the queue often (avoids audio crackling).
const unsigned long SCOPE_UPDATE_MS = 50;
unsigned long lastScopeUpdate = 0;

// Fixed scale so the period shape is clear (not over-zoomed). ~22 pixels for full scale.
#define SCOPE_FULL_SCALE_PX 56
// Ring buffer: last SCOPE_SAMPLES so the period is visible (~11.6 ms at 512, ~181 ms at 8000).
#define SCOPE_SAMPLES 2048
#define SCOPE_DECIMATE (SCOPE_SAMPLES / DISPLAY_W)  // so we get 128 pixels from the buffer
static int16_t scopeBuffer[SCOPE_SAMPLES];
static int scopeBufCount = 0;

// Snap multiplier for ResponsiveAnalogRead
#define SNAP_MULTIPLIER 0.01f
#define NUM_POTS 8

#define MIN_FREQUENCY 20
#define MAX_FREQUENCY 150

const uint8_t POT_PINS[NUM_POTS] = {A0, A1, A2, A3, A6, A7, A8, A9};
ResponsiveAnalogRead pots[NUM_POTS];

// @see https://github.com/joshnishikawa/TeensyAudioWaveshaper
float WAVESHAPE_CLIPPING[17] = {
  -0.588,
  -0.579,
  -0.549,
  -0.488,
  -0.396,
  -0.320,
  -0.228,
  -0.122,
  0.0,
  0.122,
  0.228,
  0.320,
  0.396,
  0.488,
  0.549,
  0.579,
  0.588
};

void setup() {
  #if DEBUG
  Serial.begin(115200);
  #endif

  /**
   * Display
   */
  display.begin(DISPLAY_I2C_ADDR, true);
  display.setRotation(2);  // 180° so the display is right-side up

  // Splash screen
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  const char* title = "Drone & Drama";
  int16_t x1, y1;
  uint16_t tw, th;
  display.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((DISPLAY_W - tw) / 2 - x1, (DISPLAY_H - th) / 2 - y1);
  display.print(title);
  display.display();

  /**
   * Audio
   */
  AudioMemory(50);

  queue.begin();

  // waveform1.begin(0, 20, WAVEFORM_BANDLIMIT_SQUARE);
  // waveform2.begin(0, 20, WAVEFORM_BANDLIMIT_SQUARE);
  // waveform3.begin(0, 20, WAVEFORM_BANDLIMIT_SQUARE);
  waveform1.begin(0, 20, WAVEFORM_SINE);
  waveform2.begin(0, 20, WAVEFORM_SINE);
  waveform3.begin(0, 20, WAVEFORM_SINE);
  noise.amplitude(0);

  mixer.gain(0, 0.25f);
  mixer.gain(1, 0.25f);
  mixer.gain(2, 0.25f);
  mixer.gain(3, 0.15f);

  amp.gain(5.0f);
  amp_out.gain(0.75f);

  waveshaper.shape(WAVESHAPE_CLIPPING, 17);

  filter_noise.frequency(55);
  filter_noise.resonance(4.0f);
  //Q ranges from 0.7 to 5.0. Resonance greater than 0.707 will amplify the signal near the corner frequency.

  // Knob pots: smooth analog reads (ResponsiveAnalogRead)
  for (int i = 0; i < NUM_POTS; i++) {
    pots[i].begin(POT_PINS[i], true, SNAP_MULTIPLIER);
    pots[i].setAnalogResolution(1023);
  }
}

void loop() {
  float pots_values[NUM_POTS];

  // Read the knobs (smoothed via ResponsiveAnalogRead)
  for (uint8_t i = 0; i < NUM_POTS; i++) {
    pots[i].update();
    pots_values[i] = (float)pots[i].getValue();
  }

  // quick and dirty equation for exp scale frequency adjust
  float freq = expf(pots_values[6] / 150.0f) * 10.0f + 80.0f;

  AudioNoInterrupts();

  #if DEBUG
  Serial.println(pots_values[0]);
  #endif

  waveform1.frequency(map(pots_values[0], 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY));
  waveform2.frequency(map(pots_values[2], 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY));
  waveform3.frequency(map(pots_values[4], 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY));
  filter_noise.frequency(freq);

  waveform1.amplitude(pots_values[1] / 1023.0f);
  waveform2.amplitude(pots_values[3] / 1023.0f);
  waveform3.amplitude(pots_values[5] / 1023.0f);
  noise.amplitude(pots_values[7] / 1023.0f);

  AudioInterrupts();

  // Waveform scope: drain all blocks and feed into ring buffer (last SCOPE_SAMPLES).
  // Display shows 512 samples decimated to 128 pixels (~11.6 ms) so the period is visible.
  while (queue.available()) {
    const int16_t* block = queue.readBuffer();
    if (scopeBufCount + 128 <= SCOPE_SAMPLES) {
      memcpy(scopeBuffer + scopeBufCount, block, 128 * sizeof(int16_t));
      scopeBufCount += 128;
    } else {
      memmove(scopeBuffer, scopeBuffer + 128, (SCOPE_SAMPLES - 128) * sizeof(int16_t));
      memcpy(scopeBuffer + SCOPE_SAMPLES - 128, block, 128 * sizeof(int16_t));
    }
    queue.freeBuffer();
  }

  if (scopeBufCount >= SCOPE_SAMPLES) {
    const unsigned long now = millis();
    if (now - lastScopeUpdate >= SCOPE_UPDATE_MS) {
      lastScopeUpdate = now;
      const int centerY = DISPLAY_H / 2;


      display.clearDisplay();
      for (int x = 0; x < DISPLAY_W - 1; x++) {
        int i0 = x * SCOPE_DECIMATE;
        int i1 = (x + 1) * SCOPE_DECIMATE;
        int y0 = centerY - ((int)scopeBuffer[i0] * SCOPE_FULL_SCALE_PX >> 15);
        int y1 = centerY - ((int)scopeBuffer[i1] * SCOPE_FULL_SCALE_PX >> 15);
        y0 = (y0 < 0) ? 0 : (y0 > DISPLAY_H - 1) ? DISPLAY_H - 1 : y0;
        y1 = (y1 < 0) ? 0 : (y1 > DISPLAY_H - 1) ? DISPLAY_H - 1 : y1;
        display.drawLine(x, y0, x + 1, y1, SH110X_WHITE);
      }
      display.display();
    }
  }
}