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
AudioAmplifier           ampOutLeft;
AudioAmplifier           ampOutRight;
AudioEffectWaveshaper    waveshaper;
AudioEffectFreeverbStereo reverb;
AudioMixer4              mixerOutL;
AudioMixer4              mixerOutR;
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
AudioConnection          patchCord11a(waveshaper, 0, mixerOutL, 0);
AudioConnection          patchCord11b(waveshaper, 0, mixerOutR, 0);
AudioConnection          patchCord12(reverb, 0, mixerOutL, 1);
AudioConnection          patchCord13(reverb, 1, mixerOutR, 1);
AudioConnection          patchCord14(mixerOutL, 0, queueLeft, 0);
AudioConnection          patchCord15(mixerOutR, 0, queueRight, 0);
AudioConnection          patchCord16(mixerOutL, 0, ampOutLeft, 0);
AudioConnection          patchCord17(mixerOutR, 0, ampOutRight, 0);
AudioConnection          patchCord18(ampOutLeft, 0, i2s2, 0);
AudioConnection          patchCord19(ampOutRight, 0, i2s2, 1);

Lethe::Display display;

// Snap multiplier for ResponsiveAnalogRead
const float kSnapMultiplier = 0.01f;
const uint8_t kNumPots = 8;
const int kMinFrequency = 20;
const int kMaxFrequency = 150;

const uint8_t kPotPins[kNumPots] = {A0, A1, A2, A3, A6, A7, A8, A9};
ResponsiveAnalogRead pots[kNumPots];

// @see https://github.com/joshnishikawa/TeensyAudioWaveshaper
const uint8_t kWaveshapeSize = 17;
float kWaveshapeClipping[kWaveshapeSize] = {
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

const float kReverbDamping = 0.5f;
// Ramp time for reverb bypass/on to avoid clicks (~20–50 ms at typical loop rate)
const float kReverbRampCoeff = 0.12f;

float targetReverbWet = 0.0f;
float currentReverbWet = 0.0f;

void setup() {
  #if DEBUG
  Serial.begin(115200);
  #endif

  /**
   * Audio first so the queue can receive blocks before display is touched.
   */
  AudioMemory(84);

  queueLeft.begin();
  queueRight.begin();

  waveform1.begin(0, 20, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE);
  waveform2.begin(0, 20, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE);
  waveform3.begin(0, 20, WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE);

  mixer.gain(0, 0.33f);
  mixer.gain(1, 0.33f);
  mixer.gain(2, 0.33f);

  mixerOutL.gain(1, 1.0f);
  mixerOutR.gain(1, 1.0f);
  mixerOutL.gain(1, 0.0f);
  mixerOutR.gain(1, 0.0f);

  amp.gain(5.0f);
  ampOutLeft.gain(0.35f);
  ampOutRight.gain(0.35f);

  waveshaper.shape(kWaveshapeClipping, kWaveshapeSize);

  // Q ranges from 0.7 to 5.0. Resonance greater than 0.707 will amplify the signal near the corner frequency.
  filter.frequency(kMaxFrequency);
  filter.resonance(2.0f);

  // Reverb damping: 0.0 to 1.0
  reverb.damping(kReverbDamping);

  // Knob pots: smooth analog reads
  for (int i = 0; i < kNumPots; i++) {
    pots[i].begin(kPotPins[i], true, kSnapMultiplier);
    pots[i].setAnalogResolution(1023);
  }

  // Display: splash screen and start waveform display
  display.begin();

  delay(2000);
}

void loop() {
  // Read the knobs (smoothed via ResponsiveAnalogRead)
  for (uint8_t i = 0; i < kNumPots; i++) {
    pots[i].update();
  }

  bool anyChange = false;
  for (uint8_t i = 0; i < kNumPots; i++) {
    if (pots[i].hasChanged()) {
      anyChange = true;
      break;
    }
  }

  if (anyChange) {
    if (pots[0].hasChanged()) {
      const int v = pots[0].getValue();
      waveform1.frequency(map(v, 0, 1023, kMinFrequency, kMaxFrequency));
    }
    if (pots[2].hasChanged()) {
      const int v = pots[2].getValue();
      waveform2.frequency(map(v, 0, 1023, kMinFrequency, kMaxFrequency));
    }
    if (pots[4].hasChanged()) {
      const int v = pots[4].getValue();
      waveform3.frequency(map(v, 0, 1023, kMinFrequency, kMaxFrequency));
    }

    if (pots[1].hasChanged()) {
      waveform1.amplitude(pots[1].getValue() / 1023.0f);
    }
    if (pots[3].hasChanged()) {
      waveform2.amplitude(pots[3].getValue() / 1023.0f);
    }
    if (pots[5].hasChanged()) {
      waveform3.amplitude(pots[5].getValue() / 1023.0f);
    }

    if (pots[6].hasChanged()) {
      const float p6 = (float)pots[6].getValue();
      filter.frequency(expf(p6 / 150.0f) * 10.0f + 80.0f);
    }

    if (pots[7].hasChanged()) {
      const float roomSize = pots[7].getValue() / 1023.0f;
      reverb.roomsize(roomSize);
      targetReverbWet = roomSize < 0.01f ? 0.0f : 1.0f;
    }
  }

  // Smooth reverb wet ramp to avoid clicks when bypassing or activating
  if (currentReverbWet != targetReverbWet) {
    currentReverbWet += (targetReverbWet - currentReverbWet) * kReverbRampCoeff;
    if (fabsf(currentReverbWet - targetReverbWet) < 0.005f) {
      currentReverbWet = targetReverbWet;
    }

    AudioNoInterrupts();
    mixerOutL.gain(1, currentReverbWet);
    mixerOutR.gain(1, currentReverbWet);
    AudioInterrupts();
  }

  // Waveform scope: drain stereo queues and feed display; display throttles redraws.
  while (queueLeft.available() && queueRight.available()) {
    const int16_t *blockL = queueLeft.readBuffer();
    const int16_t *blockR = queueRight.readBuffer();
    display.feedScopeBlockStereo(blockL, blockR, 128);
    queueLeft.freeBuffer();
    queueRight.freeBuffer();
  }

  display.drawScopeIfDue();

#if DEBUG
  Serial.print("Processor: ");
  Serial.println(AudioProcessorUsage());
  Serial.print(AudioProcessorUsageMax());
  Serial.println("(max)");
  Serial.print("Memory: ");
  Serial.println(AudioMemoryUsage());
  Serial.print(AudioMemoryUsageMax());
  Serial.println("(max)");
  Serial.println("    ");
#endif
}