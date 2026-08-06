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
package.json                 Pebble manifest and AppMessage keys
resources/                   Runtime images and alarm audio
src/c/main.c                 Watch application
src/js/pebble-js-app.js      Phone configuration and settings transfer
wscript                      Pebble SDK build definition
```

The watch application is currently still a large single translation unit. The
first cleanup pass deliberately removes dead code without splitting that file,
so behaviour can be verified before architecture changes are introduced.

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
AppMessage keys in `package.json`, `src/c/main.c`, and
`src/js/pebble-js-app.js` synchronized when adding fields.
