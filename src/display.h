#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

namespace Lethe {

/**
 * OLED display (SH1106 128×64 I2C) with stereo waveform scope.
 *
 * Owns display init, splash screen, and left/right scope ring buffers +
 * throttled scope drawing. Both channels are centered on the same line
 * and drawn one over the other. Call begin() in setup(); feed stereo
 * blocks from the audio queues in loop() then call drawScopeIfDue().
 */
class Display {
public:
  static constexpr uint8_t kWidth = 128;
  static constexpr uint8_t kHeight = 64;
  static constexpr uint8_t kI2CAddr = 0x3C;   // try 0x3D if display does not respond
  static constexpr unsigned long kScopeUpdateMs = 50;
  /** If scope buffer not full after this many ms, draw flat line once to leave splash. */
  static constexpr unsigned long kScopeSplashTimeoutMs = 1000;

  Display();

  /** Init I2C display and show splash. */
  void begin();

  /** Push one 128-sample block per channel into the scope ring buffers (stereo). */
  void feedScopeBlockStereo(const int16_t *blockLeft, const int16_t *blockRight, size_t len = 128);

  /** If buffer is full and throttle elapsed, draw scope and return. */
  void drawScopeIfDue();

private:
  static constexpr int kScopeFullScalePx = 56;
  static constexpr int kScopeSamples = 2048;
  static constexpr int kScopeDecimate = kScopeSamples / kWidth;
  /** Max sample magnitude to treat block as silent and show flat line. */
  static constexpr int16_t kScopeSilentThreshold = 128;

  Adafruit_SH1106G _display;
  int16_t _scopeBufferLeft[kScopeSamples];
  int16_t _scopeBufferRight[kScopeSamples];
  int _scopeBufCount;
  unsigned long _lastScopeUpdate;

  void drawScope();
};

} // namespace Lethe
