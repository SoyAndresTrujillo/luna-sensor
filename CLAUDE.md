# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32 bed-exit detector for Luna, a senior dog with dementia. FSR sensor under
her bed cushion → ESP32 → Telegram alert to whoever is on duty when she gets
up, so they can help her before she gets disoriented. Personal hardware
project, no build system beyond the Arduino IDE and a plain g++ unit test.

## Commands

Run the logic self-test (no Arduino/hardware needed):

```
g++ -std=c++11 selftest.cpp -o selftest && ./selftest
```

Flashing the firmware is done through the Arduino IDE (ESP32 board support via
Boards Manager, no external libraries) — there is no CLI build for
`luna_sensor.ino`. See README.md for full flashing steps.

There is no linter, no CI, no package manager in this repo.

## Architecture

Two-layer split so the detection logic is testable off-device:

- `luna_logic.h` — pure C++, zero Arduino dependencies. `luna_update()` is a
  symmetric-debounce state machine: an FSR reading only flips `in_bed` after
  it disagrees with the current state for `debounce_ms` (default 5s)
  continuously. This is what makes the logic unit-testable on a PC and what
  rejects brief pressure spikes (dog resettling) as false triggers.
- `luna_sensor.ino` — Arduino/ESP32 glue: samples the FSR (GPIO34, averaged
  over 8 reads every 100ms), calls into `luna_logic.h`, and on `EV_LEFT` /
  `EV_RETURNED` sends Telegram messages to every entry in `CHAT_IDS` (a plain
  array — multi-recipient fan-out, not a single chat). Also handles WiFi
  reconnect, buzzer beeps, and polls Telegram `getUpdates` for a `cal`
  command to recalibrate the baseline in place.
- `selftest.cpp` — asserts against `luna_logic.h` directly (sustained
  readings, debounce timing, spike rejection, `millis()` wraparound). This is
  the only test in the repo; extend it in place rather than adding a test
  framework.

Threshold model: `baseline` is captured at boot (or via the `cal` Telegram
command) by averaging 30 readings with Luna in bed. "Out of bed" = reading
falls below `baseline * OUT_FACTOR` (default 0.5).

Notification design: the ESP32 always messages *all* `CHAT_IDS`; who is
"on duty" is handled entirely on the phone side (muting the chat), not in
firmware — don't add server-side shift logic unless asked, it's explicitly
out of scope (see README's "Fase 2" section for deferred ideas).

## Known simplifications (marked `ponytail:` in code)

- `sendTelegramTo()` uses `WiFiClientSecure::setInsecure()` — no TLS cert
  validation. Acceptable because it only talks to Telegram's own API with a
  bot token; upgrade path is `setCACert()` if ever needed.
- `luna_update()`'s threshold is fixed at calibration time; FSR resistance
  drifts with temperature/age. Mitigated by the manual `cal` command, not by
  auto-recalibration.

## Editing config

Wifi/Telegram credentials and tunables (`OUT_FACTOR`, `DEBOUNCE_MS`,
`REPEAT_MS`, `CMD_POLL_MS`) live as `const`/`#define` at the top of
`luna_sensor.ino` — there is no separate config file or secrets store.
