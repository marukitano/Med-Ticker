# Pill Reminder

A medication reminder app for the **Pebble Time 2**.

I am building this because forgetting medication is easy — especially during
busy or irregular days. Pill Reminder is designed to make dismissing a reminder
quick, while making an accidental “taken” confirmation difficult.

> **Current status:** Early UI prototype.
>
> Scheduling, wakeups and phone-side configuration are not implemented yet.

## Current prototype

- Animated rotating pill
- Scrollable medication list
- Touch scrolling on Pebble Time 2
- Up/down button scrolling
- Deliberate hold-to-confirm interaction
- Green confirmation animation with a bouncing checkmark
- Short vibration at the checkmark impact
- Releasing the middle button too early deflates the green circle
- Back exits without confirming the medication as taken

The medication names currently shown are placeholder data.

## Interaction

### Confirm medication as taken

Hold the middle button.

A green circle expands from the button. When it fills the display, a white
checkmark appears and bounces toward the viewer. The confirmation is completed
at that moment.

Keep holding to view the result. Releasing the middle button afterwards closes
the app.

### Cancel the confirmation

Release the middle button before the checkmark appears.

The green circle shrinks back down and the app remains open.

### View the medication list

Scroll using either the touchscreen or the upper and lower buttons.

### Leave without confirming

Press the back button.

In the finished app, leaving without confirmation will keep the repeating
reminder active.

## Planned functionality

- Medication configuration through the Pebble phone app
- Daily, weekly and monthly schedules
- Multiple reminders per day
- Automatic repeat reminder every 15 minutes
- Repeat reminder cancelled only after deliberate confirmation
- Correct handling of short months for reminders on days 29, 30 and 31
- Persistent reminder state across app exits and watch restarts

## Development

Target platform:

```text
emery
```

Build:

```bash
pebble clean
pebble build
```

Install on a connected watch:

```bash
pebble install --phone WATCH_IP
```

The generated app bundle is located at:

```text
build/Pill-Reminder.pbw
```

## Project state

This project is under active development. The current code focuses on getting
the interaction and visual feedback right before adding the reminder engine.
