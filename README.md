# Pill Reminder

A native medication reminder app for the **Pebble Time 2**.

Pill Reminder keeps medication reminders simple and watch-first: when a medication is due, the watch shows the configured pills or injection pen, alerts you with sound and/or vibration, and waits for a deliberate confirmation before marking it as taken.

<!--
Add screenshots before release, for example:

<p align="center">
  <img src="docs/screenshots/pills-dark.png" width="31%" alt="Pill reminder in dark mode">
  <img src="docs/screenshots/pen.png" width="31%" alt="Injection pen reminder">
  <img src="docs/screenshots/medication-list.png" width="31%" alt="Medication list">
</p>
-->

## Highlights

- Up to **8 medications**
- Pill and **injection pen** medication types
- Daily, weekly and monthly schedules
- Four configurable dayparts: morning, noon, evening and night
- Configurable pill shape, color, size and imprint
- Configurable pen body and accent colors
- Animated pills with accelerometer-driven physics
- Deliberate **hold-to-confirm** interaction
- Touch and button scrolling
- Repeating reminders
- Alarm sound on/off with configurable volume
- Vibration on/off
- **German and English** interface
- **Light, Dark and Shake** themes
- In Shake mode, a wrist shake switches between Light and Dark
- Subtle Seigaiha-inspired background pattern on the reminder screen
- Persistent reminder state and Pebble wakeups

## How it works

When a medication becomes due, Pill Reminder opens a dedicated reminder screen.

Pills behave like small physical objects and react to movement of the watch. Injection medications are shown as a large animated pen. If pills and a pen are due at the same time, they are presented as separate confirmation groups.

A medication is not marked as taken by a simple accidental tap. Confirmation requires a deliberate press-and-hold action with visual feedback.

After all due medication has been confirmed, the app shows the completed state and the full medication list remains available by scrolling.

## Configuration

The companion configuration page lets you manage medications directly from your phone.

For each medication you can configure:

- Name
- Effect / description
- Dosage
- Quantity
- Daypart
- Daily, weekly or monthly schedule
- Pill or injection pen
- Appearance
- Active / inactive state

General settings include:

- Morning, noon, evening and night start times
- Alarm sound
- Alarm volume
- Vibration
- Reminder interval
- Language
- Theme

Settings are stored locally and transferred to the watch through Pebble AppMessage.

## Themes

Pill Reminder includes three theme modes:

**Dark**  
Black interface with light text.

**Light**  
Light interface with dark text.

**Shake**  
Uses the same Light and Dark themes, but lets you switch between them with a wrist shake while the app is open.

The reminder screen uses a subtle Seigaiha-inspired wave pattern designed for the Pebble Time 2 display.

## Languages

The user interface is available in:

- English
- German

Medication names, dosage and description are user-provided and are not translated automatically.

## Supported device

Pill Reminder currently targets:

- **Pebble Time 2 (`emery`)**

The project uses the Pebble SDK 3 application format.

## Build from source

Requirements:

- Pebble SDK with Emery / Pebble Time 2 support
- Node.js as required by the Pebble SDK

Build:

```bash
pebble clean
pebble build
```

The resulting app bundle is written to:

```text
build/Pill-Reminder.pbw
```

Install on a connected watch:

```bash
pebble install --phone WATCH_IP
```

## Project structure

```text
package.json
resources/
src/
├── c/
│   ├── main.c
│   ├── app_state.c
│   ├── app_util.c
│   ├── watch_settings.c
│   ├── medication_model.c
│   ├── medication_alarm.c
│   ├── medication_ui.c
│   ├── pill_physics.c
│   ├── pill_renderer.c
│   ├── scroll_controller.c
│   └── confirmation_ui.c
└── js/
    └── pebble-js-app.js
```

The watch application is split into separate modules for UI, medication state, alarms, rendering, physics, scrolling, confirmation and settings transfer.

## Settings transfer

Phone-side settings are sent to the watch as a reset/item/commit transaction.

When adding new settings, keep the numeric AppMessage keys synchronized between:

- `package.json`
- watch-side C code
- `src/js/pebble-js-app.js`

## Medical disclaimer

Pill Reminder is a convenience tool and is **not a medical device**.

Do not rely on this app as the only safeguard for medication that must be taken at an exact time or where a missed or incorrect dose could cause harm. Always follow the instructions given by your doctor, pharmacist or medication provider.

## License

Pill Reminder is free software released under the **GNU General Public License v3.0**.

See [`LICENSE`](LICENSE) for the full license text.
