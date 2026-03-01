#include "display.h"
#include "version.h"
#include <Wire.h>
#include <cstdio>

namespace Lethe {

Display::Display()
    : _display(kWidth, kHeight, &Wire, -1, 200000), _scopeBufCount(0),
      _lastScopeUpdate(0) {}

void Display::begin() {
  _display.begin(kI2CAddr, true);
  _display.setRotation(2); // 180° so the display is right-side up

  _display.clearDisplay();
  _display.setTextColor(SH110X_WHITE);

  // Splash screen: title at size 2, version at size 1 (smaller)
  const char *title = "Lethe";
  int16_t x1, y1;
  uint16_t tw, th;
  _display.setTextSize(2);
  _display.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  char versionBuf[16];
  (void)snprintf(versionBuf, sizeof(versionBuf), "v%u.%u.%u",
                 (unsigned)FW_VERSION_MAJOR, (unsigned)FW_VERSION_MINOR, (unsigned)FW_VERSION_PATCH);
  uint16_t vw, vh;
  _display.setTextSize(1);
  _display.getTextBounds(versionBuf, 0, 0, &x1, &y1, &vw, &vh);

  const int gap = 6;
  const int blockH = (int)th + gap + (int)vh;
  int16_t blockY = (_display.height() - blockH) / 2 - y1;

  // Title centered (size 2)
  _display.setTextSize(2);
  int16_t cxTitle = (_display.width() - (int16_t)tw) / 2 - x1;
  _display.setCursor(cxTitle, blockY);
  _display.print(title);

  // Version centered below the name (size 1, smaller)
  _display.setTextSize(1);
  int16_t cxVer = (_display.width() - (int16_t)vw) / 2 - x1;
  _display.setCursor(cxVer, blockY + (int)th + gap);
  _display.print(versionBuf);
  _display.display();
}

void Display::feedScopeBlockStereo(const int16_t *blockLeft, const int16_t *blockRight, size_t len) {
  if (blockLeft == nullptr || blockRight == nullptr || len == 0) {
    return;
  }
  const size_t n = (len <= 128) ? len : 128u;

  // When buffer is already full and both channels silent, show flat line immediately.
  if (_scopeBufCount >= kScopeSamples) {
    int16_t maxAbsL = 0, maxAbsR = 0;
    for (size_t i = 0; i < n; i++) {
      int16_t sL = blockLeft[i], sR = blockRight[i];
      if (sL < 0) sL = -sL;
      if (sR < 0) sR = -sR;
      if (sL > maxAbsL) maxAbsL = sL;
      if (sR > maxAbsR) maxAbsR = sR;
    }
    if (maxAbsL <= kScopeSilentThreshold && maxAbsR <= kScopeSilentThreshold) {
      memset(_scopeBufferLeft, 0, sizeof(_scopeBufferLeft));
      memset(_scopeBufferRight, 0, sizeof(_scopeBufferRight));
      _scopeBufCount = kScopeSamples;
      return;
    }
  }

  if (_scopeBufCount + (int)n <= kScopeSamples) {
    memcpy(_scopeBufferLeft + _scopeBufCount, blockLeft, n * sizeof(int16_t));
    memcpy(_scopeBufferRight + _scopeBufCount, blockRight, n * sizeof(int16_t));
    _scopeBufCount += (int)n;
  } else {
    memmove(_scopeBufferLeft, _scopeBufferLeft + (int)n,
            (kScopeSamples - (int)n) * sizeof(int16_t));
    memmove(_scopeBufferRight, _scopeBufferRight + (int)n,
            (kScopeSamples - (int)n) * sizeof(int16_t));
    memcpy(_scopeBufferLeft + kScopeSamples - (int)n, blockLeft, n * sizeof(int16_t));
    memcpy(_scopeBufferRight + kScopeSamples - (int)n, blockRight, n * sizeof(int16_t));
  }
}

void Display::drawScopeIfDue() {
  if (_scopeBufCount < kScopeSamples) {
    const unsigned long now = millis();
    if (now > kScopeSplashTimeoutMs) {
      memset(_scopeBufferLeft, 0, sizeof(_scopeBufferLeft));
      memset(_scopeBufferRight, 0, sizeof(_scopeBufferRight));
      _scopeBufCount = kScopeSamples;
    } else {
      return;
    }
  }
  const unsigned long now = millis();
  if (now - _lastScopeUpdate < kScopeUpdateMs) {
    return;
  }
  _lastScopeUpdate = now;
  drawScope();
}

void Display::drawScope() {
  // Both channels centered on the same line, drawn one over the other.
  const int centerY = kHeight / 2;

  _display.clearDisplay();
  for (int x = 0; x < kWidth - 1; x++) {
    int i0 = x * kScopeDecimate;
    int i1 = (x + 1) * kScopeDecimate;

    int y0L = centerY - ((int)_scopeBufferLeft[i0] * kScopeFullScalePx >> 15);
    int y1L = centerY - ((int)_scopeBufferLeft[i1] * kScopeFullScalePx >> 15);
    y0L = (y0L < 0) ? 0 : (y0L > kHeight - 1) ? kHeight - 1 : y0L;
    y1L = (y1L < 0) ? 0 : (y1L > kHeight - 1) ? kHeight - 1 : y1L;
    _display.drawLine(x, y0L, x + 1, y1L, SH110X_WHITE);

    int y0R = centerY - ((int)_scopeBufferRight[i0] * kScopeFullScalePx >> 15);
    int y1R = centerY - ((int)_scopeBufferRight[i1] * kScopeFullScalePx >> 15);
    y0R = (y0R < 0) ? 0 : (y0R > kHeight - 1) ? kHeight - 1 : y0R;
    y1R = (y1R < 0) ? 0 : (y1R > kHeight - 1) ? kHeight - 1 : y1R;
    _display.drawLine(x, y0R, x + 1, y1R, SH110X_WHITE);
  }
  _display.display();
}

} // namespace Lethe
