//DRONE
//three sine oscillators - very close - creates modulations in amplitude

#include <ResponsiveAnalogRead.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

#include "display.h"

AudioSynthWaveform       waveform1;
AudioSynthWaveform       waveform2;
AudioSynthWaveform       waveform3;
AudioFilterStateVariable filter;
AudioMixer4              mixer;
AudioAmplifier           amp;
AudioAmplifier           amp_out_left;
AudioAmplifier           amp_out_right;
AudioEffectWaveshaper    waveshaper;
AudioEffectFreeverbStereo reverb;
AudioRecordQueue         queueLeft;
AudioRecordQueue         queueRight;
AudioOutputI2S2          i2s2;

AudioConnection          patchCord3(waveform1, 0, mixer, 0);
AudioConnection          patchCord4(waveform2, 0, mixer, 1);
AudioConnection          patchCord5(waveform3, 0, mixer, 2);
AudioConnection          patchCord6(mixer, 0, filter, 0);
AudioConnection          patchCord7(filter, 0, amp, 0);
AudioConnection          patchCord8(amp, 0, waveshaper, 0);
AudioConnection          patchCord10(waveshaper, 0, reverb, 0);
AudioConnection          patchCord12(reverb, 0, queueLeft, 0);
AudioConnection          patchCord13(reverb, 1, queueRight, 0);
AudioConnection          patchCord14(reverb, 0, amp_out_left, 0);
AudioConnection          patchCord15(reverb, 1, amp_out_right, 0);
AudioConnection          patchCord16(amp_out_left, 0, i2s2, 0);
AudioConnection          patchCord17(amp_out_right, 0, i2s2, 1);

Lethe::Display display;

// Snap multiplier for ResponsiveAnalogRead
#define SNAP_MULTIPLIER 0.01f
#define NUM_POTS 8

#define MIN_FREQUENCY 20
#define MAX_FREQUENCY 150

const uint8_t POT_PINS[NUM_POTS] = {A0, A1, A2, A3, A6, A7, A8, A9};
ResponsiveAnalogRead pots[NUM_POTS];

// @see https://github.com/joshnishikawa/TeensyAudioWaveshaper
const uint8_t WAVESHAPE_SIZE = 17;
float WAVESHAPE_CLIPPING[WAVESHAPE_SIZE] = {
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

const float REVERB_DAMPING = 0.6f;

void setup() {
  #if DEBUG
  Serial.begin(115200);
  #endif

  /**
   * Audio first so the queue can receive blocks before display is touched.
   */
  AudioMemory(85);

  queueLeft.begin();
  queueRight.begin();

  // waveform1.begin(0, 20, WAVEFORM_BANDLIMIT_SQUARE);
  // waveform2.begin(0, 20, WAVEFORM_BANDLIMIT_SQUARE);
  // waveform3.begin(0, 20, WAVEFORM_BANDLIMIT_SQUARE);
  waveform1.begin(0, 20, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE);
  waveform2.begin(0, 20, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE);
  waveform3.begin(0, 20, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE);

  mixer.gain(0, 0.25f);
  mixer.gain(1, 0.25f);
  mixer.gain(2, 0.25f);

  amp.gain(5.0f);
  amp_out_left.gain(0.5f);
  amp_out_right.gain(0.5f);

  waveshaper.shape(WAVESHAPE_CLIPPING, WAVESHAPE_SIZE);

  // Q ranges from 0.7 to 5.0. Resonance greater than 0.707 will amplify the signal near the corner frequency.
  filter.frequency(MAX_FREQUENCY);
  filter.resonance(2.0f);

  reverb.damping(REVERB_DAMPING);

  // Knob pots: smooth analog reads
  for (int i = 0; i < NUM_POTS; i++) {
    pots[i].begin(POT_PINS[i], true, SNAP_MULTIPLIER);
    pots[i].setAnalogResolution(1023);
  }

  display.begin();

  delay(2000);
}

void loop() {
  float pots_values[NUM_POTS];

  // Read the knobs (smoothed via ResponsiveAnalogRead)
  for (uint8_t i = 0; i < NUM_POTS; i++) {
    pots[i].update();
    pots_values[i] = (float)pots[i].getValue();
  }

  float osc1_freq = map(pots_values[0], 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY);
  float osc2_freq = map(pots_values[2], 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY);
  float osc3_freq = map(pots_values[4], 0, 1023, MIN_FREQUENCY, MAX_FREQUENCY);

  float osc1_amp = pots_values[1] / 1023.0f;
  float osc2_amp = pots_values[3] / 1023.0f;
  float osc3_amp = pots_values[5] / 1023.0f;

  // quick and dirty equation for exp scale frequency adjust
  float freq = expf(pots_values[6] / 150.0f) * 10.0f + 80.0f;

  // reverb roomsize: 0.0 to 1.0
  float roomsize = pots_values[7] / 1023.0f;

  AudioNoInterrupts();

  waveform1.frequency(osc1_freq);
  waveform2.frequency(osc2_freq);
  waveform3.frequency(osc3_freq);

  waveform1.amplitude(osc1_amp);
  waveform2.amplitude(osc2_amp);
  waveform3.amplitude(osc3_amp);

  filter.frequency(freq);

  reverb.roomsize(roomsize);

  AudioInterrupts();

  // Waveform scope: drain stereo queues and feed display; display throttles redraws.
  while (queueLeft.available() && queueRight.available()) {
    const int16_t *blockL = queueLeft.readBuffer();
    const int16_t *blockR = queueRight.readBuffer();
    display.feedScopeBlockStereo(blockL, blockR, 128);
    queueLeft.freeBuffer();
    queueRight.freeBuffer();
  }

  display.drawScopeIfDue();
}