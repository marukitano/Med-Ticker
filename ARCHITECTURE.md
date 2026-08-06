# Watch-side architecture

## Goal

The watch application is split into ordinary, separately compiled C modules.
`main.c` is only responsible for lifecycle order.

## Modules

- **watch_settings** — AppMessage parsing, transaction validation and persistent
  medication, daypart and appearance data.
- **medication_model** — current daypart, medication groups, confirmation state
  and the rows shown by the UI.
- **medication_alarm** — Pebble wakeups, due-window calculation, repeating
  reminders, speaker streaming and vibration.
- **pill_physics** — accelerometer sampling, rigid-body movement, wall contacts
  and pill-to-pill collision solving.
- **pill_renderer** — medication icons, pill geometry, colors, imprints and the
  medication list drawing helpers.
- **scroll_controller** — button and touch scrolling, snapping, edge bounce and
  the animated selection band.
- **confirmation_ui** — hold-to-confirm animation and settings-transfer screen.
- **medication_ui** — Pebble window/layer creation and high-level screen state.
- **app_util** — small helpers shared by multiple modules.

## Shared types and compatibility state

`app_types.h` contains constants and data structures used by more than one
module. `app_state.c/.h` temporarily holds mutable state that was previously
`static` in the old `main.c`.

`app_state.h` is an internal header: module consumers should use the focused
module headers instead. The next architecture pass can move state from
`app_state` into its owning module without changing lifecycle wiring.

## Lifecycle

The startup order is:

1. `watch_settings_init()`
2. `medication_alarm_init()`
3. `pill_physics_init()`
4. `medication_ui_init()`
5. `app_event_loop()`

Shutdown stops physics and alarms, tears down the UI, and finally unregisters
AppMessage callbacks.

## Phone-side code

`src/js/pebble-js-app.js` was not modified by this refactor.
