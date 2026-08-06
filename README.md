# Pill Reminder

Pill Reminder is a native medication reminder app for the **Pebble Time 2**
(`emery`). It combines scheduled reminders with a deliberate hold-to-confirm
interaction, so an accidental button press does not mark medication as taken.

> **Status:** active pre-release development. Do not use this app as the only
> safeguard for medication that must be taken at an exact time.

## Current features

- Up to eight configurable medications
- Daily, weekly and monthly schedules
- Four configurable dayparts: morning, noon, evening and night
- Phone-side configuration stored locally and transferred with AppMessage
- Pill and insulin-pen medication types
- Round, elliptical, tablet, rhombus and capsule pill shapes
- Configurable colors, size and tablet imprint
- Touch and button scrolling on Pebble Time 2
- Hold-to-confirm interaction with animated visual feedback
- Persistent reminder windows and Pebble wakeups
- Repeating reminders with configurable interval
- Optional vibration and PCM alarm audio with configurable volume
- Animated pills driven by the watch accelerometer
- Light and dark themes

## Repository layout

```text
package.json                       Pebble manifest and AppMessage keys
resources/                         Runtime images and alarm audio
src/c/main.c                       Application lifecycle wiring
src/c/watch_settings.c             AppMessage settings and persistence
src/c/medication_model.c           Dayparts, groups and visible medication rows
src/c/medication_alarm.c           Wakeups, reminder timing, audio and vibration
src/c/pill_physics.c               Accelerometer movement and collisions
src/c/pill_renderer.c              Medication and pill rendering
src/c/scroll_controller.c          Button/touch scrolling and band animation
src/c/confirmation_ui.c            Confirmation and transfer animations
src/c/medication_ui.c              Window and screen orchestration
src/c/app_state.c                  Internal compatibility state
src/js/pebble-js-app.js            Phone configuration and settings transfer
wscript                            Pebble SDK build definition
```

Each watch-side `.c` file is compiled as its own translation unit. There are no
implementation `.inc` files and no source-file inclusion tricks.

## Build

The project targets the Pebble SDK 3 toolchain and the `emery` platform.

```bash
pebble clean
pebble build
```

Install on a connected watch:

```bash
pebble install --phone WATCH_IP
```

The generated bundle is written to:

```text
build/Pill-Reminder.pbw
```

## Development notes

Settings are sent as a reset/item/commit transaction. Keep the numeric
AppMessage keys in `package.json`, `src/c/watch_settings.c`, and
`src/js/pebble-js-app.js` synchronized when adding fields.


## Watch-side architecture

`main.c` only starts and stops the modules around `app_event_loop()`.

The phone-side configuration remains unchanged in `src/js/pebble-js-app.js`.
On the watch, settings and medication data enter through `watch_settings`, then
flow to the medication model, alarm, physics and UI modules through declared C
interfaces.

`app_state.c` is an internal compatibility layer for state that was formerly
file-local in the monolithic implementation. It is intentionally not included
from any public module header. Future cleanup can move those fields behind the
owning module APIs one group at a time without making `main.c` large again.
