# Drone & Drama

Three sine oscillators plus pink noise through a state-variable filter and bitcrusher. Built for **Teensy 4.0** with a **PCM5102** DAC on **I2S2**. Optional OLED display (Adafruit SH110x + GFX; I2C).

## Requirements

- **Board:** Teensy 4.0
- **Audio:** PCM5102 DAC on I2S2 (Teensy’s second I2S port; level via mixer gains, no codec control)
- **Optional:** SH1106 128×64 I2C OLED for scope display

### Wiring

- **PCM5102 (I2S2):** Teensy → DAC: **2** = DIN, **3** = LRCLK (LRC/WS), **4** = BCLK; **33** = MCLK (optional on some breakouts). GND and 3.3 V in common.
- **SH1106 (I2C):** Teensy **18** = SDA, **19** = SCL; 3.3 V and GND. External 2.2–4.7 kΩ pull-ups to 3.3 V on SDA/SCL are recommended if the display does not respond.

## Build and upload

```bash
pio run -e teensy40
pio run -e teensy40 -t upload
```

Serial monitor (e.g. filter frequency debug):

```bash
pio device monitor -b 9600
```

## Configuration

- **Display:** SH1106 shows the **live output waveform** (mixer → I2S) as a scope trace. I2C address is set in `src/display.h` as `Display::kI2CAddr` (default **0x3C**; try **0x3D** if the display stays off).
- **Knobs:** A0–A2 → waveform frequencies; A3 → filter frequency (exp-scaled); A6–A9 → amplitudes (0–1).

### If the screen or DAC does not work (3.3 V OK)

1. **Display:** Confirm SDA=18, SCL=19, 3.3 V and GND. Add 2.2–4.7 kΩ pull-ups on SDA and SCL to 3.3 V if missing. Try `DISPLAY_I2C_ADDR` **0x3D** in `src/main.cpp` if 0x3C fails. This project uses the **Adafruit SH110x** library (with slower I2C clock) for better Teensy 4.0 I2C compatibility; if U8g2 did not work for you, the Adafruit stack may resolve the issue.
2. **DAC:** Confirm Teensy pins 2 (DIN), 3 (LRCLK), 4 (BCLK) to PCM5102; GND and 3.3 V in common. Some PCM5102 boards need MCLK on pin 33; others work from BCLK only.

## Project layout

- `platformio.ini` — Teensy 4.0 env, Adafruit SH110x + GFX (display).
- `src/main.cpp` — Audio graph, knobs; delegates display and scope to `Display`.
- `src/display.h`, `src/display.cpp` — Display class: SH1106 init, splash, scope ring buffer and throttled scope drawing.
- Legacy sketch: `D_D_teensy_v6_bc.ino` (reference only; firmware is in `src/main.cpp`).
