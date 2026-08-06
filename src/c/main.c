#include <pebble.h>

#include "message_keys.auto.h"

#include <stdio.h>
#include <string.h>

#define FRAME_COUNT 8
#define UI_TICK_MS 110
#define PILL_TICKS_PER_FRAME 2

#define CANVAS_START_OFFSET_Y 0

#define SCROLL_Q8 256
#define SCROLL_FRAME_MS 16

#define SCROLL_BREAKAWAY_PX 9
#define SCROLL_QUICK_SWIPE_MIN_PX 5
#define SCROLL_QUICK_SWIPE_MAX_MS 230

#define SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8 12
#define SCROLL_EDGE_HALF_INTERVAL_PX \
  ((MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP) / 2)

#define SCROLL_FINGER_SPRING_NUM 18
#define SCROLL_FINGER_SPRING_DEN 100
#define SCROLL_FINGER_DAMPING_NUM 68
#define SCROLL_FINGER_DAMPING_DEN 100

/*
 * Diese eine Feder definiert sämtliche automatischen
 * Fahrten und Bounces: Taste, Quick-Swipe, normaler
 * Swipe und die virtuellen Tabellenränder.
 */
#define SCROLL_SNAP_SPRING_NUM 24
#define SCROLL_SNAP_SPRING_DEN 100
#define SCROLL_SNAP_DAMPING_NUM 62
#define SCROLL_SNAP_DAMPING_DEN 100

#define SCROLL_SNAP_REFERENCE_PX \
  (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP)

#define SCROLL_BREAKAWAY_FORCE_Q8 \
  ( \
    SCROLL_BREAKAWAY_PX * \
    SCROLL_Q8 * \
    SCROLL_FINGER_SPRING_NUM / \
    SCROLL_FINGER_SPRING_DEN \
  )

#define SCROLL_SNAP_MAX_FORCE_Q8 \
  ( \
    SCROLL_SNAP_REFERENCE_PX * \
    SCROLL_Q8 * \
    SCROLL_SNAP_SPRING_NUM / \
    SCROLL_SNAP_SPRING_DEN \
  )

#define SCROLL_MAX_VELOCITY_Q8 \
  (32 * SCROLL_Q8)

#define SCROLL_STOP_POSITION_Q8 \
  (SCROLL_Q8 / 4)

#define SCROLL_STOP_VELOCITY_Q8 \
  (SCROLL_Q8 / 4)

#define CONFIRM_ANIMATION_INTERVAL_MS 30
#define CONFIRM_GROW_STEP 5
#define CONFIRM_SHRINK_MIN_STEP 4
#define CONFIRM_SHRINK_DIVISOR 7
#define CONFIRM_CENTER_OUTSIDE_X 8

#define CHECK_STROKE_RADIUS 8
#define CHECK_POP_GROW_STEP 44
#define CHECK_POP_SHRINK_STEP 30
#define CHECK_POP_SETTLE_SIZE 80
#define CHECK_POP_OVERSHOOT_SIZE 140

#define HINT_HALF_WIDTH 9
#define HINT_HEIGHT 10
#define HINT_POSITION_ADJUST_Y -18

#define MEDICATION_GAP 54
#define MEDICATION_HEADER_HEIGHT 28
#define MEDICATION_ROW_HEIGHT 50
#define MEDICATION_ROW_GAP 8
#define MEDICATION_ICON_SIZE 30
#define MEDICATION_ICON_LEFT 10
#define MEDICATION_ICON_TEXT_X 46
#define MEDICATION_ICON_TEXT_RIGHT 8
#define BAND_OVERSHOOT_COVER_PX 32
#define BAND_ARROW_WIDTH 18
#define TAKEN_HINT_MIN_RADIUS 5

#define THEME_PERSIST_KEY 200
#define LEGACY_MEDICATION_PERSIST_KEY 201
#define MEDICATION_LIST_PERSIST_KEY 202
#define MEDICATION_COUNT_PERSIST_KEY 203
#define SETTINGS_MESSAGE_BUFFER_SIZE 320
#define MEDICATION_NAME_LENGTH 32
#define MEDICATION_LABEL_LENGTH 48
#define MAX_MEDICATIONS 8
#define MAX_LIST_ROWS (MAX_MEDICATIONS + 2)

#define DAYPART_PERSIST_KEY 204
#define ALARM_AUDIO_VOLUME_PERSIST_KEY 205
#define ALARM_VIBRATION_PERSIST_KEY 206
#define ALARM_INTERVAL_PERSIST_KEY 207
#define ALARM_WINDOW_STATE_PERSIST_KEY 208
#define DAYPART_MINUTES_PER_DAY 1440
#define LEGACY_DEFAULT_MORNING_START_MINUTE (5 * 60)
#define LEGACY_DEFAULT_NOON_START_MINUTE (11 * 60)
#define LEGACY_DEFAULT_EVENING_START_MINUTE (16 * 60)
#define LEGACY_DEFAULT_NIGHT_START_MINUTE (21 * 60)
#define DEFAULT_MORNING_START_MINUTE (6 * 60)
#define DEFAULT_NOON_START_MINUTE (12 * 60)
#define DEFAULT_EVENING_START_MINUTE (18 * 60)
#define DEFAULT_NIGHT_START_MINUTE (22 * 60)

#define DEFAULT_ALARM_AUDIO_VOLUME 100
#define DEFAULT_ALARM_VIBRATION_ENABLED true
#define DEFAULT_ALARM_REMINDER_INTERVAL_MINUTES 15
#define ALARM_ACTIVE_SECONDS 60
#define ALARM_VIBE_INTERVAL_MS 2000
#define ALARM_AUDIO_BUFFER_SIZE 1024
#define ALARM_AUDIO_PUMP_INTERVAL_MS 10
#define ALARM_AUDIO_MAX_WRITES_PER_PUMP 8
#define ALARM_WAKEUP_COOKIE 0x50494c4c
#define TRANSFER_CLOSE_DELAY_MS 5000
#define TRANSFER_ANIMATION_INTERVAL_MS 30
#define TRANSFER_MORPH_DURATION_MS 450
#define TRANSFER_SHAFT_DURATION_MS 650
#define TRANSFER_FALL_DURATION_MS 500
#define TRANSFER_PROGRESS_MAX 1000

#define PILL_PHYSICS_MAX_BODIES 12
#define PILL_PHYSICS_SCROLL_PAUSE_MS 80
#define PILL_PHYSICS_Q8 256
#define PILL_PHYSICS_EDGE_MARGIN 4
#define PILL_PHYSICS_ANGLE_BUCKET (TRIG_MAX_ANGLE / 32)
/* Capsule rigid-body physics. Positions and linear velocities use Q8. */
#define PILL_RB_FRAME_MS 40
#define PILL_RB_ALARM_FRAME_MS 40
#define PILL_RB_ACCEL_DIVISOR 1
#define PILL_RB_MAX_LINEAR_Q8 (52 * PILL_PHYSICS_Q8)
#define PILL_RB_MAX_ANGULAR (TRIG_MAX_ANGLE / 12)
#define PILL_RB_ANGULAR_DAMPING_NUM 253
#define PILL_RB_ANGULAR_DAMPING_DEN 256
#define PILL_RB_RESTITUTION_NUM 1
#define PILL_RB_RESTITUTION_DEN 5
#define PILL_RB_RESTITUTION_SPEED_Q8 (2 * PILL_PHYSICS_Q8)
#define PILL_RB_FLAT_CONTACT_TOLERANCE_Q8 (4 * PILL_PHYSICS_Q8)
#define PILL_RB_FRICTION_NUM 3
#define PILL_RB_FRICTION_DEN 5
#define PILL_RB_POSITION_SLOP_Q8 (PILL_PHYSICS_Q8 / 8)
#define PILL_RB_SOLVER_ITERATIONS 4
#define PILL_RB_PARAMETER_Q12 4096
#define PILL_RB_ANGLE_TO_LINEAR_NUM 201
#define PILL_RB_ANGLE_TO_LINEAR_DEN 8192
#define PILL_RB_RAD_TO_ANGLE_NUM 10430
#define PILL_RB_SLEEP_LINEAR_Q8 (PILL_PHYSICS_Q8)
#define PILL_RB_SLEEP_ANGULAR (TRIG_MAX_ANGLE / 240)
#define PILL_RB_SLEEP_FRAMES 5
#define PILL_RB_SENSOR_WAKE_MG 35
#define PILL_RB_TILT_DEADZONE_MG 50
#define PILL_RB_TILT_WAKE_HYSTERESIS_MG 10
#define PILL_RB_REST_TRAVEL_Q8 (PILL_PHYSICS_Q8)
#define PILL_RB_REST_ANGLE (TRIG_MAX_ANGLE / 180)

typedef enum {
  MEDICATION_TIME_MORNING,
  MEDICATION_TIME_NOON,
  MEDICATION_TIME_EVENING,
  MEDICATION_TIME_NIGHT
} MedicationTime;

typedef enum {
  MEDICATION_SCHEDULE_DAILY,
  MEDICATION_SCHEDULE_WEEKLY,
  MEDICATION_SCHEDULE_MONTHLY
} MedicationSchedule;

typedef enum {
  MEDICATION_SYMBOL_PILL,
  MEDICATION_SYMBOL_PEN
} MedicationSymbol;

#define LEGACY_MEDICATION_SYMBOL_TUBE 2

typedef enum {
  MEDICATION_ROW_ITEM,
  MEDICATION_ROW_CONFIRM_PILLS,
  MEDICATION_ROW_CONFIRM_PEN
} MedicationRowKind;

typedef struct {
  char name[MEDICATION_NAME_LENGTH];
  uint8_t quantity;
  uint8_t time;
  uint8_t schedule;
  uint8_t day;
  uint8_t symbol;
  uint8_t enabled;
} LegacyMedicationSettingsV1;

typedef struct {
  char name[MEDICATION_NAME_LENGTH];
  uint8_t quantity;
  uint8_t time;
  uint8_t schedule;
  uint8_t day;
  uint8_t symbol;
  uint8_t shape;
  uint8_t color;
  uint8_t icon_set;
  uint8_t enabled;
} MedicationSettings;

typedef struct {
  uint16_t morning;
  uint16_t noon;
  uint16_t evening;
  uint16_t night;
} DaypartSettings;

typedef struct {
  int32_t window_start;
  int32_t last_reminder;
  uint8_t confirmed_mask;
  uint8_t reserved[3];
} AlarmWindowState;

typedef enum {
  CONFIRM_IDLE,
  CONFIRM_GROWING,
  CONFIRM_SHRINKING,
  CONFIRM_COMPLETE
} ConfirmationState;

typedef enum {
  CHECK_HIDDEN,
  CHECK_POPPING_OUT,
  CHECK_AT_PEAK,
  CHECK_SETTLING,
  CHECK_VISIBLE
} CheckState;

static const int8_t s_hint_offsets[] = {
  0, 1, 3, 5, 3, 1, 0, 0
};

static const uint32_t s_impact_vibration_durations[] = {
  50
};

static const VibePattern s_impact_vibration_pattern = {
  .durations = s_impact_vibration_durations,
  .num_segments = ARRAY_LENGTH(s_impact_vibration_durations)
};

static const MedicationSettings s_default_medication = {
  .name = "Xarelto 20 mg",
  .quantity = 1,
  .time = MEDICATION_TIME_MORNING,
  .schedule = MEDICATION_SCHEDULE_DAILY,
  .day = 0,
  .symbol = MEDICATION_SYMBOL_PILL,
  .shape = 2,
  .color = 255,
  .icon_set = 1,
  .enabled = 1
};

static const DaypartSettings s_default_dayparts = {
  .morning = DEFAULT_MORNING_START_MINUTE,
  .noon = DEFAULT_NOON_START_MINUTE,
  .evening = DEFAULT_EVENING_START_MINUTE,
  .night = DEFAULT_NIGHT_START_MINUTE
};

static DaypartSettings s_dayparts;

static MedicationSettings s_medications[
  MAX_MEDICATIONS
];
static uint8_t s_medication_count;

static MedicationSettings s_pending_medications[
  MAX_MEDICATIONS
];
static uint8_t s_pending_count;
static uint16_t s_pending_received_mask;

static char s_row_labels[
  MAX_LIST_ROWS
][MEDICATION_LABEL_LENGTH];
static const char *s_rows[MAX_LIST_ROWS];
static MedicationRowKind s_row_kinds[
  MAX_LIST_ROWS
];
static int8_t s_row_medication_indices[
  MAX_LIST_ROWS
];
static uint8_t s_list_row_count = 1;
static MedicationTime s_visible_medication_time;
static bool s_visible_medication_time_set;
static bool s_pills_confirmed;
static bool s_pen_confirmed;

#define LIST_ROW_COUNT ((int)s_list_row_count)

static Window *s_window;
static Layer *s_canvas_layer;
static Layer *s_band_layer;
static Layer *s_band_arrow_layer;
static Layer *s_confirmation_layer;

typedef struct {
  int32_t x_q8;
  int32_t velocity_q8;
  int32_t target_x_q8;
  bool target_visible;
  bool animating;
} BandAnimationState;

static BandAnimationState s_band;

static GBitmap *s_sheet;
static GBitmap *s_frames[FRAME_COUNT];

static AppTimer *s_ui_timer;
static AppTimer *s_confirmation_timer;
static AppTimer *s_scroll_physics_timer;
static AppTimer *s_band_animation_timer;

static GFont s_medication_font;
static GFont s_header_font;

static int16_t s_frame_width;
static int16_t s_frame_height;
static uint8_t s_animation_tick;
static int8_t s_taken_hint_phase;
static bool s_light_theme;
static bool s_confirmed_screen_active;

typedef enum {
  TRANSFER_ANIMATION_IDLE,
  TRANSFER_ANIMATION_MORPHING,
  TRANSFER_ANIMATION_SHAFT_DROP,
  TRANSFER_ANIMATION_READY,
  TRANSFER_ANIMATION_FALLING
} TransferAnimationState;

static bool s_transfer_screen_active;
static AppTimer *s_transfer_close_timer;
static AppTimer *s_transfer_animation_timer;
static TransferAnimationState s_transfer_animation_state;
static uint16_t s_transfer_animation_elapsed_ms;
static int16_t s_transfer_fall_offset;

typedef struct {
  int32_t x_q8;
  int32_t y_q8;
  int32_t vx_q8;
  int32_t vy_q8;
  int32_t angle;
  int32_t angular_velocity;
  uint8_t medication_index;
  uint8_t collision_radius;
  uint8_t collision_half_length;
} PillPhysicsBody;

static PillPhysicsBody s_pill_physics_bodies[
  PILL_PHYSICS_MAX_BODIES
];
static uint8_t s_pill_physics_body_count;
static AppTimer *s_pill_physics_timer;
static bool s_pill_physics_accel_subscribed;
static bool s_pill_physics_window_visible;
static int16_t s_pill_physics_gravity_x;
static int16_t s_pill_physics_gravity_y;
static int16_t s_pill_physics_last_target_x;
static int16_t s_pill_physics_last_target_y;
static uint8_t s_pill_physics_quiet_frames;
static uint8_t s_pill_physics_collision_phase;
static uint8_t s_pill_physics_sensor_quiet_samples;

static uint8_t s_alarm_audio_volume =
    DEFAULT_ALARM_AUDIO_VOLUME;
static bool s_alarm_vibration_enabled =
    DEFAULT_ALARM_VIBRATION_ENABLED;
static uint8_t s_alarm_reminder_interval_minutes =
    DEFAULT_ALARM_REMINDER_INTERVAL_MINUTES;
static AlarmWindowState s_alarm_window_state;
static bool s_alarm_window_state_loaded;
static bool s_alarm_active;
static bool s_alarm_launch_pending;
static time_t s_alarm_stop_time;
static uint8_t s_alarm_due_symbol_mask;
static AppTimer *s_alarm_pulse_timer;
static AppTimer *s_alarm_audio_pump_timer;
static ResHandle s_alarm_audio_resource;
static size_t s_alarm_audio_resource_size;
static size_t s_alarm_audio_resource_offset;
static uint8_t s_alarm_audio_buffer[
  ALARM_AUDIO_BUFFER_SIZE
];
static size_t s_alarm_audio_buffer_size;
static size_t s_alarm_audio_buffer_offset;
static bool s_alarm_audio_active;

typedef enum {
  SCROLL_IDLE,
  SCROLL_TOUCH,
  SCROLL_SNAP,
  SCROLL_EDGE_BOUNCE
} ScrollMode;

typedef struct {
  int32_t position_q8;
  int32_t velocity_q8;
  int32_t target_q8;
  int32_t breakaway_anchor_q8;

  int8_t snap_index;

  ScrollMode mode;
  bool breakaway_locked;
} ScrollState;

static ScrollState s_scroll;

#if defined(PBL_TOUCH)
typedef struct {
  int16_t last_y;
  int16_t total_delta_y;
  uint32_t start_time_ms;

  int8_t start_index;
  int8_t neighbor_index;
  int8_t pair_direction;

  bool dragging;
  bool pair_selected;
  bool edge_consumed;
} ScrollTouchState;

static ScrollTouchState s_touch;
#endif

static int16_t s_confirm_radius;
static int16_t s_confirm_max_radius;
static ConfirmationState s_confirmation_state;
static MedicationSymbol s_confirmation_symbol;
static bool s_confirmation_symbol_set;

static int16_t s_check_size;
static CheckState s_check_state;

static GColor theme_background_color(void) {
  return s_light_theme
      ? GColorWhite
      : GColorBlack;
}

static GColor theme_foreground_color(void) {
  return s_light_theme
      ? GColorBlack
      : GColorWhite;
}

static GColor theme_hint_color(void) {
  return s_light_theme
      ? GColorDarkGray
      : GColorLightGray;
}

static void update_band_animation_target(void);
static void reset_ui_state(GRect bounds);
static void refresh_medication_rows_for_time(void);
static void alarm_audio_stop(void);
static bool alarm_audio_start(void);
static void alarm_stop(void);
static void schedule_next_alarm_wakeup(void);
static void alarm_handle_minute_tick(
    const struct tm *tick_time
);
static void alarm_confirmation_received(
    MedicationSymbol symbol
);
static void alarm_reset_after_settings_save(void);

static void alarm_refresh_window_state(void);
static void refresh_app_screen_state(void);
static void show_transfer_screen(void);
static void start_transfer_animation(void);
static void schedule_transfer_close(void);
static void pill_physics_rebuild(void);
static void pill_physics_update_activity(void);
static void pill_physics_stop(void);

static void mark_scene_dirty(void) {
  update_band_animation_target();

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  if (s_band_layer && !layer_get_hidden(s_band_layer)) {
    layer_mark_dirty(s_band_layer);
  }

  if (s_band_arrow_layer && !layer_get_hidden(s_band_arrow_layer)) {
    layer_mark_dirty(s_band_arrow_layer);
  }
}

static void apply_theme(
    bool light_theme,
    bool save
) {
  s_light_theme = light_theme;

  if (save) {
    persist_write_int(
      THEME_PERSIST_KEY,
      light_theme ? 1 : 0
    );
  }

  if (s_window) {
    window_set_background_color(
      s_window,
      theme_background_color()
    );
  }

  mark_scene_dirty();

  if (s_confirmation_layer) {
    layer_mark_dirty(
      s_confirmation_layer
    );
  }
}

static bool alarm_reminder_interval_valid(int value) {
  switch (value) {
    case 1:
    case 5:
    case 10:
    case 15:
    case 20:
    case 30:
    case 60:
      return true;

    default:
      return false;
  }
}

static void load_alarm_settings(void) {
  s_alarm_audio_volume = DEFAULT_ALARM_AUDIO_VOLUME;
  s_alarm_vibration_enabled =
      DEFAULT_ALARM_VIBRATION_ENABLED;
  s_alarm_reminder_interval_minutes =
      DEFAULT_ALARM_REMINDER_INTERVAL_MINUTES;

  if (persist_exists(ALARM_AUDIO_VOLUME_PERSIST_KEY)) {
    const int stored_volume = persist_read_int(
      ALARM_AUDIO_VOLUME_PERSIST_KEY
    );

    if (stored_volume >= 0 && stored_volume <= 100) {
      s_alarm_audio_volume = (uint8_t)stored_volume;
    }
  }

  if (persist_exists(ALARM_VIBRATION_PERSIST_KEY)) {
    s_alarm_vibration_enabled =
        persist_read_int(ALARM_VIBRATION_PERSIST_KEY) != 0;
  }

  if (persist_exists(ALARM_INTERVAL_PERSIST_KEY)) {
    const int stored_interval = persist_read_int(
      ALARM_INTERVAL_PERSIST_KEY
    );

    if (alarm_reminder_interval_valid(stored_interval)) {
      s_alarm_reminder_interval_minutes =
          (uint8_t)stored_interval;
    }
  }
}

static void apply_alarm_settings(
    uint8_t volume,
    bool vibration_enabled,
    uint8_t reminder_interval_minutes,
    bool save
) {
  if (!alarm_reminder_interval_valid(reminder_interval_minutes)) {
    reminder_interval_minutes =
        DEFAULT_ALARM_REMINDER_INTERVAL_MINUTES;
  }

  s_alarm_audio_volume = volume;
  s_alarm_vibration_enabled = vibration_enabled;
  s_alarm_reminder_interval_minutes =
      reminder_interval_minutes;

  if (save) {
    persist_write_int(
      ALARM_AUDIO_VOLUME_PERSIST_KEY,
      s_alarm_audio_volume
    );
    persist_write_int(
      ALARM_VIBRATION_PERSIST_KEY,
      s_alarm_vibration_enabled ? 1 : 0
    );
    persist_write_int(
      ALARM_INTERVAL_PERSIST_KEY,
      s_alarm_reminder_interval_minutes
    );
  }

  if (!s_alarm_vibration_enabled) {
    vibes_cancel();
  }

  if (s_alarm_active) {
    if (
      s_alarm_audio_volume == 0 ||
      speaker_is_muted()
    ) {
      alarm_audio_stop();
    } else if (s_alarm_audio_active) {
      speaker_set_volume(s_alarm_audio_volume);
    } else {
      (void)alarm_audio_start();
    }
  }
}

typedef enum {
  SETTINGS_COMMAND_RESET,
  SETTINGS_COMMAND_ITEM,
  SETTINGS_COMMAND_COMMIT
} SettingsCommand;

static bool medication_settings_valid(
    const MedicationSettings *settings
) {
  if (
    !settings ||
    settings->name[0] == '\0' ||
    settings->name[
      MEDICATION_NAME_LENGTH - 1
    ] != '\0' ||
    settings->quantity < 1 ||
    settings->quantity > 20 ||
    settings->time > MEDICATION_TIME_NIGHT ||
    settings->schedule >
        MEDICATION_SCHEDULE_MONTHLY ||
    settings->symbol >
        MEDICATION_SYMBOL_PEN ||
    settings->shape > 3 ||
    settings->color < 192 ||
    settings->icon_set > 1 ||
    settings->enabled > 1 ||
    (settings->enabled && !settings->icon_set)
  ) {
    return false;
  }

  if (
    settings->schedule ==
        MEDICATION_SCHEDULE_DAILY
  ) {
    return settings->day == 0;
  }

  if (
    settings->schedule ==
        MEDICATION_SCHEDULE_WEEKLY
  ) {
    return settings->day <= 6;
  }

  return
      settings->day >= 1 &&
      settings->day <= 31;
}

static MedicationSettings medication_from_legacy(
    const LegacyMedicationSettingsV1 *legacy
) {
  MedicationSettings medication =
      s_default_medication;

  if (!legacy) {
    return medication;
  }

  memcpy(
    medication.name,
    legacy->name,
    sizeof(medication.name)
  );

  medication.quantity = legacy->quantity;
  medication.time = legacy->time;
  medication.schedule = legacy->schedule;
  medication.day = legacy->day;
  medication.symbol = legacy->symbol;
  medication.enabled = legacy->enabled;
  medication.shape = 2;
  medication.color = 255;
  medication.icon_set = 1;

  return medication;
}

static bool migrate_legacy_medication_symbol(
    MedicationSettings *settings
) {
  if (
    settings &&
    settings->symbol ==
        LEGACY_MEDICATION_SYMBOL_TUBE
  ) {
    settings->symbol =
        MEDICATION_SYMBOL_PILL;
    return true;
  }

  return false;
}

static void format_medication_label(
    const MedicationSettings *medication,
    char *label,
    size_t label_size
) {
  if (medication->quantity > 1) {
    snprintf(
      label,
      label_size,
      "%s x%u",
      medication->name,
      (unsigned int)medication->quantity
    );
    return;
  }

  snprintf(
    label,
    label_size,
    "%s",
    medication->name
  );
}

static bool daypart_settings_valid(
    const DaypartSettings *settings
) {
  return
      settings &&
      settings->morning < settings->noon &&
      settings->noon < settings->evening &&
      settings->evening < settings->night &&
      settings->night < DAYPART_MINUTES_PER_DAY;
}

static void load_daypart_settings(void) {
  s_dayparts = s_default_dayparts;

  if (
    !persist_exists(DAYPART_PERSIST_KEY) ||
    persist_get_size(DAYPART_PERSIST_KEY) !=
        (int)sizeof(DaypartSettings)
  ) {
    return;
  }

  DaypartSettings stored;

  if (
    persist_read_data(
      DAYPART_PERSIST_KEY,
      &stored,
      sizeof(stored)
    ) == (int)sizeof(stored) &&
    daypart_settings_valid(&stored)
  ) {
    const bool legacy_defaults =
        stored.morning ==
            LEGACY_DEFAULT_MORNING_START_MINUTE &&
        stored.noon ==
            LEGACY_DEFAULT_NOON_START_MINUTE &&
        stored.evening ==
            LEGACY_DEFAULT_EVENING_START_MINUTE &&
        stored.night ==
            LEGACY_DEFAULT_NIGHT_START_MINUTE;

    if (legacy_defaults) {
      persist_write_data(
        DAYPART_PERSIST_KEY,
        &s_dayparts,
        sizeof(s_dayparts)
      );
    } else {
      s_dayparts = stored;
    }
  }
}

static void apply_daypart_settings(
    const DaypartSettings *settings,
    bool save
) {
  if (!daypart_settings_valid(settings)) {
    return;
  }

  s_dayparts = *settings;

  if (save) {
    persist_write_data(
      DAYPART_PERSIST_KEY,
      &s_dayparts,
      sizeof(s_dayparts)
    );
  }

  s_visible_medication_time_set = false;
  refresh_medication_rows_for_time();
}

static MedicationTime medication_time_for_minute(
    int minute
) {
  if (
    minute >= s_dayparts.morning &&
    minute < s_dayparts.noon
  ) {
    return MEDICATION_TIME_MORNING;
  }

  if (
    minute >= s_dayparts.noon &&
    minute < s_dayparts.evening
  ) {
    return MEDICATION_TIME_NOON;
  }

  if (
    minute >= s_dayparts.evening &&
    minute < s_dayparts.night
  ) {
    return MEDICATION_TIME_EVENING;
  }

  return MEDICATION_TIME_NIGHT;
}

static MedicationTime current_medication_time(void) {
  const time_t now = time(NULL);
  struct tm *local_time = localtime(&now);

  if (!local_time) {
    return MEDICATION_TIME_MORNING;
  }

  return medication_time_for_minute(
    local_time->tm_hour * 60 +
    local_time->tm_min
  );
}

static void reset_medication_confirmations(void) {
  alarm_refresh_window_state();
  s_pills_confirmed =
      (s_alarm_window_state.confirmed_mask &
       (1u << MEDICATION_SYMBOL_PILL)) != 0;
  s_pen_confirmed =
      (s_alarm_window_state.confirmed_mask &
       (1u << MEDICATION_SYMBOL_PEN)) != 0;
}

static bool medication_group_is_confirmed(
    MedicationSymbol symbol
) {
  return
      symbol == MEDICATION_SYMBOL_PILL
          ? s_pills_confirmed
          : s_pen_confirmed;
}

static void mark_medication_group_confirmed(
    MedicationSymbol symbol
) {
  if (symbol == MEDICATION_SYMBOL_PILL) {
    s_pills_confirmed = true;
    return;
  }

  s_pen_confirmed = true;
}

static bool medication_matches_group(
    const MedicationSettings *medication,
    MedicationTime visible_time,
    MedicationSymbol symbol
) {
  return
      medication &&
      medication->enabled &&
      medication->time ==
          (uint8_t)visible_time &&
      medication->symbol ==
          (uint8_t)symbol;
}

static bool medication_group_is_due(
    MedicationSymbol symbol
) {
  if (medication_group_is_confirmed(symbol)) {
    return false;
  }

  const MedicationTime visible_time =
      current_medication_time();

  for (
    uint8_t index = 0;
    index < s_medication_count;
    index++
  ) {
    if (
      medication_matches_group(
        &s_medications[index],
        visible_time,
        symbol
      )
    ) {
      return true;
    }
  }

  return false;
}

static bool unconfirmed_medication_group_is_due(void) {
  return
      medication_group_is_due(
        MEDICATION_SYMBOL_PILL
      ) ||
      medication_group_is_due(
        MEDICATION_SYMBOL_PEN
      );
}

static void append_confirmation_row(
    MedicationSymbol symbol
) {
  const char *prompt =
      symbol == MEDICATION_SYMBOL_PILL
          ? "Tabletten genommen?"
          : "Pen injiziert?";

  snprintf(
    s_row_labels[s_list_row_count],
    sizeof(s_row_labels[s_list_row_count]),
    "%s",
    prompt
  );

  s_rows[s_list_row_count] =
      s_row_labels[s_list_row_count];

  s_row_kinds[s_list_row_count] =
      symbol == MEDICATION_SYMBOL_PILL
          ? MEDICATION_ROW_CONFIRM_PILLS
          : MEDICATION_ROW_CONFIRM_PEN;

  s_row_medication_indices[s_list_row_count] = -1;

  s_list_row_count++;
}

static void rebuild_medication_rows(void) {
  const MedicationTime visible_time =
      current_medication_time();

  s_visible_medication_time = visible_time;
  s_visible_medication_time_set = true;
  s_list_row_count = 0;

  memset(
    s_row_medication_indices,
    -1,
    sizeof(s_row_medication_indices)
  );

  for (
    uint8_t group_index = 0;
    group_index < 2;
    group_index++
  ) {
    const MedicationSymbol symbol =
        (MedicationSymbol)group_index;

    if (medication_group_is_confirmed(symbol)) {
      continue;
    }

    bool group_has_medication = false;

    for (
      uint8_t index = 0;
      index < s_medication_count;
      index++
    ) {
      if (
        !medication_matches_group(
          &s_medications[index],
          visible_time,
          symbol
        )
      ) {
        continue;
      }

      format_medication_label(
        &s_medications[index],
        s_row_labels[s_list_row_count],
        sizeof(s_row_labels[s_list_row_count])
      );

      s_rows[s_list_row_count] =
          s_row_labels[s_list_row_count];

      s_row_kinds[s_list_row_count] =
          MEDICATION_ROW_ITEM;

      s_row_medication_indices[s_list_row_count] =
          (int8_t)index;

      s_list_row_count++;
      group_has_medication = true;
    }

    if (group_has_medication) {
      append_confirmation_row(symbol);
    }
  }
}

static void refresh_medication_rows_for_time(void) {
  const MedicationTime visible_time =
      current_medication_time();

  if (
    s_visible_medication_time_set &&
    visible_time == s_visible_medication_time
  ) {
    return;
  }

  refresh_app_screen_state();
}

static void daypart_tick_handler(
    struct tm *tick_time,
    TimeUnits units_changed
) {
  (void)units_changed;

  refresh_medication_rows_for_time();
  alarm_handle_minute_tick(tick_time);
}

static bool medication_list_valid(
    const MedicationSettings *medications,
    uint8_t count
) {
  if (count > MAX_MEDICATIONS) {
    return false;
  }

  for (
    uint8_t index = 0;
    index < count;
    index++
  ) {
    if (
      !medication_settings_valid(
        &medications[index]
      )
    ) {
      return false;
    }
  }

  return true;
}

static void persist_medication_list(void) {
  persist_write_int(
    MEDICATION_COUNT_PERSIST_KEY,
    s_medication_count
  );

  if (s_medication_count == 0) {
    persist_delete(
      MEDICATION_LIST_PERSIST_KEY
    );
    return;
  }

  persist_write_data(
    MEDICATION_LIST_PERSIST_KEY,
    s_medications,
    sizeof(MedicationSettings) *
        s_medication_count
  );
}

static void apply_medication_list(
    const MedicationSettings *medications,
    uint8_t count,
    bool save
) {
  if (
    !medication_list_valid(
      medications,
      count
    )
  ) {
    return;
  }

  memset(
    s_medications,
    0,
    sizeof(s_medications)
  );

  if (count > 0) {
    memcpy(
      s_medications,
      medications,
      sizeof(MedicationSettings) * count
    );
  }

  s_medication_count = count;

  if (save) {
    persist_medication_list();
  }

  refresh_app_screen_state();
}

static bool load_current_medication_list(void) {
  if (
    !persist_exists(
      MEDICATION_COUNT_PERSIST_KEY
    )
  ) {
    return false;
  }

  const int stored_count =
      persist_read_int(
        MEDICATION_COUNT_PERSIST_KEY
      );

  if (
    stored_count < 0 ||
    stored_count > MAX_MEDICATIONS
  ) {
    return false;
  }

  if (stored_count == 0) {
    s_medication_count = 0;
    rebuild_medication_rows();
    return true;
  }

  if (
    !persist_exists(
      MEDICATION_LIST_PERSIST_KEY
    )
  ) {
    return false;
  }

  const int stored_size =
      persist_get_size(
        MEDICATION_LIST_PERSIST_KEY
      );

  const int current_size =
      (int)(
        sizeof(MedicationSettings) *
        stored_count
      );

  const int legacy_size =
      (int)(
        sizeof(LegacyMedicationSettingsV1) *
        stored_count
      );

  MedicationSettings stored[
    MAX_MEDICATIONS
  ];

  memset(stored, 0, sizeof(stored));

  bool needs_persist = false;

  if (stored_size == current_size) {
    if (
      persist_read_data(
        MEDICATION_LIST_PERSIST_KEY,
        stored,
        current_size
      ) != current_size
    ) {
      return false;
    }
  } else if (stored_size == legacy_size) {
    LegacyMedicationSettingsV1 legacy[
      MAX_MEDICATIONS
    ];

    memset(legacy, 0, sizeof(legacy));

    if (
      persist_read_data(
        MEDICATION_LIST_PERSIST_KEY,
        legacy,
        legacy_size
      ) != legacy_size
    ) {
      return false;
    }

    for (
      uint8_t index = 0;
      index < (uint8_t)stored_count;
      index++
    ) {
      stored[index] =
          medication_from_legacy(
            &legacy[index]
          );
    }

    needs_persist = true;
  } else {
    return false;
  }

  for (
    uint8_t index = 0;
    index < (uint8_t)stored_count;
    index++
  ) {
    if (
      migrate_legacy_medication_symbol(
        &stored[index]
      )
    ) {
      needs_persist = true;
    }
  }

  if (
    !medication_list_valid(
      stored,
      (uint8_t)stored_count
    )
  ) {
    return false;
  }

  memcpy(
    s_medications,
    stored,
    sizeof(MedicationSettings) *
        stored_count
  );

  s_medication_count =
      (uint8_t)stored_count;

  reset_medication_confirmations();
  rebuild_medication_rows();

  if (needs_persist) {
    persist_medication_list();
  }

  return true;
}

static void load_medication_settings(void) {
  memset(
    s_medications,
    0,
    sizeof(s_medications)
  );

  if (load_current_medication_list()) {
    return;
  }

  MedicationSettings migrated =
      s_default_medication;

  if (
    persist_exists(
      LEGACY_MEDICATION_PERSIST_KEY
    )
  ) {
    const int legacy_size =
        persist_get_size(
          LEGACY_MEDICATION_PERSIST_KEY
        );

    MedicationSettings stored =
        s_default_medication;

    bool read_successfully = false;

    if (
      legacy_size ==
          (int)sizeof(MedicationSettings)
    ) {
      read_successfully =
          persist_read_data(
            LEGACY_MEDICATION_PERSIST_KEY,
            &stored,
            sizeof(stored)
          ) == (int)sizeof(stored);
    } else if (
      legacy_size ==
          (int)sizeof(LegacyMedicationSettingsV1)
    ) {
      LegacyMedicationSettingsV1 legacy;

      if (
        persist_read_data(
          LEGACY_MEDICATION_PERSIST_KEY,
          &legacy,
          sizeof(legacy)
        ) == (int)sizeof(legacy)
      ) {
        stored = medication_from_legacy(&legacy);
        read_successfully = true;
      }
    }

    if (read_successfully) {
      migrate_legacy_medication_symbol(
        &stored
      );

      if (medication_settings_valid(&stored)) {
        migrated = stored;
      }
    }
  }

  s_medications[0] = migrated;
  s_medication_count = 1;
  rebuild_medication_rows();
  persist_medication_list();
}

static bool tuple_read_int32(
    Tuple *tuple,
    int32_t *value
) {
  if (
    !tuple ||
    !value ||
    (
      tuple->type != TUPLE_INT &&
      tuple->type != TUPLE_UINT
    )
  ) {
    return false;
  }

  *value = tuple->value->int32;
  return true;
}

static bool read_dayparts_from_message(
    DictionaryIterator *iterator,
    DaypartSettings *settings
) {
  Tuple *morning_tuple = dict_find(
    iterator,
    MESSAGE_KEY_DAYPART_MORNING
  );

  Tuple *noon_tuple = dict_find(
    iterator,
    MESSAGE_KEY_DAYPART_NOON
  );

  Tuple *evening_tuple = dict_find(
    iterator,
    MESSAGE_KEY_DAYPART_EVENING
  );

  Tuple *night_tuple = dict_find(
    iterator,
    MESSAGE_KEY_DAYPART_NIGHT
  );

  int32_t morning;
  int32_t noon;
  int32_t evening;
  int32_t night;

  if (
    !tuple_read_int32(
      morning_tuple,
      &morning
    ) ||
    !tuple_read_int32(
      noon_tuple,
      &noon
    ) ||
    !tuple_read_int32(
      evening_tuple,
      &evening
    ) ||
    !tuple_read_int32(
      night_tuple,
      &night
    ) ||
    morning < 0 ||
    noon < 0 ||
    evening < 0 ||
    night < 0 ||
    morning >= DAYPART_MINUTES_PER_DAY ||
    noon >= DAYPART_MINUTES_PER_DAY ||
    evening >= DAYPART_MINUTES_PER_DAY ||
    night >= DAYPART_MINUTES_PER_DAY
  ) {
    return false;
  }

  const DaypartSettings parsed = {
    .morning = (uint16_t)morning,
    .noon = (uint16_t)noon,
    .evening = (uint16_t)evening,
    .night = (uint16_t)night
  };

  if (!daypart_settings_valid(&parsed)) {
    return false;
  }

  *settings = parsed;
  return true;
}

static bool read_medication_from_message(
    DictionaryIterator *iterator,
    MedicationSettings *settings
) {
  Tuple *name_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_NAME
  );

  Tuple *quantity_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_QUANTITY
  );

  Tuple *time_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_TIME
  );

  Tuple *schedule_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_SCHEDULE
  );

  Tuple *day_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_DAY
  );

  Tuple *symbol_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_SYMBOL
  );

  Tuple *shape_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_SHAPE
  );

  Tuple *color_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_COLOR
  );

  Tuple *icon_set_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_ICON_SET
  );

  Tuple *enabled_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_ENABLED
  );

  if (
    !name_tuple ||
    name_tuple->type != TUPLE_CSTRING
  ) {
    return false;
  }

  const char *name =
      name_tuple->value->cstring;
  const size_t name_length = strlen(name);

  int32_t quantity;
  int32_t time;
  int32_t schedule;
  int32_t day;
  int32_t symbol;
  int32_t shape;
  int32_t color;
  int32_t icon_set;
  int32_t enabled;

  if (
    name_length == 0 ||
    name_length >= MEDICATION_NAME_LENGTH ||
    !tuple_read_int32(
      quantity_tuple,
      &quantity
    ) ||
    !tuple_read_int32(
      time_tuple,
      &time
    ) ||
    !tuple_read_int32(
      schedule_tuple,
      &schedule
    ) ||
    !tuple_read_int32(
      day_tuple,
      &day
    ) ||
    !tuple_read_int32(
      symbol_tuple,
      &symbol
    ) ||
    !tuple_read_int32(
      shape_tuple,
      &shape
    ) ||
    !tuple_read_int32(
      color_tuple,
      &color
    ) ||
    !tuple_read_int32(
      icon_set_tuple,
      &icon_set
    ) ||
    !tuple_read_int32(
      enabled_tuple,
      &enabled
    ) ||
    quantity < 1 ||
    quantity > 20 ||
    time < MEDICATION_TIME_MORNING ||
    time > MEDICATION_TIME_NIGHT ||
    schedule < MEDICATION_SCHEDULE_DAILY ||
    schedule > MEDICATION_SCHEDULE_MONTHLY ||
    symbol < MEDICATION_SYMBOL_PILL ||
    symbol > MEDICATION_SYMBOL_PEN ||
    shape < 0 ||
    shape > 4 ||
    color < 192 ||
    color > 255 ||
    icon_set < 0 ||
    icon_set > 1 ||
    enabled < 0 ||
    enabled > 1 ||
    (enabled && !icon_set)
  ) {
    return false;
  }

  if (
    (
      schedule == MEDICATION_SCHEDULE_DAILY &&
      day != 0
    ) ||
    (
      schedule == MEDICATION_SCHEDULE_WEEKLY &&
      (day < 0 || day > 6)
    ) ||
    (
      schedule == MEDICATION_SCHEDULE_MONTHLY &&
      (day < 1 || day > 31)
    )
  ) {
    return false;
  }

  MedicationSettings parsed = {
    .quantity = (uint8_t)quantity,
    .time = (uint8_t)time,
    .schedule = (uint8_t)schedule,
    .day = (uint8_t)day,
    .symbol = (uint8_t)symbol,
    .shape = (uint8_t)(shape <= 3 ? shape : 2),
    .color = (uint8_t)color,
    .icon_set = (uint8_t)icon_set,
    .enabled = (uint8_t)enabled
  };

  memcpy(
    parsed.name,
    name,
    name_length + 1
  );

  if (!medication_settings_valid(&parsed)) {
    return false;
  }

  *settings = parsed;
  return true;
}

static uint16_t expected_pending_mask(
    uint8_t count
) {
  if (count == 0) {
    return 0;
  }

  return
      (uint16_t)((1u << count) - 1u);
}

static void reset_pending_medications(
    uint8_t count
) {
  memset(
    s_pending_medications,
    0,
    sizeof(s_pending_medications)
  );

  s_pending_count = count;
  s_pending_received_mask = 0;
}

#define MEDICATION_APPEARANCE_IMPRINT_LENGTH 6
#define MEDICATION_APPEARANCE_COUNT_PERSIST_KEY 219
#define MEDICATION_APPEARANCE_PERSIST_KEY_BASE 220

typedef struct {
  bool valid;
  uint8_t shape;
  uint8_t primary_color;
  uint8_t secondary_color;
  uint8_t size;
  char imprint[MEDICATION_APPEARANCE_IMPRINT_LENGTH];
} MedicationAppearance;

static MedicationAppearance s_medication_appearances[MAX_MEDICATIONS];
static MedicationAppearance s_pending_medication_appearances[MAX_MEDICATIONS];
static uint8_t s_medication_appearance_count = 0;
static uint8_t s_pending_medication_appearance_count = 0;
static uint16_t s_pending_medication_appearance_mask = 0;

static int medication_appearance_persist_key(uint8_t index) {
  return MEDICATION_APPEARANCE_PERSIST_KEY_BASE + index;
}

static void reset_pending_medication_appearances(uint8_t count) {
  s_pending_medication_appearance_count = count;
  s_pending_medication_appearance_mask = 0;
  memset(
    s_pending_medication_appearances,
    0,
    sizeof(s_pending_medication_appearances)
  );
}

static bool read_medication_appearance_from_message(
    DictionaryIterator *iterator,
    MedicationAppearance *appearance
) {
  if (!iterator || !appearance) {
    return false;
  }

  Tuple *shape_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_SHAPE
  );
  Tuple *primary_color_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_COLOR
  );
  Tuple *secondary_color_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_COLOR2
  );
  Tuple *size_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_SIZE
  );
  Tuple *imprint_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_IMPRINT
  );

  int32_t shape;
  int32_t primary_color;
  int32_t secondary_color;
  int32_t size;

  if (
    !tuple_read_int32(shape_tuple, &shape) ||
    !tuple_read_int32(primary_color_tuple, &primary_color) ||
    !tuple_read_int32(secondary_color_tuple, &secondary_color) ||
    !tuple_read_int32(size_tuple, &size) ||
    !imprint_tuple ||
    imprint_tuple->type != TUPLE_CSTRING ||
    shape < 0 ||
    shape > 4 ||
    primary_color < 192 ||
    primary_color > 255 ||
    secondary_color < 192 ||
    secondary_color > 255 ||
    size < 60 ||
    size > 140
  ) {
    return false;
  }

  const char *imprint = imprint_tuple->value->cstring;
  const size_t imprint_length = strlen(imprint);

  if (imprint_length >= MEDICATION_APPEARANCE_IMPRINT_LENGTH) {
    return false;
  }

  for (size_t index = 0; index < imprint_length; index++) {
    const unsigned char character = (unsigned char)imprint[index];

    if (character < 32 || character > 126) {
      return false;
    }
  }

  MedicationAppearance parsed = {
    .valid = true,
    .shape = (uint8_t)shape,
    .primary_color = (uint8_t)primary_color,
    .secondary_color = (uint8_t)secondary_color,
    .size = (uint8_t)size,
    .imprint = { 0 }
  };

  memcpy(
    parsed.imprint,
    imprint,
    imprint_length + 1
  );

  *appearance = parsed;
  return true;
}

static void persist_medication_appearances(void) {
  persist_write_int(
    MEDICATION_APPEARANCE_COUNT_PERSIST_KEY,
    s_medication_appearance_count
  );

  for (uint8_t index = 0; index < MAX_MEDICATIONS; index++) {
    const int key = medication_appearance_persist_key(index);

    if (
      index < s_medication_appearance_count &&
      s_medication_appearances[index].valid
    ) {
      persist_write_data(
        key,
        &s_medication_appearances[index],
        sizeof(MedicationAppearance)
      );
    } else if (persist_exists(key)) {
      persist_delete(key);
    }
  }
}

static void load_medication_appearances(void) {
  memset(
    s_medication_appearances,
    0,
    sizeof(s_medication_appearances)
  );
  s_medication_appearance_count = 0;

  if (!persist_exists(MEDICATION_APPEARANCE_COUNT_PERSIST_KEY)) {
    return;
  }

  const int stored_count = persist_read_int(
    MEDICATION_APPEARANCE_COUNT_PERSIST_KEY
  );

  if (stored_count < 0 || stored_count > MAX_MEDICATIONS) {
    return;
  }

  s_medication_appearance_count = (uint8_t)stored_count;

  for (uint8_t index = 0; index < s_medication_appearance_count; index++) {
    const int key = medication_appearance_persist_key(index);

    if (
      !persist_exists(key) ||
      persist_get_size(key) != (int)sizeof(MedicationAppearance) ||
      persist_read_data(
        key,
        &s_medication_appearances[index],
        sizeof(MedicationAppearance)
      ) != (int)sizeof(MedicationAppearance) ||
      !s_medication_appearances[index].valid ||
      s_medication_appearances[index].shape > 4 ||
      s_medication_appearances[index].primary_color < 192 ||
      s_medication_appearances[index].secondary_color < 192 ||
      s_medication_appearances[index].size < 60 ||
      s_medication_appearances[index].size > 140 ||
      s_medication_appearances[index].imprint[
        MEDICATION_APPEARANCE_IMPRINT_LENGTH - 1
      ] != '\0'
    ) {
      memset(
        &s_medication_appearances[index],
        0,
        sizeof(MedicationAppearance)
      );
    }
  }
}

static void apply_medication_appearances(void) {
  memcpy(
    s_medication_appearances,
    s_pending_medication_appearances,
    sizeof(s_medication_appearances)
  );
  s_medication_appearance_count =
      s_pending_medication_appearance_count;
  persist_medication_appearances();
  pill_physics_rebuild();

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void settings_inbox_received(
    DictionaryIterator *iterator,
    void *context
) {
  (void)context;

  Tuple *command_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_COMMAND
  );

  int32_t command;

  if (
    !tuple_read_int32(
      command_tuple,
      &command
    )
  ) {
    return;
  }

  Tuple *count_tuple = dict_find(
    iterator,
    MESSAGE_KEY_MED_COUNT
  );

  int32_t count;

  if (
    !tuple_read_int32(
      count_tuple,
      &count
    ) ||
    count < 0 ||
    count > MAX_MEDICATIONS
  ) {
    return;
  }

  if (command == SETTINGS_COMMAND_RESET) {
    reset_pending_medications(
      (uint8_t)count
    );
    reset_pending_medication_appearances(
      (uint8_t)count
    );
    show_transfer_screen();
    return;
  }

  if (
    count != s_pending_count ||
    count != s_pending_medication_appearance_count
  ) {
    return;
  }

  if (command == SETTINGS_COMMAND_ITEM) {
    Tuple *index_tuple = dict_find(
      iterator,
      MESSAGE_KEY_MED_INDEX
    );

    int32_t index;

    if (
      !tuple_read_int32(
        index_tuple,
        &index
      ) ||
      index < 0 ||
      index >= count
    ) {
      return;
    }

    MedicationSettings medication;
    MedicationAppearance appearance;

    if (
      !read_medication_from_message(
        iterator,
        &medication
      ) ||
      !read_medication_appearance_from_message(
        iterator,
        &appearance
      )
    ) {
      APP_LOG(
        APP_LOG_LEVEL_WARNING,
        "Incomplete medication item %ld ignored",
        (long)index
      );
      return;
    }

    s_pending_medications[index] = medication;
    s_pending_medication_appearances[index] = appearance;

    s_pending_received_mask |=
        (uint16_t)(1u << index);
    s_pending_medication_appearance_mask |=
        (uint16_t)(1u << index);

    APP_LOG(
      APP_LOG_LEVEL_INFO,
      "Medication item %ld complete: shape=%u size=%u imprint=%s",
      (long)index,
      (unsigned int)appearance.shape,
      (unsigned int)appearance.size,
      appearance.imprint
    );
    return;
  }

  if (
    command != SETTINGS_COMMAND_COMMIT ||
    s_pending_received_mask !=
        expected_pending_mask(
          s_pending_count
        ) ||
    s_pending_medication_appearance_mask !=
        expected_pending_mask(
          s_pending_medication_appearance_count
        )
  ) {
    return;
  }

  Tuple *theme_tuple = dict_find(
    iterator,
    MESSAGE_KEY_THEME
  );
  Tuple *audio_volume_tuple = dict_find(
    iterator,
    MESSAGE_KEY_AUDIO_VOLUME
  );
  Tuple *vibration_tuple = dict_find(
    iterator,
    MESSAGE_KEY_VIBRATION_ENABLED
  );
  Tuple *reminder_interval_tuple = dict_find(
    iterator,
    MESSAGE_KEY_REMINDER_INTERVAL
  );

  int32_t theme_value;
  int32_t audio_volume;
  int32_t vibration_enabled;
  int32_t reminder_interval;
  DaypartSettings dayparts;

  if (
    !tuple_read_int32(
      theme_tuple,
      &theme_value
    ) ||
    (theme_value != 0 && theme_value != 1) ||
    !read_dayparts_from_message(
      iterator,
      &dayparts
    ) ||
    !tuple_read_int32(
      audio_volume_tuple,
      &audio_volume
    ) ||
    audio_volume < 0 ||
    audio_volume > 100 ||
    !tuple_read_int32(
      vibration_tuple,
      &vibration_enabled
    ) ||
    (vibration_enabled != 0 && vibration_enabled != 1) ||
    !tuple_read_int32(
      reminder_interval_tuple,
      &reminder_interval
    ) ||
    !alarm_reminder_interval_valid(
      reminder_interval
    )
  ) {
    APP_LOG(
      APP_LOG_LEVEL_WARNING,
      "Incomplete settings commit ignored"
    );
    return;
  }

  apply_theme(
    theme_value == 1,
    true
  );
  apply_daypart_settings(
    &dayparts,
    true
  );
  apply_alarm_settings(
    (uint8_t)audio_volume,
    vibration_enabled == 1,
    (uint8_t)reminder_interval,
    true
  );
  apply_medication_list(
    s_pending_medications,
    s_pending_count,
    true
  );
  apply_medication_appearances();

  alarm_reset_after_settings_save();
  reset_pending_medications(0);
  reset_pending_medication_appearances(0);

  APP_LOG(
    APP_LOG_LEVEL_INFO,
    "Single settings transaction committed completely"
  );
  schedule_transfer_close();
}

static void settings_init(void) {
  s_light_theme =
      persist_exists(THEME_PERSIST_KEY) &&
      persist_read_int(THEME_PERSIST_KEY) == 1;

  load_daypart_settings();
  load_alarm_settings();
  load_medication_settings();
  reset_pending_medications(0);
  reset_pending_medication_appearances(0);

  app_message_register_inbox_received(
    settings_inbox_received
  );

  const AppMessageResult result =
      app_message_open(
        SETTINGS_MESSAGE_BUFFER_SIZE,
        64
      );

  if (result != APP_MSG_OK) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "AppMessage open failed: %d",
      (int)result
    );
  }
}

static void settings_deinit(void) {
  app_message_deregister_callbacks();
}

static void cancel_timer(AppTimer **timer) {
  if (!*timer) {
    return;
  }

  app_timer_cancel(*timer);
  *timer = NULL;
}

/******************************************************************************
 * Medication alarm, speaker streaming and wakeups
 ******************************************************************************/

static void alarm_audio_reset_state(void) {
  s_alarm_audio_resource = NULL;
  s_alarm_audio_resource_size = 0;
  s_alarm_audio_resource_offset = 0;
  s_alarm_audio_buffer_size = 0;
  s_alarm_audio_buffer_offset = 0;
  s_alarm_audio_active = false;
}

static void alarm_audio_stop(void) {
  cancel_timer(&s_alarm_audio_pump_timer);

  if (s_alarm_audio_active) {
    speaker_stop();
  }

  alarm_audio_reset_state();
}

static bool alarm_audio_load_next_chunk(void) {
  if (
    s_alarm_audio_resource == NULL ||
    s_alarm_audio_resource_size == 0
  ) {
    return false;
  }

  if (
    s_alarm_audio_resource_offset >=
        s_alarm_audio_resource_size
  ) {
    s_alarm_audio_resource_offset = 0;
  }

  const size_t bytes_remaining =
      s_alarm_audio_resource_size -
      s_alarm_audio_resource_offset;

  const size_t bytes_requested =
      bytes_remaining < sizeof(s_alarm_audio_buffer)
          ? bytes_remaining
          : sizeof(s_alarm_audio_buffer);

  const size_t bytes_loaded =
      resource_load_byte_range(
        s_alarm_audio_resource,
        (uint32_t)s_alarm_audio_resource_offset,
        s_alarm_audio_buffer,
        bytes_requested
      );

  if (bytes_loaded == 0) {
    return false;
  }

  s_alarm_audio_buffer_size = bytes_loaded;
  s_alarm_audio_buffer_offset = 0;
  return true;
}

static void alarm_audio_pump(void *context);

static void alarm_audio_schedule_pump(void) {
  if (
    !s_alarm_audio_active ||
    s_alarm_audio_pump_timer != NULL
  ) {
    return;
  }

  s_alarm_audio_pump_timer = app_timer_register(
    ALARM_AUDIO_PUMP_INTERVAL_MS,
    alarm_audio_pump,
    NULL
  );

  if (!s_alarm_audio_pump_timer) {
    alarm_audio_stop();
  }
}

static void alarm_audio_pump(void *context) {
  (void)context;
  s_alarm_audio_pump_timer = NULL;

  if (!s_alarm_audio_active) {
    return;
  }

  for (
    uint8_t attempt = 0;
    attempt < ALARM_AUDIO_MAX_WRITES_PER_PUMP;
    attempt++
  ) {
    if (
      s_alarm_audio_buffer_offset >=
          s_alarm_audio_buffer_size
    ) {
      if (!alarm_audio_load_next_chunk()) {
        alarm_audio_stop();
        return;
      }
    }

    const size_t bytes_available =
        s_alarm_audio_buffer_size -
        s_alarm_audio_buffer_offset;

    const uint32_t bytes_written =
        speaker_stream_write(
          s_alarm_audio_buffer +
              s_alarm_audio_buffer_offset,
          (uint32_t)bytes_available
        );

    if (bytes_written == 0) {
      break;
    }

    s_alarm_audio_buffer_offset += bytes_written;

    if (
      s_alarm_audio_buffer_offset >=
          s_alarm_audio_buffer_size
    ) {
      s_alarm_audio_resource_offset +=
          s_alarm_audio_buffer_size;
      s_alarm_audio_buffer_size = 0;
      s_alarm_audio_buffer_offset = 0;

      if (
        s_alarm_audio_resource_offset >=
            s_alarm_audio_resource_size
      ) {
        s_alarm_audio_resource_offset = 0;
      }
    }
  }

  alarm_audio_schedule_pump();
}

static bool alarm_audio_start(void) {
  alarm_audio_stop();

  if (
    s_alarm_audio_volume == 0 ||
    speaker_is_muted()
  ) {
    return false;
  }

  s_alarm_audio_resource =
      resource_get_handle(RESOURCE_ID_ALARM_PCM);
  s_alarm_audio_resource_size =
      resource_size(s_alarm_audio_resource);

  if (
    s_alarm_audio_resource == NULL ||
    s_alarm_audio_resource_size == 0
  ) {
    alarm_audio_reset_state();
    return false;
  }

  if (
    !speaker_stream_open(
      SpeakerPcmFormat_16kHz_8bit,
      s_alarm_audio_volume
    )
  ) {
    alarm_audio_reset_state();
    return false;
  }

  s_alarm_audio_active = true;
  alarm_audio_pump(NULL);
  return s_alarm_audio_active;
}

static bool medication_due_on_date(
    const MedicationSettings *medication,
    const struct tm *local_date
) {
  if (!medication || !local_date || !medication->enabled) {
    return false;
  }

  if (
    medication->schedule ==
        MEDICATION_SCHEDULE_DAILY
  ) {
    return true;
  }

  if (
    medication->schedule ==
        MEDICATION_SCHEDULE_WEEKLY
  ) {
    const uint8_t monday_based_weekday =
        (uint8_t)((local_date->tm_wday + 6) % 7);

    return medication->day == monday_based_weekday;
  }

  if (
    medication->schedule ==
        MEDICATION_SCHEDULE_MONTHLY
  ) {
    return medication->day == local_date->tm_mday;
  }

  return false;
}

static bool alarm_window_bounds_at(
    time_t timestamp,
    time_t *window_start,
    time_t *window_end,
    MedicationTime *window_slot
) {
  struct tm *local_ptr = localtime(&timestamp);

  if (!local_ptr) {
    return false;
  }

  const struct tm local = *local_ptr;
  const int minute = local.tm_hour * 60 + local.tm_min;

  MedicationTime slot;
  int start_day_offset = 0;
  int end_day_offset = 0;
  uint16_t start_minute;
  uint16_t end_minute;

  if (
    minute >= s_dayparts.morning &&
    minute < s_dayparts.noon
  ) {
    slot = MEDICATION_TIME_MORNING;
    start_minute = s_dayparts.morning;
    end_minute = s_dayparts.noon;
  } else if (
    minute >= s_dayparts.noon &&
    minute < s_dayparts.evening
  ) {
    slot = MEDICATION_TIME_NOON;
    start_minute = s_dayparts.noon;
    end_minute = s_dayparts.evening;
  } else if (
    minute >= s_dayparts.evening &&
    minute < s_dayparts.night
  ) {
    slot = MEDICATION_TIME_EVENING;
    start_minute = s_dayparts.evening;
    end_minute = s_dayparts.night;
  } else if (minute >= s_dayparts.night) {
    slot = MEDICATION_TIME_NIGHT;
    start_minute = s_dayparts.night;
    end_minute = s_dayparts.morning;
    end_day_offset = 1;
  } else {
    slot = MEDICATION_TIME_NIGHT;
    start_minute = s_dayparts.night;
    end_minute = s_dayparts.morning;
    start_day_offset = -1;
  }

  struct tm start_tm = local;
  start_tm.tm_mday += start_day_offset;
  start_tm.tm_hour = start_minute / 60;
  start_tm.tm_min = start_minute % 60;
  start_tm.tm_sec = 0;
  start_tm.tm_isdst = -1;

  struct tm end_tm = local;
  end_tm.tm_mday += end_day_offset;
  end_tm.tm_hour = end_minute / 60;
  end_tm.tm_min = end_minute % 60;
  end_tm.tm_sec = 0;
  end_tm.tm_isdst = -1;

  const time_t start = mktime(&start_tm);
  const time_t end = mktime(&end_tm);

  if (start <= 0 || end <= start) {
    return false;
  }

  if (window_start) {
    *window_start = start;
  }
  if (window_end) {
    *window_end = end;
  }
  if (window_slot) {
    *window_slot = slot;
  }

  return true;
}

static void persist_alarm_window_state(void) {
  persist_write_data(
    ALARM_WINDOW_STATE_PERSIST_KEY,
    &s_alarm_window_state,
    sizeof(s_alarm_window_state)
  );
}

static void alarm_refresh_window_state(void) {
  time_t window_start;

  if (
    !alarm_window_bounds_at(
      time(NULL),
      &window_start,
      NULL,
      NULL
    )
  ) {
    return;
  }

  if (!s_alarm_window_state_loaded) {
    memset(
      &s_alarm_window_state,
      0,
      sizeof(s_alarm_window_state)
    );

    if (
      persist_exists(ALARM_WINDOW_STATE_PERSIST_KEY) &&
      persist_get_size(ALARM_WINDOW_STATE_PERSIST_KEY) ==
          (int)sizeof(AlarmWindowState)
    ) {
      (void)persist_read_data(
        ALARM_WINDOW_STATE_PERSIST_KEY,
        &s_alarm_window_state,
        sizeof(s_alarm_window_state)
      );
    }

    s_alarm_window_state_loaded = true;
  }

  if (
    s_alarm_window_state.window_start !=
        (int32_t)window_start
  ) {
    s_alarm_window_state = (AlarmWindowState) {
      .window_start = (int32_t)window_start,
      .last_reminder = 0,
      .confirmed_mask = 0
    };

    persist_alarm_window_state();
  }
}

static uint8_t alarm_symbol_mask_for_window(
    time_t window_start,
    MedicationTime slot
) {
  struct tm *local_ptr = localtime(&window_start);

  if (!local_ptr) {
    return 0;
  }

  const struct tm schedule_date = *local_ptr;
  uint8_t mask = 0;

  for (
    uint8_t index = 0;
    index < s_medication_count;
    index++
  ) {
    const MedicationSettings *medication =
        &s_medications[index];

    if (
      medication->time != (uint8_t)slot ||
      !medication_due_on_date(
        medication,
        &schedule_date
      )
    ) {
      continue;
    }

    mask |= (uint8_t)(1u << medication->symbol);
  }

  return mask;
}

static uint8_t alarm_unconfirmed_symbol_mask_at(
    time_t timestamp
) {
  time_t window_start;
  MedicationTime slot;

  if (
    !alarm_window_bounds_at(
      timestamp,
      &window_start,
      NULL,
      &slot
    )
  ) {
    return 0;
  }

  alarm_refresh_window_state();

  return
      alarm_symbol_mask_for_window(
        window_start,
        slot
      ) &
      (uint8_t)~s_alarm_window_state.confirmed_mask;
}

static time_t next_due_window_start_after(time_t now) {
  struct tm *today_ptr = localtime(&now);

  if (!today_ptr) {
    return 0;
  }

  const struct tm today = *today_ptr;
  const uint16_t starts[] = {
    s_dayparts.morning,
    s_dayparts.noon,
    s_dayparts.evening,
    s_dayparts.night
  };

  for (uint16_t day_offset = 0; day_offset <= 370; day_offset++) {
    for (uint8_t slot = 0; slot < ARRAY_LENGTH(starts); slot++) {
      struct tm candidate_tm = today;
      candidate_tm.tm_mday += day_offset;
      candidate_tm.tm_hour = starts[slot] / 60;
      candidate_tm.tm_min = starts[slot] % 60;
      candidate_tm.tm_sec = 0;
      candidate_tm.tm_isdst = -1;

      const time_t candidate = mktime(&candidate_tm);

      if (
        candidate <= now ||
        alarm_symbol_mask_for_window(
          candidate,
          (MedicationTime)slot
        ) == 0
      ) {
        continue;
      }

      return candidate;
    }
  }

  return 0;
}

static time_t next_alarm_timestamp_after(time_t now) {
  time_t window_start;
  time_t window_end;

  if (
    alarm_window_bounds_at(
      now,
      &window_start,
      &window_end,
      NULL
    ) &&
    alarm_unconfirmed_symbol_mask_at(now) != 0
  ) {
    time_t candidate;

    if (
      s_alarm_window_state.last_reminder <
          (int32_t)window_start
    ) {
      candidate = now + 60;
    } else {
      candidate =
          (time_t)s_alarm_window_state.last_reminder +
          (time_t)s_alarm_reminder_interval_minutes * 60;

      if (candidate <= now) {
        candidate = now + 60;
      }
    }

    if (candidate < window_end) {
      return candidate;
    }
  }

  return next_due_window_start_after(now);
}

static void schedule_next_alarm_wakeup(void) {
  wakeup_cancel_all();

  const time_t now = time(NULL);
  const time_t candidate =
      next_alarm_timestamp_after(now);

  if (candidate <= now) {
    return;
  }

  WakeupId result = E_INTERNAL;
  time_t scheduled = candidate;

  for (
    uint8_t attempt = 0;
    attempt <= 120;
    attempt++, scheduled++
  ) {
    result = wakeup_schedule(
      scheduled,
      ALARM_WAKEUP_COOKIE,
      true
    );

    if (result != E_RANGE) {
      break;
    }
  }

  if (result < 0) {
    APP_LOG(
      APP_LOG_LEVEL_WARNING,
      "Medication wakeup failed: %ld",
      (long)result
    );
  } else {
    APP_LOG(
      APP_LOG_LEVEL_INFO,
      "Medication wakeup scheduled in %ld seconds",
      (long)(scheduled - now)
    );
  }
}

static void alarm_vibrate(void) {
  if (s_alarm_vibration_enabled) {
    vibes_double_pulse();
  }
}

static void alarm_stop(void) {
  cancel_timer(&s_alarm_pulse_timer);
  vibes_cancel();
  alarm_audio_stop();
  s_alarm_active = false;
  s_alarm_stop_time = 0;
  s_alarm_due_symbol_mask = 0;
}

static void alarm_pulse_timer_handler(void *context) {
  (void)context;
  s_alarm_pulse_timer = NULL;

  if (!s_alarm_active) {
    return;
  }

  if (time(NULL) >= s_alarm_stop_time) {
    alarm_stop();
    return;
  }

  alarm_vibrate();

  if (!s_alarm_audio_active) {
    (void)alarm_audio_start();
  }

  s_alarm_pulse_timer = app_timer_register(
    ALARM_VIBE_INTERVAL_MS,
    alarm_pulse_timer_handler,
    NULL
  );

  if (!s_alarm_pulse_timer) {
    alarm_stop();
  }
}

static void alarm_start(void) {
  if (s_alarm_active) {
    return;
  }

  if (s_transfer_screen_active) {
    schedule_next_alarm_wakeup();
    return;
  }

  refresh_app_screen_state();

  const time_t now = time(NULL);
  const uint8_t due_mask =
      alarm_unconfirmed_symbol_mask_at(now);

  if (due_mask == 0) {
    schedule_next_alarm_wakeup();
    return;
  }

  alarm_refresh_window_state();
  s_alarm_window_state.last_reminder =
      (int32_t)now;
  persist_alarm_window_state();

  s_alarm_due_symbol_mask = due_mask;
  s_alarm_active = true;
  s_alarm_stop_time = now + ALARM_ACTIVE_SECONDS;

  alarm_pulse_timer_handler(NULL);
}

static void alarm_wakeup_handler(
    WakeupId wakeup_id,
    int32_t cookie
) {
  (void)wakeup_id;

  if (cookie != ALARM_WAKEUP_COOKIE) {
    return;
  }

  refresh_medication_rows_for_time();
  alarm_start();
  schedule_next_alarm_wakeup();
}

static bool minute_is_daypart_start(int minute) {
  return
      minute == s_dayparts.morning ||
      minute == s_dayparts.noon ||
      minute == s_dayparts.evening ||
      minute == s_dayparts.night;
}

static void alarm_handle_minute_tick(
    const struct tm *tick_time
) {
  if (!tick_time) {
    return;
  }

  const int minute =
      tick_time->tm_hour * 60 +
      tick_time->tm_min;

  if (minute_is_daypart_start(minute)) {
    alarm_refresh_window_state();
    alarm_start();
  }

  schedule_next_alarm_wakeup();
}

static void alarm_reset_after_settings_save(void) {
  alarm_stop();
  wakeup_cancel_all();

  time_t window_start = 0;
  (void)alarm_window_bounds_at(
    time(NULL),
    &window_start,
    NULL,
    NULL
  );

  s_alarm_window_state = (AlarmWindowState) {
    .window_start = (int32_t)window_start,
    .last_reminder = 0,
    .confirmed_mask = 0
  };
  s_alarm_window_state_loaded = true;
  persist_alarm_window_state();

  if (!s_transfer_screen_active) {
    refresh_app_screen_state();
  }
  schedule_next_alarm_wakeup();

  APP_LOG(
    APP_LOG_LEVEL_INFO,
    "Settings saved: alarm plan reset"
  );
}

static void alarm_confirmation_received(
    MedicationSymbol symbol
) {
  alarm_refresh_window_state();

  const uint8_t symbol_mask =
      (uint8_t)(1u << symbol);

  s_alarm_window_state.confirmed_mask |=
      symbol_mask;
  persist_alarm_window_state();

  s_alarm_due_symbol_mask &=
      (uint8_t)~symbol_mask;

  if (
    s_alarm_active &&
    s_alarm_due_symbol_mask == 0
  ) {
    alarm_stop();
  }

  schedule_next_alarm_wakeup();
}

static int32_t abs_int32(int32_t value) {
  return value < 0 ? -value : value;
}

static int32_t clamp_symmetric(int32_t value, int32_t limit) {
  if (value > limit) {
    return limit;
  }

  if (value < -limit) {
    return -limit;
  }

  return value;
}

static uint32_t current_time_ms(void) {
  time_t seconds;
  uint16_t milliseconds;

  time_ms(
    &seconds,
    &milliseconds
  );

  return
      (uint32_t)seconds * 1000u +
      milliseconds;
}

static int32_t snap_anchor_for_index(int index) {
  if (index <= 0) {
    return CANVAS_START_OFFSET_Y;
  }

  const int medication_index = index - 1;

  const int32_t row_center_from_pill_center =
      s_frame_height / 2 +
      MEDICATION_GAP - 7 +
      HINT_POSITION_ADJUST_Y +
      HINT_HEIGHT +
      15 +
      MEDICATION_HEADER_HEIGHT +
      medication_index *
          (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP) +
      MEDICATION_ROW_HEIGHT / 2;

  return -row_center_from_pill_center;
}

static int clamp_snap_index(int index) {
  if (index < 0) {
    return 0;
  }

  if (index > LIST_ROW_COUNT) {
    return LIST_ROW_COUNT;
  }

  return index;
}

static int32_t visual_canvas_offset_y(void) {
  const int32_t rounding =
      s_scroll.position_q8 >= 0
          ? SCROLL_Q8 / 2
          : -SCROLL_Q8 / 2;

  return
      (
        s_scroll.position_q8 +
        rounding
      ) /
      SCROLL_Q8;
}

static void destroy_pill_bitmaps(void) {
  for (uint8_t index = 0; index < FRAME_COUNT; index++) {
    if (s_frames[index]) {
      gbitmap_destroy(s_frames[index]);
      s_frames[index] = NULL;
    }
  }

  if (s_sheet) {
    gbitmap_destroy(s_sheet);
    s_sheet = NULL;
  }
}

static void draw_scroll_hint(
    GContext *ctx,
    GRect bounds,
    int32_t pill_y
) {
  const int32_t hint_y =
      pill_y +
      s_frame_height +
      MEDICATION_GAP - 7 +
      HINT_POSITION_ADJUST_Y +
      s_hint_offsets[s_animation_tick % ARRAY_LENGTH(s_hint_offsets)];

  if (hint_y < -HINT_HEIGHT - 2 ||
      hint_y > bounds.size.h + HINT_HEIGHT + 2) {
    return;
  }

  const int16_t x = bounds.size.w / 2;
  const int16_t y = (int16_t)hint_y;

  graphics_context_set_stroke_color(ctx, theme_hint_color());

  graphics_draw_line(
    ctx,
    GPoint(x - HINT_HALF_WIDTH, y),
    GPoint(x, y + HINT_HEIGHT)
  );
  graphics_draw_line(
    ctx,
    GPoint(x - HINT_HALF_WIDTH, y + 1),
    GPoint(x, y + HINT_HEIGHT + 1)
  );
  graphics_draw_line(
    ctx,
    GPoint(x, y + HINT_HEIGHT),
    GPoint(x + HINT_HALF_WIDTH, y)
  );
  graphics_draw_line(
    ctx,
    GPoint(x, y + HINT_HEIGHT + 1),
    GPoint(x + HINT_HALF_WIDTH, y + 1)
  );
}

static int32_t current_pill_y(void) {
  if (!s_canvas_layer) {
    return 0;
  }

  const GRect bounds =
      layer_get_bounds(
        s_canvas_layer
      );

  return
      (
        (
          int32_t
        )bounds.size.h -
        s_frame_height
      ) /
      2 +
      visual_canvas_offset_y();
}

static bool pill_physics_medication_is_visible(
    const MedicationSettings *medication
) {
  return
      medication &&
      medication->enabled &&
      medication->icon_set &&
      medication->symbol == MEDICATION_SYMBOL_PILL &&
      medication->time ==
          (uint8_t)current_medication_time() &&
      !s_pills_confirmed;
}

static int32_t pill_rb_clamp_angle(int32_t angle) {
  while (angle < 0) {
    angle += TRIG_MAX_ANGLE;
  }
  while (angle >= TRIG_MAX_ANGLE) {
    angle -= TRIG_MAX_ANGLE;
  }
  return angle;
}

static int32_t pill_rb_clamp_int32(
    int32_t value,
    int32_t minimum,
    int32_t maximum
) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

static uint64_t pill_rb_integer_sqrt64(uint64_t value) {
  uint64_t result = 0;
  uint64_t bit = (uint64_t)1 << 62;

  while (bit > value) {
    bit >>= 2;
  }

  while (bit != 0) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }

  return result;
}

static void pill_rb_collision_geometry(
    uint8_t medication_index,
    uint8_t *half_length,
    uint8_t *radius
) {
  uint8_t shape = 0;
  uint8_t size = 100;

  if (medication_index < s_medication_count) {
    shape = s_medications[medication_index].shape;
  }

  if (
    medication_index < s_medication_appearance_count &&
    s_medication_appearances[medication_index].valid
  ) {
    const MedicationAppearance *appearance =
        &s_medication_appearances[medication_index];
    shape = appearance->shape;
    size = appearance->size;
  }

  int16_t local_half_length;
  int16_t local_radius;

  switch (shape) {
    case 1:
      local_half_length = 19;
      local_radius = 13;
      break;
    case 2:
      local_half_length = 8;
      local_radius = 7;
      break;
    case 4:
      local_half_length = 26;
      local_radius = 12;
      break;
    case 3:
      local_half_length = 0;
      local_radius = 13;
      break;
    case 0:
    default:
      local_half_length = 0;
      local_radius = 10;
      break;
  }

  local_half_length = (int16_t)(
    ((int32_t)local_half_length * size + 50) / 100
  );
  local_radius = (int16_t)(
    ((int32_t)local_radius * size + 50) / 100
  );

  /* Small safety envelope prevents visible interpenetration. */
  if (local_half_length > 0) {
    local_half_length += 1;
  }
  local_radius += 2;

  if (local_half_length < 0) {
    local_half_length = 0;
  } else if (local_half_length > 40) {
    local_half_length = 40;
  }

  if (local_radius < 5) {
    local_radius = 5;
  } else if (local_radius > 28) {
    local_radius = 28;
  }

  *half_length = (uint8_t)local_half_length;
  *radius = (uint8_t)local_radius;
}

static int32_t pill_rb_inertia(const PillPhysicsBody *body) {
  const int32_t half_length =
      body->collision_half_length;
  const int32_t radius =
      body->collision_radius;
  int32_t inertia =
      (half_length * half_length) / 3 +
      (radius * radius) / 2;

  if (inertia < 24) {
    inertia = 24;
  }

  return inertia;
}

static void pill_rb_segment_endpoints(
    const PillPhysicsBody *body,
    int32_t *ax_q8,
    int32_t *ay_q8,
    int32_t *bx_q8,
    int32_t *by_q8
) {
  const int32_t dx_q8 = (int32_t)(
    ((int64_t)cos_lookup(body->angle) *
     body->collision_half_length *
     PILL_PHYSICS_Q8) /
    TRIG_MAX_RATIO
  );
  const int32_t dy_q8 = (int32_t)(
    ((int64_t)sin_lookup(body->angle) *
     body->collision_half_length *
     PILL_PHYSICS_Q8) /
    TRIG_MAX_RATIO
  );

  *ax_q8 = body->x_q8 - dx_q8;
  *ay_q8 = body->y_q8 - dy_q8;
  *bx_q8 = body->x_q8 + dx_q8;
  *by_q8 = body->y_q8 + dy_q8;
}

static int32_t pill_rb_rotational_velocity_q8(
    int32_t angular_velocity,
    int16_t lever_pixels
) {
  return (int32_t)(
    ((int64_t)angular_velocity *
     lever_pixels *
     PILL_RB_ANGLE_TO_LINEAR_NUM) /
    PILL_RB_ANGLE_TO_LINEAR_DEN
  );
}

static void pill_rb_contact_velocity(
    const PillPhysicsBody *body,
    int16_t lever_x,
    int16_t lever_y,
    int32_t *velocity_x_q8,
    int32_t *velocity_y_q8
) {
  *velocity_x_q8 =
      body->vx_q8 -
      pill_rb_rotational_velocity_q8(
        body->angular_velocity,
        lever_y
      );
  *velocity_y_q8 =
      body->vy_q8 +
      pill_rb_rotational_velocity_q8(
        body->angular_velocity,
        lever_x
      );
}

static int32_t pill_rb_cross_lever_normal(
    int16_t lever_x,
    int16_t lever_y,
    int32_t normal_x_q12,
    int32_t normal_y_q12
) {
  return (int32_t)(
    ((int64_t)lever_x * normal_y_q12 -
     (int64_t)lever_y * normal_x_q12) /
    PILL_RB_PARAMETER_Q12
  );
}

static void pill_rb_apply_impulse(
    PillPhysicsBody *body,
    int32_t impulse_q8,
    int32_t normal_x_q12,
    int32_t normal_y_q12,
    int16_t lever_x,
    int16_t lever_y,
    int direction
) {
  body->vx_q8 += (int32_t)(
    ((int64_t)direction * impulse_q8 *
     normal_x_q12) /
    PILL_RB_PARAMETER_Q12
  );
  body->vy_q8 += (int32_t)(
    ((int64_t)direction * impulse_q8 *
     normal_y_q12) /
    PILL_RB_PARAMETER_Q12
  );

  const int32_t cross =
      pill_rb_cross_lever_normal(
        lever_x,
        lever_y,
        normal_x_q12,
        normal_y_q12
      );
  const int32_t inertia = pill_rb_inertia(body);

  body->angular_velocity += (int32_t)(
    ((int64_t)direction * cross * impulse_q8 *
     PILL_RB_RAD_TO_ANGLE_NUM) /
    ((int64_t)PILL_PHYSICS_Q8 * inertia)
  );

  body->angular_velocity = pill_rb_clamp_int32(
    body->angular_velocity,
    -PILL_RB_MAX_ANGULAR,
    PILL_RB_MAX_ANGULAR
  );
}

static int32_t pill_rb_effective_mass_q8(
    const PillPhysicsBody *first,
    int16_t first_lever_x,
    int16_t first_lever_y,
    const PillPhysicsBody *second,
    int16_t second_lever_x,
    int16_t second_lever_y,
    int32_t normal_x_q12,
    int32_t normal_y_q12
) {
  int32_t denominator_q8 = PILL_PHYSICS_Q8;

  const int32_t first_cross =
      pill_rb_cross_lever_normal(
        first_lever_x,
        first_lever_y,
        normal_x_q12,
        normal_y_q12
      );
  denominator_q8 += (int32_t)(
    ((int64_t)first_cross * first_cross *
     PILL_PHYSICS_Q8) /
    pill_rb_inertia(first)
  );

  if (second) {
    denominator_q8 += PILL_PHYSICS_Q8;
    const int32_t second_cross =
        pill_rb_cross_lever_normal(
          second_lever_x,
          second_lever_y,
          normal_x_q12,
          normal_y_q12
        );
    denominator_q8 += (int32_t)(
      ((int64_t)second_cross * second_cross *
       PILL_PHYSICS_Q8) /
      pill_rb_inertia(second)
    );
  }

  return denominator_q8 > 0
      ? denominator_q8
      : PILL_PHYSICS_Q8;
}

static int32_t pill_rb_solve_contact_impulse(
    PillPhysicsBody *first,
    PillPhysicsBody *second,
    int16_t first_lever_x,
    int16_t first_lever_y,
    int16_t second_lever_x,
    int16_t second_lever_y,
    int32_t normal_x_q12,
    int32_t normal_y_q12
) {
  int32_t first_vx_q8;
  int32_t first_vy_q8;
  int32_t second_vx_q8 = 0;
  int32_t second_vy_q8 = 0;

  pill_rb_contact_velocity(
    first,
    first_lever_x,
    first_lever_y,
    &first_vx_q8,
    &first_vy_q8
  );

  if (second) {
    pill_rb_contact_velocity(
      second,
      second_lever_x,
      second_lever_y,
      &second_vx_q8,
      &second_vy_q8
    );
  }

  const int32_t relative_vx_q8 =
      first_vx_q8 - second_vx_q8;
  const int32_t relative_vy_q8 =
      first_vy_q8 - second_vy_q8;
  const int32_t normal_velocity_q8 = (int32_t)(
    ((int64_t)relative_vx_q8 * normal_x_q12 +
     (int64_t)relative_vy_q8 * normal_y_q12) /
    PILL_RB_PARAMETER_Q12
  );

  if (normal_velocity_q8 >= 0) {
    return 0;
  }

  const int32_t denominator_q8 =
      pill_rb_effective_mass_q8(
        first,
        first_lever_x,
        first_lever_y,
        second,
        second_lever_x,
        second_lever_y,
        normal_x_q12,
        normal_y_q12
      );

  /*
   * Resting contacts must not bounce. At tiny closing speeds the previous
   * 20 % restitution re-injected energy every frame, especially where two
   * walls constrained the same pill. Real impact bounce is retained only
   * above the low-speed threshold.
   */
  const int32_t closing_speed_q8 =
      -normal_velocity_q8;
  const int32_t restitution_num =
      closing_speed_q8 >
          PILL_RB_RESTITUTION_SPEED_Q8
          ? PILL_RB_RESTITUTION_NUM
          : 0;
  const int32_t normal_impulse_q8 = (int32_t)(
    ((int64_t)closing_speed_q8 *
     (PILL_RB_RESTITUTION_DEN +
      restitution_num) *
     PILL_PHYSICS_Q8) /
    ((int64_t)PILL_RB_RESTITUTION_DEN *
     denominator_q8)
  );

  pill_rb_apply_impulse(
    first,
    normal_impulse_q8,
    normal_x_q12,
    normal_y_q12,
    first_lever_x,
    first_lever_y,
    1
  );

  if (second) {
    pill_rb_apply_impulse(
      second,
      normal_impulse_q8,
      normal_x_q12,
      normal_y_q12,
      second_lever_x,
      second_lever_y,
      -1
    );
  }

  /* Coulomb friction at the same contact point. */
  const int32_t tangent_x_q12 = -normal_y_q12;
  const int32_t tangent_y_q12 = normal_x_q12;

  pill_rb_contact_velocity(
    first,
    first_lever_x,
    first_lever_y,
    &first_vx_q8,
    &first_vy_q8
  );
  if (second) {
    pill_rb_contact_velocity(
      second,
      second_lever_x,
      second_lever_y,
      &second_vx_q8,
      &second_vy_q8
    );
  }

  const int32_t tangent_velocity_q8 = (int32_t)(
    ((int64_t)(first_vx_q8 - second_vx_q8) *
         tangent_x_q12 +
     (int64_t)(first_vy_q8 - second_vy_q8) *
         tangent_y_q12) /
    PILL_RB_PARAMETER_Q12
  );

  const int32_t tangent_denominator_q8 =
      pill_rb_effective_mass_q8(
        first,
        first_lever_x,
        first_lever_y,
        second,
        second_lever_x,
        second_lever_y,
        tangent_x_q12,
        tangent_y_q12
      );

  int32_t tangent_impulse_q8 = (int32_t)(
    ((int64_t)(-tangent_velocity_q8) *
     PILL_PHYSICS_Q8) /
    tangent_denominator_q8
  );
  const int32_t friction_limit_q8 = (int32_t)(
    ((int64_t)normal_impulse_q8 *
     PILL_RB_FRICTION_NUM) /
    PILL_RB_FRICTION_DEN
  );

  tangent_impulse_q8 = pill_rb_clamp_int32(
    tangent_impulse_q8,
    -friction_limit_q8,
    friction_limit_q8
  );

  pill_rb_apply_impulse(
    first,
    tangent_impulse_q8,
    tangent_x_q12,
    tangent_y_q12,
    first_lever_x,
    first_lever_y,
    1
  );
  if (second) {
    pill_rb_apply_impulse(
      second,
      tangent_impulse_q8,
      tangent_x_q12,
      tangent_y_q12,
      second_lever_x,
      second_lever_y,
      -1
    );
  }

  return normal_impulse_q8;
}

static void pill_rb_initialize_body(
    PillPhysicsBody *body,
    uint8_t medication_index,
    uint8_t body_index
) {
  if (!body || medication_index >= s_medication_count) {
    return;
  }

  uint8_t half_length;
  uint8_t radius;
  pill_rb_collision_geometry(
    medication_index,
    &half_length,
    &radius
  );

  int16_t arena_width = 228;
  int16_t arena_height = 228;
  int16_t arena_y = 0;

  if (s_canvas_layer) {
    const GRect bounds = layer_get_bounds(s_canvas_layer);
    arena_width = bounds.size.w;
    arena_height = bounds.size.h;
    arena_y = (int16_t)current_pill_y();
  }

  const uint8_t column = body_index % 3;
  const uint8_t row = body_index / 3;
  int16_t x = (int16_t)(
    ((int32_t)(column * 2 + 1) * arena_width) / 6
  );
  int16_t screen_y = (int16_t)(22 + row * 38);
  int16_t local_y = (int16_t)(screen_y - arena_y);
  const int16_t extent =
      radius + half_length + PILL_PHYSICS_EDGE_MARGIN;

  if (x < extent) {
    x = extent;
  } else if (x > arena_width - extent) {
    x = arena_width - extent;
  }

  if (screen_y < extent) {
    local_y = extent - arena_y;
  } else if (screen_y > arena_height - extent) {
    local_y = arena_height - extent - arena_y;
  }

  *body = (PillPhysicsBody) {
    .x_q8 = (int32_t)x * PILL_PHYSICS_Q8,
    .y_q8 = (int32_t)local_y * PILL_PHYSICS_Q8,
    .vx_q8 = 0,
    .vy_q8 = 0,
    .angle =
        ((int32_t)(body_index * 5u + 1u) *
         TRIG_MAX_ANGLE) /
        32,
    .angular_velocity = 0,
    .medication_index = medication_index,
    .collision_radius = radius,
    .collision_half_length = half_length
  };
}

static bool pill_rb_add_body(uint8_t medication_index) {
  if (
    s_pill_physics_body_count >=
        PILL_PHYSICS_MAX_BODIES
  ) {
    return false;
  }

  pill_rb_initialize_body(
    &s_pill_physics_bodies[
      s_pill_physics_body_count
    ],
    medication_index,
    s_pill_physics_body_count
  );
  s_pill_physics_body_count++;
  return true;
}

static void pill_physics_rebuild(void) {
  memset(
    s_pill_physics_bodies,
    0,
    sizeof(s_pill_physics_bodies)
  );
  s_pill_physics_body_count = 0;

  uint8_t remaining[MAX_MEDICATIONS] = { 0 };

  for (
    uint8_t index = 0;
    index < s_medication_count &&
    s_pill_physics_body_count <
        PILL_PHYSICS_MAX_BODIES;
    index++
  ) {
    const MedicationSettings *medication =
        &s_medications[index];

    if (!pill_physics_medication_is_visible(medication)) {
      continue;
    }

    (void)pill_rb_add_body(index);
    remaining[index] =
        medication->quantity > 0
            ? medication->quantity - 1
            : 0;
  }

  bool added = true;
  while (
    added &&
    s_pill_physics_body_count <
        PILL_PHYSICS_MAX_BODIES
  ) {
    added = false;
    for (
      uint8_t index = 0;
      index < s_medication_count &&
      s_pill_physics_body_count <
          PILL_PHYSICS_MAX_BODIES;
      index++
    ) {
      if (remaining[index] == 0) {
        continue;
      }
      if (pill_rb_add_body(index)) {
        remaining[index]--;
        added = true;
      }
    }
  }

  s_pill_physics_gravity_x = 0;
  s_pill_physics_gravity_y = 0;
  s_pill_physics_last_target_x = 0;
  s_pill_physics_last_target_y = 0;
  s_pill_physics_quiet_frames = 0;
  s_pill_physics_sensor_quiet_samples = 0;

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void pill_rb_closest_point_on_segment(
    int32_t point_x_q8,
    int32_t point_y_q8,
    int32_t start_x_q8,
    int32_t start_y_q8,
    int32_t end_x_q8,
    int32_t end_y_q8,
    int32_t *closest_x_q8,
    int32_t *closest_y_q8
) {
  const int32_t segment_x_q8 = end_x_q8 - start_x_q8;
  const int32_t segment_y_q8 = end_y_q8 - start_y_q8;
  const int64_t length_squared =
      (int64_t)segment_x_q8 * segment_x_q8 +
      (int64_t)segment_y_q8 * segment_y_q8;

  if (length_squared <= 0) {
    *closest_x_q8 = start_x_q8;
    *closest_y_q8 = start_y_q8;
    return;
  }

  int64_t parameter_q12 =
      ((int64_t)(point_x_q8 - start_x_q8) *
           segment_x_q8 +
       (int64_t)(point_y_q8 - start_y_q8) *
           segment_y_q8) *
      PILL_RB_PARAMETER_Q12 /
      length_squared;

  if (parameter_q12 < 0) {
    parameter_q12 = 0;
  } else if (parameter_q12 > PILL_RB_PARAMETER_Q12) {
    parameter_q12 = PILL_RB_PARAMETER_Q12;
  }

  *closest_x_q8 = start_x_q8 + (int32_t)(
    ((int64_t)segment_x_q8 * parameter_q12) /
    PILL_RB_PARAMETER_Q12
  );
  *closest_y_q8 = start_y_q8 + (int32_t)(
    ((int64_t)segment_y_q8 * parameter_q12) /
    PILL_RB_PARAMETER_Q12
  );
}

static int64_t pill_rb_cross_q8(
    int32_t ax_q8,
    int32_t ay_q8,
    int32_t bx_q8,
    int32_t by_q8
) {
  return
      (int64_t)ax_q8 * by_q8 -
      (int64_t)ay_q8 * bx_q8;
}

static bool pill_rb_segment_intersection(
    int32_t a0x_q8,
    int32_t a0y_q8,
    int32_t a1x_q8,
    int32_t a1y_q8,
    int32_t b0x_q8,
    int32_t b0y_q8,
    int32_t b1x_q8,
    int32_t b1y_q8,
    int32_t *intersection_x_q8,
    int32_t *intersection_y_q8
) {
  const int32_t rx_q8 = a1x_q8 - a0x_q8;
  const int32_t ry_q8 = a1y_q8 - a0y_q8;
  const int32_t sx_q8 = b1x_q8 - b0x_q8;
  const int32_t sy_q8 = b1y_q8 - b0y_q8;
  const int32_t qpx_q8 = b0x_q8 - a0x_q8;
  const int32_t qpy_q8 = b0y_q8 - a0y_q8;
  const int64_t denominator =
      pill_rb_cross_q8(
        rx_q8,
        ry_q8,
        sx_q8,
        sy_q8
      );

  if (denominator == 0) {
    return false;
  }

  const int64_t t_numerator =
      pill_rb_cross_q8(
        qpx_q8,
        qpy_q8,
        sx_q8,
        sy_q8
      );
  const int64_t u_numerator =
      pill_rb_cross_q8(
        qpx_q8,
        qpy_q8,
        rx_q8,
        ry_q8
      );

  if (
    (denominator > 0 &&
     (t_numerator < 0 || t_numerator > denominator ||
      u_numerator < 0 || u_numerator > denominator)) ||
    (denominator < 0 &&
     (t_numerator > 0 || t_numerator < denominator ||
      u_numerator > 0 || u_numerator < denominator))
  ) {
    return false;
  }

  const int64_t t_q12 =
      t_numerator * PILL_RB_PARAMETER_Q12 /
      denominator;
  *intersection_x_q8 = a0x_q8 + (int32_t)(
    ((int64_t)rx_q8 * t_q12) /
    PILL_RB_PARAMETER_Q12
  );
  *intersection_y_q8 = a0y_q8 + (int32_t)(
    ((int64_t)ry_q8 * t_q12) /
    PILL_RB_PARAMETER_Q12
  );
  return true;
}

static void pill_rb_consider_closest_pair(
    int32_t candidate_ax_q8,
    int32_t candidate_ay_q8,
    int32_t candidate_bx_q8,
    int32_t candidate_by_q8,
    uint64_t *best_distance_squared,
    int32_t *best_ax_q8,
    int32_t *best_ay_q8,
    int32_t *best_bx_q8,
    int32_t *best_by_q8
) {
  const int64_t dx_q8 =
      (int64_t)candidate_ax_q8 - candidate_bx_q8;
  const int64_t dy_q8 =
      (int64_t)candidate_ay_q8 - candidate_by_q8;
  const uint64_t distance_squared = (uint64_t)(
    dx_q8 * dx_q8 + dy_q8 * dy_q8
  );

  if (distance_squared >= *best_distance_squared) {
    return;
  }

  *best_distance_squared = distance_squared;
  *best_ax_q8 = candidate_ax_q8;
  *best_ay_q8 = candidate_ay_q8;
  *best_bx_q8 = candidate_bx_q8;
  *best_by_q8 = candidate_by_q8;
}

static void pill_rb_closest_segment_points(
    const PillPhysicsBody *first,
    const PillPhysicsBody *second,
    int32_t *first_x_q8,
    int32_t *first_y_q8,
    int32_t *second_x_q8,
    int32_t *second_y_q8
) {
  int32_t a0x_q8;
  int32_t a0y_q8;
  int32_t a1x_q8;
  int32_t a1y_q8;
  int32_t b0x_q8;
  int32_t b0y_q8;
  int32_t b1x_q8;
  int32_t b1y_q8;

  pill_rb_segment_endpoints(
    first,
    &a0x_q8,
    &a0y_q8,
    &a1x_q8,
    &a1y_q8
  );
  pill_rb_segment_endpoints(
    second,
    &b0x_q8,
    &b0y_q8,
    &b1x_q8,
    &b1y_q8
  );

  int32_t intersection_x_q8;
  int32_t intersection_y_q8;
  if (
    pill_rb_segment_intersection(
      a0x_q8,
      a0y_q8,
      a1x_q8,
      a1y_q8,
      b0x_q8,
      b0y_q8,
      b1x_q8,
      b1y_q8,
      &intersection_x_q8,
      &intersection_y_q8
    )
  ) {
    *first_x_q8 = intersection_x_q8;
    *first_y_q8 = intersection_y_q8;
    *second_x_q8 = intersection_x_q8;
    *second_y_q8 = intersection_y_q8;
    return;
  }

  uint64_t best_distance_squared = UINT64_MAX;
  int32_t closest_x_q8;
  int32_t closest_y_q8;

  pill_rb_closest_point_on_segment(
    a0x_q8,
    a0y_q8,
    b0x_q8,
    b0y_q8,
    b1x_q8,
    b1y_q8,
    &closest_x_q8,
    &closest_y_q8
  );
  pill_rb_consider_closest_pair(
    a0x_q8,
    a0y_q8,
    closest_x_q8,
    closest_y_q8,
    &best_distance_squared,
    first_x_q8,
    first_y_q8,
    second_x_q8,
    second_y_q8
  );

  pill_rb_closest_point_on_segment(
    a1x_q8,
    a1y_q8,
    b0x_q8,
    b0y_q8,
    b1x_q8,
    b1y_q8,
    &closest_x_q8,
    &closest_y_q8
  );
  pill_rb_consider_closest_pair(
    a1x_q8,
    a1y_q8,
    closest_x_q8,
    closest_y_q8,
    &best_distance_squared,
    first_x_q8,
    first_y_q8,
    second_x_q8,
    second_y_q8
  );

  pill_rb_closest_point_on_segment(
    b0x_q8,
    b0y_q8,
    a0x_q8,
    a0y_q8,
    a1x_q8,
    a1y_q8,
    &closest_x_q8,
    &closest_y_q8
  );
  pill_rb_consider_closest_pair(
    closest_x_q8,
    closest_y_q8,
    b0x_q8,
    b0y_q8,
    &best_distance_squared,
    first_x_q8,
    first_y_q8,
    second_x_q8,
    second_y_q8
  );

  pill_rb_closest_point_on_segment(
    b1x_q8,
    b1y_q8,
    a0x_q8,
    a0y_q8,
    a1x_q8,
    a1y_q8,
    &closest_x_q8,
    &closest_y_q8
  );
  pill_rb_consider_closest_pair(
    closest_x_q8,
    closest_y_q8,
    b1x_q8,
    b1y_q8,
    &best_distance_squared,
    first_x_q8,
    first_y_q8,
    second_x_q8,
    second_y_q8
  );
}

static bool pill_rb_solve_pair(
    PillPhysicsBody *first,
    PillPhysicsBody *second
) {
  int32_t first_line_x_q8;
  int32_t first_line_y_q8;
  int32_t second_line_x_q8;
  int32_t second_line_y_q8;

  pill_rb_closest_segment_points(
    first,
    second,
    &first_line_x_q8,
    &first_line_y_q8,
    &second_line_x_q8,
    &second_line_y_q8
  );

  int32_t delta_x_q8 =
      first_line_x_q8 - second_line_x_q8;
  int32_t delta_y_q8 =
      first_line_y_q8 - second_line_y_q8;
  uint64_t distance_squared =
      (uint64_t)((int64_t)delta_x_q8 * delta_x_q8) +
      (uint64_t)((int64_t)delta_y_q8 * delta_y_q8);
  int32_t distance_q8 = (int32_t)
      pill_rb_integer_sqrt64(distance_squared);
  const int32_t minimum_distance_q8 =
      (first->collision_radius +
       second->collision_radius) *
      PILL_PHYSICS_Q8;

  if (distance_q8 >= minimum_distance_q8) {
    return false;
  }

  if (distance_q8 <= 0) {
    delta_x_q8 = first->x_q8 - second->x_q8;
    delta_y_q8 = first->y_q8 - second->y_q8;
    distance_squared =
        (uint64_t)((int64_t)delta_x_q8 * delta_x_q8) +
        (uint64_t)((int64_t)delta_y_q8 * delta_y_q8);
    distance_q8 = (int32_t)
        pill_rb_integer_sqrt64(distance_squared);

    if (distance_q8 <= 0) {
      delta_x_q8 = PILL_PHYSICS_Q8;
      delta_y_q8 = 0;
      distance_q8 = PILL_PHYSICS_Q8;
    }
  }

  const int32_t normal_x_q12 = (int32_t)(
    ((int64_t)delta_x_q8 *
     PILL_RB_PARAMETER_Q12) /
    distance_q8
  );
  const int32_t normal_y_q12 = (int32_t)(
    ((int64_t)delta_y_q8 *
     PILL_RB_PARAMETER_Q12) /
    distance_q8
  );
  int32_t penetration_q8 =
      minimum_distance_q8 - distance_q8;

  if (penetration_q8 > PILL_RB_POSITION_SLOP_Q8) {
    penetration_q8 -= PILL_RB_POSITION_SLOP_Q8;
    const int32_t correction_x_q8 = (int32_t)(
      ((int64_t)normal_x_q12 * penetration_q8) /
      (PILL_RB_PARAMETER_Q12 * 2)
    );
    const int32_t correction_y_q8 = (int32_t)(
      ((int64_t)normal_y_q12 * penetration_q8) /
      (PILL_RB_PARAMETER_Q12 * 2)
    );
    first->x_q8 += correction_x_q8;
    first->y_q8 += correction_y_q8;
    second->x_q8 -= correction_x_q8;
    second->y_q8 -= correction_y_q8;
  }

  const int32_t first_surface_x_q8 =
      first_line_x_q8 - (int32_t)(
        ((int64_t)normal_x_q12 *
         first->collision_radius *
         PILL_PHYSICS_Q8) /
        PILL_RB_PARAMETER_Q12
      );
  const int32_t first_surface_y_q8 =
      first_line_y_q8 - (int32_t)(
        ((int64_t)normal_y_q12 *
         first->collision_radius *
         PILL_PHYSICS_Q8) /
        PILL_RB_PARAMETER_Q12
      );
  const int32_t second_surface_x_q8 =
      second_line_x_q8 + (int32_t)(
        ((int64_t)normal_x_q12 *
         second->collision_radius *
         PILL_PHYSICS_Q8) /
        PILL_RB_PARAMETER_Q12
      );
  const int32_t second_surface_y_q8 =
      second_line_y_q8 + (int32_t)(
        ((int64_t)normal_y_q12 *
         second->collision_radius *
         PILL_PHYSICS_Q8) /
        PILL_RB_PARAMETER_Q12
      );
  const int32_t contact_x_q8 =
      (first_surface_x_q8 + second_surface_x_q8) / 2;
  const int32_t contact_y_q8 =
      (first_surface_y_q8 + second_surface_y_q8) / 2;

  pill_rb_solve_contact_impulse(
    first,
    second,
    (int16_t)(
      (contact_x_q8 - first->x_q8) /
      PILL_PHYSICS_Q8
    ),
    (int16_t)(
      (contact_y_q8 - first->y_q8) /
      PILL_PHYSICS_Q8
    ),
    (int16_t)(
      (contact_x_q8 - second->x_q8) /
      PILL_PHYSICS_Q8
    ),
    (int16_t)(
      (contact_y_q8 - second->y_q8) /
      PILL_PHYSICS_Q8
    ),
    normal_x_q12,
    normal_y_q12
  );
  return true;
}

static bool pill_rb_solve_wall(
    PillPhysicsBody *body,
    uint8_t wall,
    int32_t minimum_x_q8,
    int32_t maximum_x_q8,
    int32_t minimum_y_q8,
    int32_t maximum_y_q8
) {
  int32_t ax_q8;
  int32_t ay_q8;
  int32_t bx_q8;
  int32_t by_q8;
  pill_rb_segment_endpoints(
    body,
    &ax_q8,
    &ay_q8,
    &bx_q8,
    &by_q8
  );

  const int32_t radius_q8 =
      body->collision_radius * PILL_PHYSICS_Q8;
  int32_t penetration_q8 = 0;
  int32_t normal_x_q12 = 0;
  int32_t normal_y_q12 = 0;
  int32_t support_x_q8 = body->x_q8;
  int32_t support_y_q8 = body->y_q8;

  switch (wall) {
    case 0: {
      const int32_t minimum_surface_q8 =
          (ax_q8 < bx_q8 ? ax_q8 : bx_q8) -
          radius_q8;
      penetration_q8 =
          minimum_x_q8 - minimum_surface_q8;
      normal_x_q12 = PILL_RB_PARAMETER_Q12;
      if (abs_int32(ax_q8 - bx_q8) <= PILL_RB_FLAT_CONTACT_TOLERANCE_Q8) {
        support_x_q8 = (ax_q8 + bx_q8) / 2;
        support_y_q8 = (ay_q8 + by_q8) / 2;
      } else if (ax_q8 < bx_q8) {
        support_x_q8 = ax_q8;
        support_y_q8 = ay_q8;
      } else {
        support_x_q8 = bx_q8;
        support_y_q8 = by_q8;
      }
      break;
    }
    case 1: {
      const int32_t maximum_surface_q8 =
          (ax_q8 > bx_q8 ? ax_q8 : bx_q8) +
          radius_q8;
      penetration_q8 =
          maximum_surface_q8 - maximum_x_q8;
      normal_x_q12 = -PILL_RB_PARAMETER_Q12;
      if (abs_int32(ax_q8 - bx_q8) <= PILL_RB_FLAT_CONTACT_TOLERANCE_Q8) {
        support_x_q8 = (ax_q8 + bx_q8) / 2;
        support_y_q8 = (ay_q8 + by_q8) / 2;
      } else if (ax_q8 > bx_q8) {
        support_x_q8 = ax_q8;
        support_y_q8 = ay_q8;
      } else {
        support_x_q8 = bx_q8;
        support_y_q8 = by_q8;
      }
      break;
    }
    case 2: {
      const int32_t minimum_surface_q8 =
          (ay_q8 < by_q8 ? ay_q8 : by_q8) -
          radius_q8;
      penetration_q8 =
          minimum_y_q8 - minimum_surface_q8;
      normal_y_q12 = PILL_RB_PARAMETER_Q12;
      if (abs_int32(ay_q8 - by_q8) <= PILL_RB_FLAT_CONTACT_TOLERANCE_Q8) {
        support_x_q8 = (ax_q8 + bx_q8) / 2;
        support_y_q8 = (ay_q8 + by_q8) / 2;
      } else if (ay_q8 < by_q8) {
        support_x_q8 = ax_q8;
        support_y_q8 = ay_q8;
      } else {
        support_x_q8 = bx_q8;
        support_y_q8 = by_q8;
      }
      break;
    }
    case 3:
    default: {
      const int32_t maximum_surface_q8 =
          (ay_q8 > by_q8 ? ay_q8 : by_q8) +
          radius_q8;
      penetration_q8 =
          maximum_surface_q8 - maximum_y_q8;
      normal_y_q12 = -PILL_RB_PARAMETER_Q12;
      if (abs_int32(ay_q8 - by_q8) <= PILL_RB_FLAT_CONTACT_TOLERANCE_Q8) {
        support_x_q8 = (ax_q8 + bx_q8) / 2;
        support_y_q8 = (ay_q8 + by_q8) / 2;
      } else if (ay_q8 > by_q8) {
        support_x_q8 = ax_q8;
        support_y_q8 = ay_q8;
      } else {
        support_x_q8 = bx_q8;
        support_y_q8 = by_q8;
      }
      break;
    }
  }

  if (penetration_q8 <= 0) {
    return false;
  }

  body->x_q8 += (int32_t)(
    ((int64_t)normal_x_q12 * penetration_q8) /
    PILL_RB_PARAMETER_Q12
  );
  body->y_q8 += (int32_t)(
    ((int64_t)normal_y_q12 * penetration_q8) /
    PILL_RB_PARAMETER_Q12
  );

  const int32_t contact_x_q8 =
      support_x_q8 - (int32_t)(
        ((int64_t)normal_x_q12 * radius_q8) /
        PILL_RB_PARAMETER_Q12
      );
  const int32_t contact_y_q8 =
      support_y_q8 - (int32_t)(
        ((int64_t)normal_y_q12 * radius_q8) /
        PILL_RB_PARAMETER_Q12
      );

  pill_rb_solve_contact_impulse(
    body,
    NULL,
    (int16_t)(
      (contact_x_q8 - body->x_q8) /
      PILL_PHYSICS_Q8
    ),
    (int16_t)(
      (contact_y_q8 - body->y_q8) /
      PILL_PHYSICS_Q8
    ),
    0,
    0,
    normal_x_q12,
    normal_y_q12
  );

  return true;
}

static int16_t pill_rb_tilt_magnitude(
    int16_t x,
    int16_t y
) {
  const int64_t x_squared =
      (int64_t)x * x;
  const int64_t y_squared =
      (int64_t)y * y;

  return (int16_t)pill_rb_integer_sqrt64(
    (uint64_t)(x_squared + y_squared)
  );
}

static void pill_rb_drive_from_tilt(
    int16_t *drive_x,
    int16_t *drive_y
) {
  if (!drive_x || !drive_y) {
    return;
  }

  const int16_t magnitude =
      pill_rb_tilt_magnitude(
        s_pill_physics_gravity_x,
        s_pill_physics_gravity_y
      );

  if (magnitude <= PILL_RB_TILT_DEADZONE_MG) {
    *drive_x = 0;
    *drive_y = 0;
    return;
  }

  /*
   * Radial deadzone: the first 50 mg only overcome static friction.
   * Above that threshold the remaining tilt becomes acceleration, so there
   * is no visible jump when the pills break loose.
   */
  const int16_t active_magnitude =
      magnitude - PILL_RB_TILT_DEADZONE_MG;

  *drive_x = (int16_t)(
    ((int32_t)s_pill_physics_gravity_x *
     active_magnitude) /
    magnitude
  );
  *drive_y = (int16_t)(
    ((int32_t)s_pill_physics_gravity_y *
     active_magnitude) /
    magnitude
  );
}

static void pill_physics_tick(void *context);

static void pill_physics_schedule_tick(uint32_t delay_ms) {
  if (
    s_pill_physics_timer ||
    !s_pill_physics_window_visible ||
    s_confirmed_screen_active ||
    s_transfer_screen_active ||
    s_pill_physics_body_count == 0
  ) {
    return;
  }

  s_pill_physics_timer = app_timer_register(
    delay_ms,
    pill_physics_tick,
    NULL
  );
}

static void pill_physics_tick(void *context) {
  (void)context;
  s_pill_physics_timer = NULL;

  if (
    !s_pill_physics_window_visible ||
    s_confirmed_screen_active ||
    s_transfer_screen_active ||
    s_pill_physics_body_count == 0
  ) {
    pill_physics_update_activity();
    return;
  }

  if (s_scroll.mode != SCROLL_IDLE) {
    s_pill_physics_quiet_frames = 0;
    pill_physics_schedule_tick(
      PILL_PHYSICS_SCROLL_PAUSE_MS
    );
    return;
  }

  int16_t arena_width = 228;
  int16_t arena_height = 228;
  int16_t arena_y = 0;

  if (s_canvas_layer) {
    const GRect bounds = layer_get_bounds(s_canvas_layer);
    arena_width = bounds.size.w;
    arena_height = bounds.size.h;
    arena_y = (int16_t)current_pill_y();
  }

  const int32_t minimum_x_q8 =
      PILL_PHYSICS_EDGE_MARGIN * PILL_PHYSICS_Q8;
  const int32_t maximum_x_q8 =
      (arena_width - PILL_PHYSICS_EDGE_MARGIN) *
      PILL_PHYSICS_Q8;
  const int32_t minimum_y_q8 =
      (PILL_PHYSICS_EDGE_MARGIN - arena_y) *
      PILL_PHYSICS_Q8;
  const int32_t maximum_y_q8 =
      (arena_height - PILL_PHYSICS_EDGE_MARGIN - arena_y) *
      PILL_PHYSICS_Q8;

  int16_t drive_x;
  int16_t drive_y;
  pill_rb_drive_from_tilt(
    &drive_x,
    &drive_y
  );

  int32_t old_x_q8[PILL_PHYSICS_MAX_BODIES];
  int32_t old_y_q8[PILL_PHYSICS_MAX_BODIES];
  int32_t old_angle[PILL_PHYSICS_MAX_BODIES];

  for (
    uint8_t index = 0;
    index < s_pill_physics_body_count;
    index++
  ) {
    PillPhysicsBody *body =
        &s_pill_physics_bodies[index];
    old_x_q8[index] = body->x_q8;
    old_y_q8[index] = body->y_q8;
    old_angle[index] = body->angle;

    body->vx_q8 +=
        drive_x /
        PILL_RB_ACCEL_DIVISOR;
    body->vy_q8 +=
        drive_y /
        PILL_RB_ACCEL_DIVISOR;

    body->vx_q8 = pill_rb_clamp_int32(
      body->vx_q8,
      -PILL_RB_MAX_LINEAR_Q8,
      PILL_RB_MAX_LINEAR_Q8
    );
    body->vy_q8 = pill_rb_clamp_int32(
      body->vy_q8,
      -PILL_RB_MAX_LINEAR_Q8,
      PILL_RB_MAX_LINEAR_Q8
    );

    body->angular_velocity = (int32_t)(
      ((int64_t)body->angular_velocity *
       PILL_RB_ANGULAR_DAMPING_NUM) /
      PILL_RB_ANGULAR_DAMPING_DEN
    );

    body->x_q8 += body->vx_q8;
    body->y_q8 += body->vy_q8;
    body->angle = pill_rb_clamp_angle(
      body->angle + body->angular_velocity
    );
  }

  bool had_contact = false;

  for (
    uint8_t iteration = 0;
    iteration < PILL_RB_SOLVER_ITERATIONS;
    iteration++
  ) {
    for (
      uint8_t index = 0;
      index < s_pill_physics_body_count;
      index++
    ) {
      PillPhysicsBody *body =
          &s_pill_physics_bodies[index];
      had_contact |= pill_rb_solve_wall(
        body,
        0,
        minimum_x_q8,
        maximum_x_q8,
        minimum_y_q8,
        maximum_y_q8
      );
      had_contact |= pill_rb_solve_wall(
        body,
        1,
        minimum_x_q8,
        maximum_x_q8,
        minimum_y_q8,
        maximum_y_q8
      );
      had_contact |= pill_rb_solve_wall(
        body,
        2,
        minimum_x_q8,
        maximum_x_q8,
        minimum_y_q8,
        maximum_y_q8
      );
      had_contact |= pill_rb_solve_wall(
        body,
        3,
        minimum_x_q8,
        maximum_x_q8,
        minimum_y_q8,
        maximum_y_q8
      );
    }

    for (
      uint8_t first_index = 0;
      first_index < s_pill_physics_body_count;
      first_index++
    ) {
      for (
        uint8_t second_index = first_index + 1;
        second_index < s_pill_physics_body_count;
        second_index++
      ) {
        had_contact |= pill_rb_solve_pair(
          &s_pill_physics_bodies[first_index],
          &s_pill_physics_bodies[second_index]
        );
      }
    }
  }

  bool visual_changed = false;
  int32_t maximum_rest_travel_q8 = 0;
  int32_t maximum_rest_angle = 0;
  const bool resting_contact_candidate =
      had_contact &&
      s_pill_physics_sensor_quiet_samples >= 3;

  for (
    uint8_t index = 0;
    index < s_pill_physics_body_count;
    index++
  ) {
    PillPhysicsBody *body =
        &s_pill_physics_bodies[index];
    const int32_t travel_x_q8 =
        body->x_q8 - old_x_q8[index];
    const int32_t travel_y_q8 =
        body->y_q8 - old_y_q8[index];
    const int32_t travel_q8 =
        abs_int32(travel_x_q8) +
        abs_int32(travel_y_q8);
    int32_t angle_delta =
        body->angle - old_angle[index];

    if (angle_delta > TRIG_MAX_ANGLE / 2) {
      angle_delta -= TRIG_MAX_ANGLE;
    } else if (angle_delta < -TRIG_MAX_ANGLE / 2) {
      angle_delta += TRIG_MAX_ANGLE;
    }

    const int32_t angle_travel =
        abs_int32(angle_delta);

    if (travel_q8 > maximum_rest_travel_q8) {
      maximum_rest_travel_q8 = travel_q8;
    }
    if (angle_travel > maximum_rest_angle) {
      maximum_rest_angle = angle_travel;
    }

    /*
     * Remove only tiny residual solver velocities here. Strong drive into a
     * wall may still leave a larger attempted velocity, so final sleeping is
     * based below on the real post-solver movement instead of that velocity.
     */
    if (resting_contact_candidate) {
      if (
        abs_int32(body->vx_q8) <=
            PILL_RB_SLEEP_LINEAR_Q8
      ) {
        body->vx_q8 = 0;
      }
      if (
        abs_int32(body->vy_q8) <=
            PILL_RB_SLEEP_LINEAR_Q8
      ) {
        body->vy_q8 = 0;
      }
      if (
        abs_int32(body->angular_velocity) <=
            PILL_RB_SLEEP_ANGULAR
      ) {
        body->angular_velocity = 0;
      }
    }

    if (
      body->x_q8 / PILL_PHYSICS_Q8 !=
          old_x_q8[index] / PILL_PHYSICS_Q8 ||
      body->y_q8 / PILL_PHYSICS_Q8 !=
          old_y_q8[index] / PILL_PHYSICS_Q8 ||
      body->angle / PILL_PHYSICS_ANGLE_BUCKET !=
          old_angle[index] / PILL_PHYSICS_ANGLE_BUCKET
    ) {
      visual_changed = true;
    }
  }

  if (visual_changed && s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  const bool geometrically_resting =
      resting_contact_candidate &&
      maximum_rest_travel_q8 <=
          PILL_RB_REST_TRAVEL_Q8 &&
      maximum_rest_angle <=
          PILL_RB_REST_ANGLE;

  if (geometrically_resting) {
    if (s_pill_physics_quiet_frames < 255) {
      s_pill_physics_quiet_frames++;
    }
  } else {
    s_pill_physics_quiet_frames = 0;
  }

  if (s_pill_physics_quiet_frames >= PILL_RB_SLEEP_FRAMES) {
    for (
      uint8_t index = 0;
      index < s_pill_physics_body_count;
      index++
    ) {
      s_pill_physics_bodies[index].vx_q8 = 0;
      s_pill_physics_bodies[index].vy_q8 = 0;
      s_pill_physics_bodies[index].angular_velocity = 0;
    }
    return;
  }

  pill_physics_schedule_tick(
    s_alarm_active
        ? PILL_RB_ALARM_FRAME_MS
        : PILL_RB_FRAME_MS
  );
}

static void pill_physics_accel_handler(
    AccelData *data,
    uint32_t num_samples
) {
  if (!data || num_samples == 0) {
    return;
  }

  const AccelData sample = data[num_samples - 1];

  if (sample.did_vibrate) {
    return;
  }

  const int16_t target_x = sample.x;
  const int16_t target_y = (int16_t)-sample.y;
  const int16_t old_magnitude =
      pill_rb_tilt_magnitude(
        s_pill_physics_gravity_x,
        s_pill_physics_gravity_y
      );

  s_pill_physics_gravity_x = (int16_t)(
    (s_pill_physics_gravity_x + target_x * 5) / 6
  );
  s_pill_physics_gravity_y = (int16_t)(
    (s_pill_physics_gravity_y + target_y * 5) / 6
  );

  const int16_t new_magnitude =
      pill_rb_tilt_magnitude(
        s_pill_physics_gravity_x,
        s_pill_physics_gravity_y
      );
  /*
   * Hysteresis prevents filtered sensor noise around 50 mg from repeatedly
   * waking and sleeping a settled pile. Drive still starts at the 50 mg
   * deadzone; only the wake event uses the wider 40/60 mg band.
   */
  const bool entered_drive_band =
      old_magnitude <=
          PILL_RB_TILT_DEADZONE_MG +
          PILL_RB_TILT_WAKE_HYSTERESIS_MG &&
      new_magnitude >
          PILL_RB_TILT_DEADZONE_MG +
          PILL_RB_TILT_WAKE_HYSTERESIS_MG;
  const bool left_drive_band =
      old_magnitude >=
          PILL_RB_TILT_DEADZONE_MG -
          PILL_RB_TILT_WAKE_HYSTERESIS_MG &&
      new_magnitude <
          PILL_RB_TILT_DEADZONE_MG -
          PILL_RB_TILT_WAKE_HYSTERESIS_MG;
  const bool deadzone_crossed =
      entered_drive_band ||
      left_drive_band;
  const bool meaningful_change =
      deadzone_crossed ||
      abs_int32(
        (int32_t)s_pill_physics_gravity_x -
        s_pill_physics_last_target_x
      ) >= PILL_RB_SENSOR_WAKE_MG ||
      abs_int32(
        (int32_t)s_pill_physics_gravity_y -
        s_pill_physics_last_target_y
      ) >= PILL_RB_SENSOR_WAKE_MG;

  if (!meaningful_change) {
    if (s_pill_physics_sensor_quiet_samples < 255) {
      s_pill_physics_sensor_quiet_samples++;
    }
    return;
  }

  s_pill_physics_last_target_x =
      s_pill_physics_gravity_x;
  s_pill_physics_last_target_y =
      s_pill_physics_gravity_y;
  s_pill_physics_sensor_quiet_samples = 0;
  s_pill_physics_quiet_frames = 0;

  if (!s_pill_physics_timer) {
    pill_physics_update_activity();
  }
}

static void pill_physics_stop(void) {
  cancel_timer(&s_pill_physics_timer);

  if (s_pill_physics_accel_subscribed) {
    accel_data_service_unsubscribe();
    s_pill_physics_accel_subscribed = false;
  }
}

static void pill_physics_update_activity(void) {
  const bool should_run =
      s_pill_physics_window_visible &&
      !s_confirmed_screen_active &&
      !s_transfer_screen_active &&
      s_pill_physics_body_count > 0;

  if (!should_run) {
    pill_physics_stop();
    return;
  }

  if (!s_pill_physics_accel_subscribed) {
    accel_service_set_sampling_rate(
      ACCEL_SAMPLING_25HZ
    );
    accel_data_service_subscribe(
      1,
      pill_physics_accel_handler
    );
    s_pill_physics_accel_subscribed = true;
  }

  if (!s_pill_physics_timer) {
    s_pill_physics_quiet_frames = 0;
    pill_physics_schedule_tick(
      s_alarm_active
          ? PILL_RB_ALARM_FRAME_MS
          : PILL_RB_FRAME_MS
    );
  }
}

static bool scrolling_back_to_pill(void) {
#if defined(PBL_TOUCH)
  /* Das gehaltene Paar 1↔0 fährt erst bei echter Abwärtsbewegung aus. */
  if (
    s_touch.dragging &&
    s_touch.pair_selected &&
    s_touch.start_index == 1 &&
    s_touch.neighbor_index == 0 &&
    s_touch.total_delta_y > 0
  ) {
    return true;
  }
#endif

  return
      s_scroll.mode == SCROLL_SNAP &&
      s_scroll.snap_index == 0;
}

static void set_band_layer_x_q8(
    int32_t x_q8
) {
  if (!s_band_layer) {
    return;
  }

  GRect frame =
      layer_get_frame(
        s_band_layer
      );

  const int32_t rounded_x =
      x_q8 >= 0
          ? x_q8 + SCROLL_Q8 / 2
          : x_q8 - SCROLL_Q8 / 2;

  frame.origin.x =
      (int16_t)(
        rounded_x /
        SCROLL_Q8
      );

  /* Beim Ausfahren bleibt der Balken an der ersten Zeile. */
  if (!s_band.target_visible) {
    frame.origin.y =
        (int16_t)(
          current_pill_y() +
          s_frame_height +
          MEDICATION_GAP - 7 +
          HINT_POSITION_ADJUST_Y +
          HINT_HEIGHT +
          15 +
          MEDICATION_HEADER_HEIGHT
        );
  } else if (s_canvas_layer) {
    const GRect canvas_bounds =
        layer_get_bounds(
          s_canvas_layer
        );

    frame.origin.y =
        (
          canvas_bounds.size.h -
          MEDICATION_ROW_HEIGHT
        ) /
        2;
  }

  layer_set_frame(
    s_band_layer,
    frame
  );

  if (s_band_arrow_layer) {
    GRect arrow_frame =
        layer_get_frame(
          s_band_arrow_layer
        );

    arrow_frame.origin.x =
        frame.origin.x -
        BAND_ARROW_WIDTH;

    arrow_frame.origin.y =
        frame.origin.y;

    layer_set_frame(
      s_band_arrow_layer,
      arrow_frame
    );
  }
}

static void set_band_and_arrow_hidden(
    bool hidden
) {
  if (s_band_layer) {
    layer_set_hidden(
      s_band_layer,
      hidden
    );
  }

  if (s_band_arrow_layer) {
    layer_set_hidden(
      s_band_arrow_layer,
      hidden
    );
  }
}

static void schedule_band_animation(void);

static void band_animation_tick(
    void *context
) {
  s_band_animation_timer = NULL;

  if (
    !s_band.animating ||
    !s_band_layer
  ) {
    return;
  }

  int32_t force_q8 =
      (
        (
          s_band.target_x_q8 -
          s_band.x_q8
        ) *
        SCROLL_SNAP_SPRING_NUM
      ) /
      SCROLL_SNAP_SPRING_DEN;

  force_q8 = clamp_symmetric(
    force_q8,
    SCROLL_SNAP_MAX_FORCE_Q8
  );

  s_band.velocity_q8 +=
      force_q8;

  s_band.velocity_q8 =
      (
        s_band.velocity_q8 *
        SCROLL_SNAP_DAMPING_NUM
      ) /
      SCROLL_SNAP_DAMPING_DEN;

  s_band.x_q8 +=
      s_band.velocity_q8;

  const int32_t minimum_x_q8 =
      -BAND_OVERSHOOT_COVER_PX *
      SCROLL_Q8;

  if (s_band.x_q8 < minimum_x_q8) {
    s_band.x_q8 = minimum_x_q8;

    if (s_band.velocity_q8 < 0) {
      s_band.velocity_q8 = 0;
    }
  }

  set_band_layer_x_q8(
    s_band.x_q8
  );

  if (
    abs_int32(
      s_band.target_x_q8 -
      s_band.x_q8
    ) <= SCROLL_STOP_POSITION_Q8 &&
    abs_int32(
      s_band.velocity_q8
    ) <= SCROLL_STOP_VELOCITY_Q8
  ) {
    s_band.x_q8 =
        s_band.target_x_q8;

    s_band.velocity_q8 = 0;
    s_band.animating = false;

    set_band_layer_x_q8(
      s_band.x_q8
    );

    if (!s_band.target_visible) {
      set_band_and_arrow_hidden(
        true
      );
    }

    return;
  }

  layer_mark_dirty(
    s_band_layer
  );

  schedule_band_animation();
}

static void schedule_band_animation(void) {
  if (
    s_band_animation_timer ||
    !s_band.animating
  ) {
    return;
  }

  s_band_animation_timer =
      app_timer_register(
        SCROLL_FRAME_MS,
        band_animation_tick,
        NULL
      );
}

static void update_band_animation_target(void) {
  if (!s_band_layer || !s_canvas_layer) {
    return;
  }

  if (
    s_confirmed_screen_active ||
    s_transfer_screen_active
  ) {
    set_band_and_arrow_hidden(true);
    return;
  }

  const bool visible =
      s_scroll.snap_index > 0 &&
      !scrolling_back_to_pill();

  const int32_t target_x_q8 =
      visible
          ? 0
          : layer_get_bounds(s_canvas_layer).size.w * SCROLL_Q8;

  if (
    s_band.target_visible == visible &&
    s_band.target_x_q8 == target_x_q8
  ) {
    return;
  }

  s_band.target_visible = visible;
  s_band.target_x_q8 = target_x_q8;
  s_band.animating = true;

  if (visible) {
    set_band_and_arrow_hidden(false);
  }

  schedule_band_animation();
}

static void draw_icon_rounded_rect(
    GContext *ctx,
    GRect rect,
    uint16_t radius,
    GColor fill_color,
    GColor outline_color
) {
  graphics_context_set_fill_color(
    ctx,
    outline_color
  );
  graphics_fill_rect(
    ctx,
    rect,
    radius,
    GCornersAll
  );

  if (
    rect.size.w <= 4 ||
    rect.size.h <= 4
  ) {
    return;
  }

  const GRect inner = GRect(
    rect.origin.x + 2,
    rect.origin.y + 2,
    rect.size.w - 4,
    rect.size.h - 4
  );

  graphics_context_set_fill_color(
    ctx,
    fill_color
  );
  graphics_fill_rect(
    ctx,
    inner,
    radius > 2 ? radius - 2 : 0,
    GCornersAll
  );
}

static void draw_tablet_icon(
    GContext *ctx,
    GRect frame,
    const MedicationSettings *medication,
    GColor outline_color
) {
  const GColor fill_color = {
    .argb = medication->color
  };

  const int16_t center_x =
      frame.origin.x + frame.size.w / 2;
  const int16_t center_y =
      frame.origin.y + frame.size.h / 2;

  switch (medication->shape) {
    case 0:
      graphics_context_set_fill_color(
        ctx,
        outline_color
      );
      graphics_fill_circle(
        ctx,
        GPoint(center_x, center_y),
        10
      );

      graphics_context_set_fill_color(
        ctx,
        fill_color
      );
      graphics_fill_circle(
        ctx,
        GPoint(center_x, center_y),
        8
      );
      break;

    case 1:
      draw_icon_rounded_rect(
        ctx,
        GRect(
          center_x - 13,
          center_y - 9,
          26,
          18
        ),
        9,
        fill_color,
        outline_color
      );
      break;

    case 2:
      draw_icon_rounded_rect(
        ctx,
        GRect(
          center_x - 15,
          center_y - 7,
          30,
          14
        ),
        7,
        fill_color,
        outline_color
      );

      graphics_context_set_stroke_color(
        ctx,
        outline_color
      );
      graphics_context_set_stroke_width(
        ctx,
        2
      );
      graphics_draw_line(
        ctx,
        GPoint(center_x, center_y - 5),
        GPoint(center_x, center_y + 5)
      );
      break;

    case 3: {
      GPoint points[] = {
        GPoint(0, -11),
        GPoint(11, 0),
        GPoint(0, 11),
        GPoint(-11, 0)
      };

      const GPathInfo path_info = {
        .num_points = ARRAY_LENGTH(points),
        .points = points
      };

      GPath *path =
          gpath_create(&path_info);

      if (!path) {
        return;
      }

      gpath_move_to(
        path,
        GPoint(center_x, center_y)
      );

      graphics_context_set_fill_color(
        ctx,
        fill_color
      );
      graphics_context_set_stroke_color(
        ctx,
        outline_color
      );
      graphics_context_set_stroke_width(
        ctx,
        2
      );

      gpath_draw_filled(ctx, path);
      gpath_draw_outline(ctx, path);
      gpath_destroy(path);
      break;
    }
  }

  graphics_context_set_stroke_width(ctx, 1);
}

static void draw_pen_icon(
    GContext *ctx,
    GRect frame,
    const MedicationSettings *medication,
    GColor outline_color
) {
  const GColor fill_color = {
    .argb = medication->color
  };

  const GPoint barrel_start = GPoint(
    frame.origin.x + 7,
    frame.origin.y + frame.size.h - 7
  );

  const GPoint barrel_end = GPoint(
    frame.origin.x + frame.size.w - 9,
    frame.origin.y + 8
  );

  graphics_context_set_stroke_color(
    ctx,
    outline_color
  );
  graphics_context_set_stroke_width(
    ctx,
    8
  );
  graphics_draw_line(
    ctx,
    barrel_start,
    barrel_end
  );

  graphics_context_set_stroke_color(
    ctx,
    fill_color
  );
  graphics_context_set_stroke_width(
    ctx,
    5
  );
  graphics_draw_line(
    ctx,
    barrel_start,
    barrel_end
  );

  graphics_context_set_stroke_color(
    ctx,
    outline_color
  );
  graphics_context_set_stroke_width(
    ctx,
    2
  );

  graphics_draw_line(
    ctx,
    GPoint(
      barrel_start.x - 4,
      barrel_start.y - 4
    ),
    GPoint(
      barrel_start.x + 3,
      barrel_start.y + 3
    )
  );

  graphics_draw_line(
    ctx,
    barrel_end,
    GPoint(
      frame.origin.x + frame.size.w - 2,
      frame.origin.y + 1
    )
  );

  graphics_context_set_stroke_width(ctx, 1);
}

static void draw_medication_icon(
    GContext *ctx,
    GRect frame,
    const MedicationSettings *medication,
    GColor outline_color
) {
  if (
    !medication ||
    !medication->icon_set
  ) {
    return;
  }

  if (
    medication->symbol ==
        MEDICATION_SYMBOL_PEN
  ) {
    draw_pen_icon(
      ctx,
      frame,
      medication,
      outline_color
    );
    return;
  }

  draw_tablet_icon(
    ctx,
    frame,
    medication,
    outline_color
  );
}

static void draw_physics_round_line(
    GContext *ctx,
    GPoint start,
    GPoint end,
    uint8_t radius,
    GColor color
) {
  /*
   * The previous renderer filled one circle for every pixel along the line.
   * A thick native line plus two end caps produces the same rounded capsule
   * with only three drawing primitives.
   */
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(
    ctx,
    (uint8_t)(radius * 2)
  );
  graphics_draw_line(ctx, start, end);

  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, start, radius);
  graphics_fill_circle(ctx, end, radius);
  graphics_context_set_stroke_width(ctx, 1);
}

static void draw_physics_capsule(
    GContext *ctx,
    GPoint center,
    int32_t angle,
    int16_t half_length,
    uint8_t radius,
    GColor fill_color,
    GColor outline_color,
    bool draw_divider
) {
  const int16_t dx = (int16_t)(
    ((int32_t)cos_lookup(angle) *
     half_length) /
    TRIG_MAX_RATIO
  );
  const int16_t dy = (int16_t)(
    ((int32_t)sin_lookup(angle) *
     half_length) /
    TRIG_MAX_RATIO
  );
  const GPoint start = GPoint(
    center.x - dx,
    center.y - dy
  );
  const GPoint end = GPoint(
    center.x + dx,
    center.y + dy
  );

  draw_physics_round_line(
    ctx,
    start,
    end,
    radius + 2,
    outline_color
  );
  draw_physics_round_line(
    ctx,
    start,
    end,
    radius,
    fill_color
  );

  if (draw_divider) {
    const int32_t divider_angle =
        angle + TRIG_MAX_ANGLE / 4;
    const int16_t divider_dx = (int16_t)(
      ((int32_t)cos_lookup(divider_angle) *
       (radius - 1)) /
      TRIG_MAX_RATIO
    );
    const int16_t divider_dy = (int16_t)(
      ((int32_t)sin_lookup(divider_angle) *
       (radius - 1)) /
      TRIG_MAX_RATIO
    );

    draw_physics_round_line(
      ctx,
      GPoint(
        center.x - divider_dx,
        center.y - divider_dy
      ),
      GPoint(
        center.x + divider_dx,
        center.y + divider_dy
      ),
      1,
      outline_color
    );
  }
}

static uint8_t medication_appearance_size(
    const MedicationAppearance *appearance
) {
  return
      appearance &&
      appearance->valid &&
      appearance->size >= 60 &&
      appearance->size <= 140
          ? appearance->size
          : 100;
}

static int16_t medication_appearance_scaled(
    int16_t base,
    uint8_t size
) {
  const int16_t scaled = (int16_t)(
    ((int32_t)base * size + 50) / 100
  );

  return scaled < 1 ? 1 : scaled;
}

static void medication_appearance_geometry(
    const MedicationAppearance *appearance,
    int16_t *line_half,
    int16_t *radius,
    int16_t *diamond_half
) {
  const uint8_t size =
      medication_appearance_size(appearance);
  int16_t local_line_half = 0;
  int16_t local_radius = 10;
  int16_t local_diamond_half = 0;

  switch (appearance ? appearance->shape : 0) {
    case 1:
      /*
       * Same height and curvature, but about 50 % more total width.
       * Width changes from 2 * (8 + 13) = 42 px to
       * 2 * (19 + 13) = 64 px. The phone percentage is still
       * applied afterwards as before.
       */
      local_line_half = 19;
      local_radius = 13;
      break;
    case 2:
      local_line_half = 8;
      local_radius = 7;
      break;
    case 3:
      local_radius = 0;
      local_diamond_half = 11;
      break;
    case 4:
      /*
       * 20 % thinner while keeping the same total capsule length:
       * old half extent 23 + 15 = 38 px
       * new half extent 26 + 12 = 38 px
       * The phone size percentage is still applied afterwards.
       */
      local_line_half = 26;
      local_radius = 12;
      break;
    case 0:
    default:
      break;
  }

  if (line_half) {
    *line_half = medication_appearance_scaled(local_line_half, size);
  }
  if (radius) {
    *radius = medication_appearance_scaled(local_radius, size);
  }
  if (diamond_half) {
    *diamond_half = medication_appearance_scaled(local_diamond_half, size);
  }
}

static void draw_physics_capsule_pixel_run(
    GContext *ctx,
    int16_t y,
    int16_t start_x,
    int16_t end_x,
    uint8_t color_index,
    GColor outline_color,
    GColor first_color,
    GColor second_color
) {
  if (
    color_index == 0 ||
    end_x < start_x
  ) {
    return;
  }

  GColor color = outline_color;

  if (color_index == 2) {
    color = first_color;
  } else if (color_index == 3) {
    color = second_color;
  }

  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(
    ctx,
    GPoint(start_x, y),
    GPoint(end_x, y)
  );
}

static void draw_physics_two_color_capsule(
    GContext *ctx,
    GPoint center,
    int32_t angle,
    int16_t half_length,
    uint8_t radius,
    GColor first_color,
    GColor second_color,
    GColor outline_color
) {
  /*
   * Exact capsule geometry:
   * all pixels whose distance to the center line segment is <= radius.
   * The inner capsule uses exactly the same geometry with a radius reduced
   * by two pixels. This doubles the black outer outline while keeping it
   * equally thick on straight tangent sections and circular end arcs.
   */
  const int32_t cosine = cos_lookup(angle);
  const int32_t sine = sin_lookup(angle);
  const int32_t absolute_cosine = abs_int32(cosine);
  const int32_t absolute_sine = abs_int32(sine);

  /*
   * The center segment rotates, but each end cap is a circle. A circle has
   * the full radius on both screen axes at every angle. Multiplying the
   * radius by sine/cosine clipped horizontal capsules at left/right and
   * vertical capsules at top/bottom.
   */
  const int16_t extent_x = (int16_t)(
    (absolute_cosine * half_length) /
    TRIG_MAX_RATIO +
    radius +
    2
  );
  const int16_t extent_y = (int16_t)(
    (absolute_sine * half_length) /
    TRIG_MAX_RATIO +
    radius +
    2
  );

  const int16_t minimum_x =
      (int16_t)(center.x - extent_x);
  const int16_t maximum_x =
      (int16_t)(center.x + extent_x);
  const int16_t minimum_y =
      (int16_t)(center.y - extent_y);
  const int16_t maximum_y =
      (int16_t)(center.y + extent_y);

  const int32_t cosine_q8 =
      (cosine * PILL_PHYSICS_Q8) /
      TRIG_MAX_RATIO;
  const int32_t sine_q8 =
      (sine * PILL_PHYSICS_Q8) /
      TRIG_MAX_RATIO;
  const int32_t half_length_q8 =
      half_length * PILL_PHYSICS_Q8;
  const int32_t outer_radius_q8 =
      radius * PILL_PHYSICS_Q8;
  const int32_t inner_radius_q8 =
      radius > 2
          ? (radius - 2) * PILL_PHYSICS_Q8
          : 0;
  const int32_t divider_half_width_q8 =
      PILL_PHYSICS_Q8 / 2;
  const int32_t outer_radius_squared =
      outer_radius_q8 * outer_radius_q8;
  const int32_t inner_radius_squared =
      inner_radius_q8 * inner_radius_q8;

  for (int16_t y = minimum_y; y <= maximum_y; y++) {
    const int32_t relative_y = y - center.y;
    const int32_t first_relative_x =
        minimum_x - center.x;

    int32_t local_x_q8 =
        first_relative_x * cosine_q8 +
        relative_y * sine_q8;
    int32_t local_y_q8 =
        -first_relative_x * sine_q8 +
        relative_y * cosine_q8;

    uint8_t run_color = 0;
    int16_t run_start_x = minimum_x;

    for (int16_t x = minimum_x; x <= maximum_x; x++) {
      int32_t nearest_x_q8 = local_x_q8;

      if (nearest_x_q8 < -half_length_q8) {
        nearest_x_q8 = -half_length_q8;
      } else if (nearest_x_q8 > half_length_q8) {
        nearest_x_q8 = half_length_q8;
      }

      const int32_t distance_x_q8 =
          local_x_q8 - nearest_x_q8;
      const int32_t distance_squared =
          distance_x_q8 * distance_x_q8 +
          local_y_q8 * local_y_q8;

      uint8_t pixel_color = 0;

      if (distance_squared <= outer_radius_squared) {
        if (
          distance_squared > inner_radius_squared ||
          abs_int32(local_x_q8) <=
              divider_half_width_q8
        ) {
          pixel_color = 1;
        } else {
          pixel_color =
              local_x_q8 < 0 ? 2 : 3;
        }
      }

      if (pixel_color != run_color) {
        draw_physics_capsule_pixel_run(
          ctx,
          y,
          run_start_x,
          (int16_t)(x - 1),
          run_color,
          outline_color,
          first_color,
          second_color
        );

        run_color = pixel_color;
        run_start_x = x;
      }

      local_x_q8 += cosine_q8;
      local_y_q8 -= sine_q8;
    }

    draw_physics_capsule_pixel_run(
      ctx,
      y,
      run_start_x,
      maximum_x,
      run_color,
      outline_color,
      first_color,
      second_color
    );
  }
}

static void draw_physics_appearance_diamond(
    GContext *ctx,
    GPoint center,
    int32_t angle,
    int16_t half_size,
    GColor fill_color,
    GColor outline_color
) {
  GPoint outer_points[4];
  GPoint inner_points[4];
  const int16_t inner_half =
      half_size > 3 ? half_size - 3 : 1;

  for (uint8_t index = 0; index < 4; index++) {
    const int32_t point_angle =
        angle + ((int32_t)index * TRIG_MAX_ANGLE) / 4;
    outer_points[index] = GPoint(
      center.x + (int16_t)(
        ((int32_t)cos_lookup(point_angle) * half_size) /
        TRIG_MAX_RATIO
      ),
      center.y + (int16_t)(
        ((int32_t)sin_lookup(point_angle) * half_size) /
        TRIG_MAX_RATIO
      )
    );
    inner_points[index] = GPoint(
      center.x + (int16_t)(
        ((int32_t)cos_lookup(point_angle) * inner_half) /
        TRIG_MAX_RATIO
      ),
      center.y + (int16_t)(
        ((int32_t)sin_lookup(point_angle) * inner_half) /
        TRIG_MAX_RATIO
      )
    );
  }

  const GPathInfo outer_info = {
    .num_points = ARRAY_LENGTH(outer_points),
    .points = outer_points
  };
  const GPathInfo inner_info = {
    .num_points = ARRAY_LENGTH(inner_points),
    .points = inner_points
  };
  GPath *outer = gpath_create(&outer_info);
  GPath *inner = gpath_create(&inner_info);

  if (!outer || !inner) {
    if (outer) {
      gpath_destroy(outer);
    }
    if (inner) {
      gpath_destroy(inner);
    }
    return;
  }

  graphics_context_set_fill_color(ctx, outline_color);
  gpath_draw_filled(ctx, outer);
  graphics_context_set_fill_color(ctx, fill_color);
  gpath_draw_filled(ctx, inner);
  gpath_destroy(outer);
  gpath_destroy(inner);
}

static int16_t medication_appearance_brightness(GColor color) {
  const uint8_t value = color.argb;
  const uint8_t red = (value >> 4) & 3;
  const uint8_t green = (value >> 2) & 3;
  const uint8_t blue = value & 3;

  return red * 30 + green * 59 + blue * 11;
}

#define PILL_IMPRINT_GLYPH_WIDTH 5
#define PILL_IMPRINT_GLYPH_HEIGHT 7
#define PILL_IMPRINT_GLYPH_GAP 1
#define PILL_IMPRINT_MAX_SCALE 4

static const uint8_t s_pill_imprint_digits[10][7] = {
  { 14, 17, 19, 21, 25, 17, 14 },
  { 4, 12, 4, 4, 4, 4, 14 },
  { 14, 17, 1, 2, 4, 8, 31 },
  { 30, 1, 1, 14, 1, 1, 30 },
  { 2, 6, 10, 18, 31, 2, 2 },
  { 31, 16, 16, 30, 1, 1, 30 },
  { 14, 16, 16, 30, 17, 17, 14 },
  { 31, 1, 2, 4, 8, 8, 8 },
  { 14, 17, 17, 14, 17, 17, 14 },
  { 14, 17, 17, 15, 1, 1, 14 }
};

static const uint8_t s_pill_imprint_letters[26][7] = {
  { 14, 17, 17, 31, 17, 17, 17 },
  { 30, 17, 17, 30, 17, 17, 30 },
  { 14, 17, 16, 16, 16, 17, 14 },
  { 30, 17, 17, 17, 17, 17, 30 },
  { 31, 16, 16, 30, 16, 16, 31 },
  { 31, 16, 16, 30, 16, 16, 16 },
  { 14, 17, 16, 23, 17, 17, 15 },
  { 17, 17, 17, 31, 17, 17, 17 },
  { 14, 4, 4, 4, 4, 4, 14 },
  { 7, 2, 2, 2, 2, 18, 12 },
  { 17, 18, 20, 24, 20, 18, 17 },
  { 16, 16, 16, 16, 16, 16, 31 },
  { 17, 27, 21, 21, 17, 17, 17 },
  { 17, 25, 21, 19, 17, 17, 17 },
  { 14, 17, 17, 17, 17, 17, 14 },
  { 30, 17, 17, 30, 16, 16, 16 },
  { 14, 17, 17, 17, 21, 18, 13 },
  { 30, 17, 17, 30, 20, 18, 17 },
  { 15, 16, 16, 14, 1, 1, 30 },
  { 31, 4, 4, 4, 4, 4, 4 },
  { 17, 17, 17, 17, 17, 17, 14 },
  { 17, 17, 17, 17, 17, 10, 4 },
  { 17, 17, 17, 21, 21, 21, 10 },
  { 17, 17, 10, 4, 10, 17, 17 },
  { 17, 17, 10, 4, 4, 4, 4 },
  { 31, 1, 2, 4, 8, 16, 31 }
};

static const uint8_t s_pill_imprint_unknown[7] = {
  14, 17, 1, 2, 4, 0, 4
};
static const uint8_t s_pill_imprint_bar[7] = {
  4, 4, 4, 4, 4, 4, 4
};
static const uint8_t s_pill_imprint_dash[7] = {
  0, 0, 0, 31, 0, 0, 0
};
static const uint8_t s_pill_imprint_slash[7] = {
  1, 2, 2, 4, 8, 8, 16
};
static const uint8_t s_pill_imprint_plus[7] = {
  0, 4, 4, 31, 4, 4, 0
};
static const uint8_t s_pill_imprint_dot[7] = {
  0, 0, 0, 0, 0, 6, 6
};
static const uint8_t s_pill_imprint_space[7] = {
  0, 0, 0, 0, 0, 0, 0
};

static const uint8_t *pill_physics_imprint_glyph(
    char character
) {
  if (character >= '0' && character <= '9') {
    return s_pill_imprint_digits[character - '0'];
  }

  if (character >= 'a' && character <= 'z') {
    character = (char)(character - 'a' + 'A');
  }

  if (character >= 'A' && character <= 'Z') {
    return s_pill_imprint_letters[character - 'A'];
  }

  switch (character) {
    case '|':
      return s_pill_imprint_bar;
    case '-':
      return s_pill_imprint_dash;
    case '/':
      return s_pill_imprint_slash;
    case '+':
      return s_pill_imprint_plus;
    case '.':
      return s_pill_imprint_dot;
    case ' ':
      return s_pill_imprint_space;
    default:
      return s_pill_imprint_unknown;
  }
}

static GPoint pill_physics_imprint_rotated_point(
    GPoint center,
    int32_t angle,
    int16_t local_x,
    int16_t local_y
) {
  const int32_t cosine = cos_lookup(angle);
  const int32_t sine = sin_lookup(angle);

  return GPoint(
    center.x + (int16_t)(
      ((int32_t)local_x * cosine -
       (int32_t)local_y * sine) /
      TRIG_MAX_RATIO
    ),
    center.y + (int16_t)(
      ((int32_t)local_x * sine +
       (int32_t)local_y * cosine) /
      TRIG_MAX_RATIO
    )
  );
}

static void draw_physics_appearance_imprint(
    GContext *ctx,
    GPoint center,
    int32_t angle,
    const MedicationAppearance *appearance,
    int16_t body_width,
    int16_t body_height
) {
  if (
    !appearance ||
    appearance->imprint[0] == '\0' ||
    body_width < 8 ||
    body_height < 8
  ) {
    return;
  }

  uint8_t character_count = 0;
  while (
    character_count < sizeof(appearance->imprint) &&
    appearance->imprint[character_count] != '\0'
  ) {
    character_count++;
  }

  if (character_count == 0) {
    return;
  }

  const int16_t unscaled_width = (int16_t)(
    character_count * PILL_IMPRINT_GLYPH_WIDTH +
    (character_count - 1) * PILL_IMPRINT_GLYPH_GAP
  );

  int16_t scale_from_width =
      (body_width - 4) / unscaled_width;
  int16_t scale_from_height =
      (body_height - 4) / PILL_IMPRINT_GLYPH_HEIGHT;
  int16_t scale =
      scale_from_width < scale_from_height
          ? scale_from_width
          : scale_from_height;

  if (scale < 1) {
    scale = 1;
  } else if (scale > PILL_IMPRINT_MAX_SCALE) {
    scale = PILL_IMPRINT_MAX_SCALE;
  }

  const int16_t text_width =
      unscaled_width * scale;
  const int16_t text_height =
      PILL_IMPRINT_GLYPH_HEIGHT * scale;
  const int16_t text_left =
      (int16_t)(-text_width / 2);
  const int16_t text_top =
      (int16_t)(-text_height / 2);

  const GColor first_color = {
    .argb = appearance->primary_color
  };
  int16_t brightness =
      medication_appearance_brightness(first_color);

  if (appearance->shape == 4) {
    const GColor second_color = {
      .argb = appearance->secondary_color
    };
    brightness = (int16_t)(
      (brightness + medication_appearance_brightness(second_color)) / 2
    );
  }

  /*
   * Imprints are deliberately grey rather than black/white:
   * dark grey on bright tablets, light grey on dark tablets.
   */
  graphics_context_set_fill_color(
    ctx,
    brightness >= 165 ? GColorDarkGray : GColorLightGray
  );

  for (
    uint8_t character_index = 0;
    character_index < character_count;
    character_index++
  ) {
    const uint8_t *glyph =
        pill_physics_imprint_glyph(
          appearance->imprint[character_index]
        );
    const int16_t character_x = (int16_t)(
      text_left +
      character_index *
          (PILL_IMPRINT_GLYPH_WIDTH + PILL_IMPRINT_GLYPH_GAP) *
          scale
    );

    for (
      uint8_t row = 0;
      row < PILL_IMPRINT_GLYPH_HEIGHT;
      row++
    ) {
      for (
        uint8_t column = 0;
        column < PILL_IMPRINT_GLYPH_WIDTH;
        column++
      ) {
        if (
          !(glyph[row] &
            (1 << (PILL_IMPRINT_GLYPH_WIDTH - 1 - column)))
        ) {
          continue;
        }

        const int16_t local_x = (int16_t)(
          character_x + column * scale + scale / 2
        );
        const int16_t local_y = (int16_t)(
          text_top + row * scale + scale / 2
        );
        const GPoint pixel_center =
            pill_physics_imprint_rotated_point(
              center,
              angle,
              local_x,
              local_y
            );
        const int16_t pixel_half = scale / 2;

        /*
         * Filled pixel blocks are reliable even for short glyph strokes.
         * Their positions rotate with the pill; unlike graphics_draw_text(),
         * the complete imprint therefore follows body->angle.
         */
        graphics_fill_rect(
          ctx,
          GRect(
            pixel_center.x - pixel_half,
            pixel_center.y - pixel_half,
            scale,
            scale
          ),
          0,
          GCornerNone
        );
      }
    }
  }

}

#define PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS 8
#define PHYSICS_ROUNDED_OVAL_PATH_POINTS \
  (PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS * 4)
#define PHYSICS_ROUNDED_OVAL_OUTLINE_PX 2
#define PHYSICS_ROUNDED_OVAL_Q12 4096

/*
 * Longer cubic control arms mean larger visual radii:
 * - 82 % at the left/right ends makes them much rounder.
 * - 76 % along the top/bottom keeps those arcs noticeably flatter.
 */
#define PHYSICS_ROUNDED_OVAL_END_CONTROL_NUM 81
#define PHYSICS_ROUNDED_OVAL_TOP_CONTROL_NUM 57
#define PHYSICS_ROUNDED_OVAL_CONTROL_DEN 100

static int16_t pill_physics_cubic_component(
    int16_t p0,
    int16_t p1,
    int16_t p2,
    int16_t p3,
    int32_t t_q12
) {
  const int64_t scale = PHYSICS_ROUNDED_OVAL_Q12;
  const int64_t t = t_q12;
  const int64_t u = scale - t;
  const int64_t scale_cubed = scale * scale * scale;

  const int64_t value =
      u * u * u * p0 +
      3 * u * u * t * p1 +
      3 * u * t * t * p2 +
      t * t * t * p3;

  if (value >= 0) {
    return (int16_t)(
      (value + scale_cubed / 2) /
      scale_cubed
    );
  }

  return (int16_t)(
    -((-value + scale_cubed / 2) /
      scale_cubed)
  );
}

static void pill_physics_rounded_oval_quadrant(
    GPoint *points,
    uint8_t point_offset,
    int16_t p0_x,
    int16_t p0_y,
    int16_t p1_x,
    int16_t p1_y,
    int16_t p2_x,
    int16_t p2_y,
    int16_t p3_x,
    int16_t p3_y
) {
  for (
    uint8_t step = 0;
    step < PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS;
    step++
  ) {
    /*
     * The endpoint belongs to the following quadrant. Omitting it here keeps
     * every vertex unique while the final GPath closes the last gap itself.
     */
    const int32_t t_q12 = (int32_t)(
      ((int64_t)step * PHYSICS_ROUNDED_OVAL_Q12) /
      PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS
    );

    points[point_offset + step] = GPoint(
      pill_physics_cubic_component(
        p0_x,
        p1_x,
        p2_x,
        p3_x,
        t_q12
      ),
      pill_physics_cubic_component(
        p0_y,
        p1_y,
        p2_y,
        p3_y,
        t_q12
      )
    );
  }
}

static void draw_physics_rounded_oval_fill(
    GContext *ctx,
    GPoint center,
    int32_t angle,
    int16_t half_width,
    int16_t half_height,
    GColor color
) {
  if (half_width < 2 || half_height < 2) {
    return;
  }

  const int16_t end_control = (int16_t)(
    ((int32_t)half_height *
     PHYSICS_ROUNDED_OVAL_END_CONTROL_NUM +
     PHYSICS_ROUNDED_OVAL_CONTROL_DEN / 2) /
    PHYSICS_ROUNDED_OVAL_CONTROL_DEN
  );
  const int16_t top_control = (int16_t)(
    ((int32_t)half_width *
     PHYSICS_ROUNDED_OVAL_TOP_CONTROL_NUM +
     PHYSICS_ROUNDED_OVAL_CONTROL_DEN / 2) /
    PHYSICS_ROUNDED_OVAL_CONTROL_DEN
  );

  GPoint points[PHYSICS_ROUNDED_OVAL_PATH_POINTS];

  /* Right end -> top. */
  pill_physics_rounded_oval_quadrant(
    points,
    0,
    half_width,
    0,
    half_width,
    (int16_t)-end_control,
    top_control,
    (int16_t)-half_height,
    0,
    (int16_t)-half_height
  );

  /* Top -> left end. */
  pill_physics_rounded_oval_quadrant(
    points,
    PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS,
    0,
    (int16_t)-half_height,
    (int16_t)-top_control,
    (int16_t)-half_height,
    (int16_t)-half_width,
    (int16_t)-end_control,
    (int16_t)-half_width,
    0
  );

  /* Left end -> bottom. */
  pill_physics_rounded_oval_quadrant(
    points,
    PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS * 2,
    (int16_t)-half_width,
    0,
    (int16_t)-half_width,
    end_control,
    (int16_t)-top_control,
    half_height,
    0,
    half_height
  );

  /* Bottom -> right end. */
  pill_physics_rounded_oval_quadrant(
    points,
    PHYSICS_ROUNDED_OVAL_QUADRANT_STEPS * 3,
    0,
    half_height,
    top_control,
    half_height,
    half_width,
    end_control,
    half_width,
    0
  );

  const GPathInfo path_info = {
    .num_points = PHYSICS_ROUNDED_OVAL_PATH_POINTS,
    .points = points
  };
  GPath *path = gpath_create(&path_info);

  if (!path) {
    return;
  }

  gpath_move_to(path, center);
  gpath_rotate_to(path, angle);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_physics_true_ellipse(
    GContext *ctx,
    GPoint center,
    int32_t angle,
    int16_t line_half,
    int16_t radius,
    GColor fill_color,
    GColor outline_color
) {
  const int16_t inner_half_width =
      line_half + radius;
  const int16_t inner_half_height = radius;

  /*
   * The same flatter, strongly rounded outline is drawn twice. The larger
   * black shape leaves the established two-pixel border around the tablet.
   */
  draw_physics_rounded_oval_fill(
    ctx,
    center,
    angle,
    inner_half_width + PHYSICS_ROUNDED_OVAL_OUTLINE_PX,
    inner_half_height + PHYSICS_ROUNDED_OVAL_OUTLINE_PX,
    outline_color
  );
  draw_physics_rounded_oval_fill(
    ctx,
    center,
    angle,
    inner_half_width,
    inner_half_height,
    fill_color
  );
}

static void draw_physics_pill_body(
    GContext *ctx,
    const PillPhysicsBody *body,
    int32_t arena_y,
    GColor outline_color
) {
  if (
    !body ||
    body->medication_index >= s_medication_count
  ) {
    return;
  }

  const MedicationSettings *medication =
      &s_medications[body->medication_index];
  const MedicationAppearance *stored =
      body->medication_index < s_medication_appearance_count
          ? &s_medication_appearances[body->medication_index]
          : NULL;
  MedicationAppearance fallback = {
    .valid = true,
    .shape = medication->shape <= 3 ? medication->shape : 0,
    .primary_color = medication->color,
    .secondary_color = medication->color,
    .size = 100,
    .imprint = { 0 }
  };
  const MedicationAppearance *appearance =
      stored && stored->valid ? stored : &fallback;
  const GColor fill_color = {
    .argb = appearance->primary_color
  };
  const GColor second_color = {
    .argb = appearance->secondary_color
  };
  const GPoint center = GPoint(
    (int16_t)(body->x_q8 / PILL_PHYSICS_Q8),
    (int16_t)(arena_y + body->y_q8 / PILL_PHYSICS_Q8)
  );
  int16_t line_half;
  int16_t radius;
  int16_t diamond_half;

  medication_appearance_geometry(
    appearance,
    &line_half,
    &radius,
    &diamond_half
  );

  switch (appearance->shape) {
    case 1:
      draw_physics_true_ellipse(
        ctx,
        center,
        body->angle,
        line_half,
        radius,
        fill_color,
        outline_color
      );
      break;

    case 2:
      draw_physics_capsule(
        ctx,
        center,
        body->angle,
        line_half,
        (uint8_t)radius,
        fill_color,
        outline_color,
        true
      );
      break;

    case 3:
      draw_physics_appearance_diamond(
        ctx,
        center,
        body->angle,
        diamond_half,
        fill_color,
        outline_color
      );
      break;

    case 4:
      draw_physics_two_color_capsule(
        ctx,
        center,
        body->angle,
        line_half,
        (uint8_t)radius,
        fill_color,
        second_color,
        outline_color
      );
      break;

    case 0:
    default:
      graphics_context_set_fill_color(ctx, outline_color);
      graphics_fill_circle(
        ctx,
        center,
        (uint16_t)(radius + 2)
      );
      graphics_context_set_fill_color(ctx, fill_color);
      graphics_fill_circle(
        ctx,
        center,
        (uint16_t)radius
      );
      break;
  }

  const int16_t body_width =
      diamond_half > 0
          ? diamond_half * 2
          : (line_half + radius) * 2;
  const int16_t body_height =
      diamond_half > 0
          ? diamond_half * 2
          : radius * 2;

  draw_physics_appearance_imprint(
    ctx,
    center,
    body->angle,
    appearance,
    body_width,
    body_height
  );
}

static void draw_physics_pills(
    GContext *ctx,
    GRect bounds,
    int32_t arena_y
) {
  if (
    arena_y <= -s_frame_height ||
    arena_y >= bounds.size.h
  ) {
    return;
  }

  for (
    uint8_t index = 0;
    index < s_pill_physics_body_count;
    index++
  ) {
    draw_physics_pill_body(
      ctx,
      &s_pill_physics_bodies[index],
      arena_y,
      GColorLightGray
    );
  }
}

static void draw_medications(
    GContext *ctx,
    GRect bounds,
    int32_t pill_y,
    GColor text_color
) {
  const int32_t label_y =
      pill_y +
      s_frame_height +
      MEDICATION_GAP - 7 +
      HINT_POSITION_ADJUST_Y +
      HINT_HEIGHT +
      15;

  const int32_t rows_y = label_y + MEDICATION_HEADER_HEIGHT;

  if (
    rows_y +
        LIST_ROW_COUNT *
            (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP) < 0 ||
    label_y > bounds.size.h
  ) {
    return;
  }

  graphics_context_set_text_color(ctx, text_color);

  graphics_draw_text(
    ctx,
    "MEDICATIONS",
    s_header_font,
    GRect(
      bounds.origin.x + 10,
      (int16_t)label_y,
      bounds.size.w - 20,
      MEDICATION_HEADER_HEIGHT
    ),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL
  );

  for (int index = 0; index < LIST_ROW_COUNT; index++) {
    const int32_t row_y =
        rows_y +
        index * (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP);

    if (
      row_y + MEDICATION_ROW_HEIGHT < 0 ||
      row_y > bounds.size.h
    ) {
      continue;
    }

    GRect text_frame = GRect(
      bounds.origin.x + 8,
      (int16_t)row_y + 3,
      bounds.size.w - 16,
      MEDICATION_ROW_HEIGHT - 3
    );

    GTextAlignment alignment =
        GTextAlignmentCenter;

    if (
      s_row_kinds[index] ==
          MEDICATION_ROW_ITEM
    ) {
      const int8_t medication_index =
          s_row_medication_indices[index];

      if (
        medication_index >= 0 &&
        medication_index <
            (int8_t)s_medication_count
      ) {
        draw_medication_icon(
          ctx,
          GRect(
            bounds.origin.x +
                MEDICATION_ICON_LEFT,
            (int16_t)row_y +
                (
                  MEDICATION_ROW_HEIGHT -
                  MEDICATION_ICON_SIZE
                ) /
                2,
            MEDICATION_ICON_SIZE,
            MEDICATION_ICON_SIZE
          ),
          &s_medications[medication_index],
          text_color
        );

        text_frame = GRect(
          bounds.origin.x +
              MEDICATION_ICON_TEXT_X,
          (int16_t)row_y + 3,
          bounds.size.w -
              MEDICATION_ICON_TEXT_X -
              MEDICATION_ICON_TEXT_RIGHT,
          MEDICATION_ROW_HEIGHT - 3
        );

        alignment = GTextAlignmentLeft;
      }
    }

    graphics_draw_text(
      ctx,
      s_rows[index],
      s_medication_font,
      text_frame,
      GTextOverflowModeTrailingEllipsis,
      alignment,
      NULL
    );
  }
}

static void draw_confirmation_circle(
    GContext *ctx,
    GRect bounds
) {
  if (s_confirm_radius <= 0) {
    return;
  }

  graphics_context_set_fill_color(ctx, GColorGreen);
  graphics_fill_circle(
    ctx,
    GPoint(
      bounds.size.w + CONFIRM_CENTER_OUTSIDE_X,
      bounds.size.h / 2
    ),
    (uint16_t)s_confirm_radius
  );
}

static int16_t current_check_stroke_radius(void) {
  const int16_t radius =
      ((int32_t)CHECK_STROKE_RADIUS * s_check_size +
       CHECK_POP_SETTLE_SIZE / 2) /
      CHECK_POP_SETTLE_SIZE;

  return radius < 1 ? 1 : radius;
}

static void draw_round_line(
    GContext *ctx,
    GPoint start,
    GPoint end
) {
  const int16_t dx = end.x - start.x;
  const int16_t dy = end.y - start.y;
  const int16_t abs_dx = dx < 0 ? -dx : dx;
  const int16_t abs_dy = dy < 0 ? -dy : dy;
  const int16_t steps = abs_dx > abs_dy ? abs_dx : abs_dy;
  const int16_t radius = current_check_stroke_radius();

  if (steps <= 0) {
    graphics_fill_circle(ctx, start, radius);
    return;
  }

  for (int16_t step = 0; step <= steps; step++) {
    graphics_fill_circle(
      ctx,
      GPoint(
        start.x + ((int32_t)dx * step) / steps,
        start.y + ((int32_t)dy * step) / steps
      ),
      radius
    );
  }
}

static void draw_confirmation_checkmark(
    GContext *ctx,
    GRect bounds
) {
  if (s_check_state == CHECK_HIDDEN || s_check_size <= 0) {
    return;
  }

  const int16_t center_x = bounds.size.w / 2;
  const int16_t center_y = bounds.size.h / 2;
  const int16_t size = s_check_size;

  const GPoint start = GPoint(
    center_x - (size * 42) / 100,
    center_y
  );

  const GPoint middle = GPoint(
    center_x - (size * 10) / 100,
    center_y + (size * 28) / 100
  );

  const GPoint end = GPoint(
    center_x + (size * 45) / 100,
    center_y - (size * 30) / 100
  );

  graphics_context_set_fill_color(ctx, GColorWhite);
  draw_round_line(ctx, start, middle);
  draw_round_line(ctx, middle, end);
}

static void canvas_update_proc(
    Layer *layer,
    GContext *ctx
) {
  const GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(
    ctx,
    theme_background_color()
  );
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (!s_sheet) {
    return;
  }

  const int32_t pill_y = current_pill_y();

  draw_physics_pills(ctx, bounds, pill_y);
  draw_scroll_hint(ctx, bounds, pill_y);
  draw_medications(
    ctx,
    bounds,
    pill_y,
    theme_foreground_color()
  );
}

static void band_arrow_update_proc(
    Layer *layer,
    GContext *ctx
) {
  const GRect bounds =
      layer_get_bounds(layer);

  const int16_t center_y =
      bounds.size.h / 2;

  graphics_context_set_stroke_color(
    ctx,
    theme_foreground_color()
  );

  for (
    int16_t y = 0;
    y < bounds.size.h;
    y++
  ) {
    const int16_t distance =
        y <= center_y
            ? center_y - y
            : y - center_y;

    const int16_t start_x =
        (
          (
            int32_t
          )distance *
          (bounds.size.w - 1)
        ) /
        center_y;

    graphics_draw_line(
      ctx,
      GPoint(
        start_x,
        y
      ),
      GPoint(
        bounds.size.w - 1,
        y
      )
    );
  }
}

static bool first_unconfirmed_due_symbol(
    MedicationSymbol *symbol
) {
  if (
    medication_group_is_due(
      MEDICATION_SYMBOL_PILL
    )
  ) {
    if (symbol) {
      *symbol = MEDICATION_SYMBOL_PILL;
    }
    return true;
  }

  if (
    medication_group_is_due(
      MEDICATION_SYMBOL_PEN
    )
  ) {
    if (symbol) {
      *symbol = MEDICATION_SYMBOL_PEN;
    }
    return true;
  }

  return false;
}

static bool selected_confirmation_symbol(
    MedicationSymbol *symbol
) {
  if (s_scroll.snap_index == 0) {
    return first_unconfirmed_due_symbol(symbol);
  }

  if (
    s_scroll.snap_index < 1 ||
    s_scroll.snap_index > LIST_ROW_COUNT
  ) {
    return false;
  }

  const uint8_t row_index =
      (uint8_t)(s_scroll.snap_index - 1);

  if (
    s_row_kinds[row_index] ==
        MEDICATION_ROW_CONFIRM_PILLS
  ) {
    if (symbol) {
      *symbol = MEDICATION_SYMBOL_PILL;
    }

    return true;
  }

  if (
    s_row_kinds[row_index] ==
        MEDICATION_ROW_CONFIRM_PEN
  ) {
    if (symbol) {
      *symbol = MEDICATION_SYMBOL_PEN;
    }

    return true;
  }

  return false;
}

static bool confirmation_prompt_is_active(
    MedicationSymbol *symbol
) {
  if (
    s_transfer_screen_active ||
    !selected_confirmation_symbol(symbol) ||
    s_scroll.mode != SCROLL_IDLE ||
    s_band.animating
  ) {
    return false;
  }

  if (s_scroll.snap_index == 0) {
    return true;
  }

  return
      s_band.target_visible &&
      s_band_layer &&
      !layer_get_hidden(
        s_band_layer
      );
}

static void update_taken_button_hint_pulse(void) {
  if (!confirmation_prompt_is_active(NULL)) {
    s_taken_hint_phase = -1;
    return;
  }

  if (s_taken_hint_phase < 0) {
    s_taken_hint_phase = 0;
  } else if (
    s_taken_hint_phase + 1 <
    (int)ARRAY_LENGTH(s_hint_offsets)
  ) {
    s_taken_hint_phase++;
  }
}

static void draw_taken_button_hint(
    GContext *ctx,
    GRect layer_bounds,
    GRect frame,
    GRect canvas_bounds
) {
  if (!confirmation_prompt_is_active(NULL)) {
    return;
  }

  const uint8_t phase =
      s_taken_hint_phase < 0
          ? 0
          : (uint8_t)s_taken_hint_phase;

  const int16_t radius =
      TAKEN_HINT_MIN_RADIUS + s_hint_offsets[phase];

  /* Der Mittelpunkt am Displayrand erzeugt den sichtbaren Halbkreis. */
  const int16_t local_screen_edge_x =
      canvas_bounds.size.w -
      frame.origin.x;

  graphics_context_set_fill_color(
    ctx,
    theme_background_color()
  );

  graphics_fill_circle(
    ctx,
    GPoint(
      local_screen_edge_x,
      layer_bounds.size.h / 2
    ),
    (uint16_t)radius
  );
}

static void band_update_proc(
    Layer *layer,
    GContext *ctx
) {
  if (!s_canvas_layer) {
    return;
  }

  const GRect layer_bounds = layer_get_bounds(layer);
  const GRect frame = layer_get_frame(layer);
  const GRect canvas_bounds = layer_get_bounds(s_canvas_layer);

  /* Die versetzte Textkopie invertiert nur den überdeckten Bereich. */
  const GRect content_bounds = GRect(
    -frame.origin.x,
    0,
    canvas_bounds.size.w,
    layer_bounds.size.h
  );

  graphics_context_set_fill_color(
    ctx,
    theme_foreground_color()
  );
  graphics_fill_rect(ctx, layer_bounds, 0, GCornerNone);

  draw_medications(
    ctx,
    content_bounds,
    current_pill_y() - frame.origin.y,
    theme_background_color()
  );

  draw_taken_button_hint(
    ctx,
    layer_bounds,
    frame,
    canvas_bounds
  );
}

static int16_t transfer_lerp_int16(
    int16_t from,
    int16_t to,
    uint16_t progress
) {
  return (int16_t)(
    from +
    ((int32_t)(to - from) * progress) /
        TRANSFER_PROGRESS_MAX
  );
}

static GPoint transfer_lerp_point(
    GPoint from,
    GPoint to,
    uint16_t progress
) {
  return GPoint(
    transfer_lerp_int16(
      from.x,
      to.x,
      progress
    ),
    transfer_lerp_int16(
      from.y,
      to.y,
      progress
    )
  );
}

static void draw_transfer_round_line(
    GContext *ctx,
    GPoint start,
    GPoint end,
    int16_t radius
) {
  const int16_t dx = end.x - start.x;
  const int16_t dy = end.y - start.y;
  const int16_t abs_dx = dx < 0 ? -dx : dx;
  const int16_t abs_dy = dy < 0 ? -dy : dy;
  const int16_t steps = abs_dx > abs_dy ? abs_dx : abs_dy;

  if (steps <= 0) {
    graphics_fill_circle(ctx, start, (uint16_t)radius);
    return;
  }

  for (int16_t step = 0; step <= steps; step++) {
    graphics_fill_circle(
      ctx,
      GPoint(
        start.x + ((int32_t)dx * step) / steps,
        start.y + ((int32_t)dy * step) / steps
      ),
      (uint16_t)radius
    );
  }
}

static uint16_t transfer_progress_for_duration(
    uint16_t duration_ms
) {
  if (
    duration_ms == 0 ||
    s_transfer_animation_elapsed_ms >= duration_ms
  ) {
    return TRANSFER_PROGRESS_MAX;
  }

  return (uint16_t)(
    ((uint32_t)s_transfer_animation_elapsed_ms *
     TRANSFER_PROGRESS_MAX) /
    duration_ms
  );
}

static int16_t transfer_shaft_bounce_offset(
    uint16_t progress
) {
  if (progress < 760) {
    const uint16_t local_progress =
        (uint16_t)(
          ((uint32_t)progress *
           TRANSFER_PROGRESS_MAX) /
          760
        );
    const int32_t inverse =
        TRANSFER_PROGRESS_MAX -
        local_progress;
    const int32_t eased =
        TRANSFER_PROGRESS_MAX -
        (inverse * inverse) /
            TRANSFER_PROGRESS_MAX;

    return (int16_t)(
      -170 +
      (176 * eased) /
          TRANSFER_PROGRESS_MAX
    );
  }

  if (progress < 890) {
    const uint16_t local_progress =
        (uint16_t)(
          ((uint32_t)(progress - 760) *
           TRANSFER_PROGRESS_MAX) /
          130
        );

    return (int16_t)(
      6 -
      (10 * local_progress) /
          TRANSFER_PROGRESS_MAX
    );
  }

  const uint16_t local_progress =
      (uint16_t)(
        ((uint32_t)(progress - 890) *
         TRANSFER_PROGRESS_MAX) /
        110
      );

  return (int16_t)(
    -4 +
    (4 * local_progress) /
        TRANSFER_PROGRESS_MAX
  );
}

static void draw_transfer_icon(
    GContext *ctx,
    GRect bounds
) {
  const int16_t center_x =
      bounds.size.w / 2;
  const int16_t center_y =
      bounds.size.h / 2;
  const int16_t radius = CHECK_STROKE_RADIUS;
  const int16_t fall_offset =
      s_transfer_animation_state ==
          TRANSFER_ANIMATION_FALLING
          ? s_transfer_fall_offset
          : 0;

  const int16_t check_size =
      CHECK_POP_SETTLE_SIZE;

  const GPoint check_start = GPoint(
    center_x - (check_size * 42) / 100,
    center_y
  );
  const GPoint check_middle = GPoint(
    center_x - (check_size * 10) / 100,
    center_y + (check_size * 28) / 100
  );
  const GPoint check_end = GPoint(
    center_x + (check_size * 45) / 100,
    center_y - (check_size * 30) / 100
  );

  const GPoint arrow_left = GPoint(
    center_x - 24,
    center_y
  );
  const GPoint arrow_tip = GPoint(
    center_x,
    center_y + 28
  );
  const GPoint arrow_right = GPoint(
    center_x + 24,
    center_y
  );

  uint16_t morph_progress =
      TRANSFER_PROGRESS_MAX;

  if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_MORPHING
  ) {
    morph_progress =
        transfer_progress_for_duration(
          TRANSFER_MORPH_DURATION_MS
        );
  }

  GPoint left = transfer_lerp_point(
    check_start,
    arrow_left,
    morph_progress
  );
  GPoint tip = transfer_lerp_point(
    check_middle,
    arrow_tip,
    morph_progress
  );
  GPoint right = transfer_lerp_point(
    check_end,
    arrow_right,
    morph_progress
  );

  left.y += fall_offset;
  tip.y += fall_offset;
  right.y += fall_offset;

  graphics_context_set_fill_color(
    ctx,
    GColorWhite
  );

  draw_transfer_round_line(
    ctx,
    left,
    tip,
    radius
  );
  draw_transfer_round_line(
    ctx,
    tip,
    right,
    radius
  );

  if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_MORPHING
  ) {
    return;
  }

  uint16_t drop_progress =
      TRANSFER_PROGRESS_MAX;

  if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_SHAFT_DROP
  ) {
    drop_progress =
        transfer_progress_for_duration(
          TRANSFER_SHAFT_DURATION_MS
        );
  }

  const int16_t shaft_offset =
      transfer_shaft_bounce_offset(
        drop_progress
      );

  const GPoint shaft_top = GPoint(
    center_x,
    center_y - 48 +
        shaft_offset +
        fall_offset
  );
  const GPoint shaft_bottom = GPoint(
    center_x,
    center_y + 26 +
        shaft_offset +
        fall_offset
  );

  draw_transfer_round_line(
    ctx,
    shaft_top,
    shaft_bottom,
    radius
  );

  const int16_t base_half_width =
      (int16_t)(
        (30 * drop_progress) /
        TRANSFER_PROGRESS_MAX
      );

  const GPoint base_left = GPoint(
    center_x - base_half_width,
    center_y + 49 + fall_offset
  );
  const GPoint base_right = GPoint(
    center_x + base_half_width,
    center_y + 49 + fall_offset
  );

  draw_transfer_round_line(
    ctx,
    base_left,
    base_right,
    radius
  );
}

static void confirmation_update_proc(
    Layer *layer,
    GContext *ctx
) {
  const GRect bounds =
      layer_get_bounds(layer);

  if (s_transfer_screen_active) {
    graphics_context_set_fill_color(
      ctx,
      GColorGreen
    );
    graphics_fill_rect(
      ctx,
      bounds,
      0,
      GCornerNone
    );
    draw_transfer_icon(ctx, bounds);
    return;
  }

  draw_confirmation_circle(
    ctx,
    bounds
  );

  draw_confirmation_checkmark(
    ctx,
    bounds
  );
}

static void ui_timer_callback(void *context) {
  s_ui_timer = NULL;

  if (
    !s_canvas_layer ||
    s_confirmed_screen_active ||
    s_transfer_screen_active
  ) {
    return;
  }

  s_animation_tick =
      (s_animation_tick + 1) %
      (FRAME_COUNT * PILL_TICKS_PER_FRAME);

  update_taken_button_hint_pulse();
  mark_scene_dirty();

  s_ui_timer = app_timer_register(
    UI_TICK_MS,
    ui_timer_callback,
    NULL
  );
}

static void start_ui_timer(void) {
  cancel_timer(&s_ui_timer);

  if (
    s_confirmed_screen_active ||
    s_transfer_screen_active
  ) {
    return;
  }

  s_ui_timer = app_timer_register(
    UI_TICK_MS,
    ui_timer_callback,
    NULL
  );
}

static void confirm_medication_group(
    MedicationSymbol symbol
) {
  mark_medication_group_confirmed(symbol);
  alarm_confirmation_received(symbol);

  /*
   * TODO: Den späteren Wiederholungs-Wakeup nur
   * für diese Gruppe abbrechen:
   * Tabletten oder Pen.
   */
}

static bool confirmation_animation_active(void) {
  return
      s_confirmation_state == CONFIRM_GROWING ||
      s_confirmation_state == CONFIRM_SHRINKING ||
      s_check_state == CHECK_POPPING_OUT ||
      s_check_state == CHECK_AT_PEAK ||
      s_check_state == CHECK_SETTLING;
}

static void schedule_confirmation_timer(void);

static void update_confirmation_circle(void) {
  if (s_confirmation_state == CONFIRM_GROWING) {
    s_confirm_radius += CONFIRM_GROW_STEP;

    if (s_confirm_radius < s_confirm_max_radius) {
      return;
    }

    s_confirm_radius = s_confirm_max_radius;
    s_confirmation_state = CONFIRM_COMPLETE;
    s_check_size = 8;
    s_check_state = CHECK_POPPING_OUT;

    cancel_timer(&s_ui_timer);

    if (s_confirmation_symbol_set) {
      confirm_medication_group(
        s_confirmation_symbol
      );
    }

    return;
  }

  if (s_confirmation_state != CONFIRM_SHRINKING) {
    return;
  }

  int16_t shrink_step =
      s_confirm_radius / CONFIRM_SHRINK_DIVISOR;

  if (shrink_step < CONFIRM_SHRINK_MIN_STEP) {
    shrink_step = CONFIRM_SHRINK_MIN_STEP;
  }

  s_confirm_radius -= shrink_step;

  if (s_confirm_radius <= 0) {
    s_confirm_radius = 0;
    s_confirmation_state = CONFIRM_IDLE;
    s_confirmation_symbol_set = false;
  }
}

static void update_checkmark(void) {
  switch (s_check_state) {
    case CHECK_POPPING_OUT:
      s_check_size += CHECK_POP_GROW_STEP;

      if (s_check_size >= CHECK_POP_OVERSHOOT_SIZE) {
        s_check_size = CHECK_POP_OVERSHOOT_SIZE;
        s_check_state = CHECK_AT_PEAK;
      }
      break;

    case CHECK_AT_PEAK:
      vibes_enqueue_custom_pattern(
        s_impact_vibration_pattern
      );
      s_check_state = CHECK_SETTLING;
      break;

    case CHECK_SETTLING:
      s_check_size -= CHECK_POP_SHRINK_STEP;

      if (s_check_size <= CHECK_POP_SETTLE_SIZE) {
        s_check_size = CHECK_POP_SETTLE_SIZE;
        s_check_state = CHECK_VISIBLE;
      }
      break;

    case CHECK_HIDDEN:
    case CHECK_VISIBLE:
      break;
  }
}

static void confirmation_timer_callback(void *context) {
  s_confirmation_timer = NULL;

  update_confirmation_circle();
  update_checkmark();

  if (s_confirmation_layer) {
    layer_mark_dirty(s_confirmation_layer);
  }

  if (confirmation_animation_active()) {
    schedule_confirmation_timer();
  }
}

static void schedule_confirmation_timer(void) {
  if (s_confirmation_timer) {
    return;
  }

  s_confirmation_timer = app_timer_register(
    CONFIRM_ANIMATION_INTERVAL_MS,
    confirmation_timer_callback,
    NULL
  );
}

static int32_t scroll_anchor_q8(int index) {
  return
      snap_anchor_for_index(
        clamp_snap_index(index)
      ) *
      SCROLL_Q8;
}

static int32_t scroll_top_limit_q8(void) {
  return
      scroll_anchor_q8(0) +
      SCROLL_EDGE_HALF_INTERVAL_PX *
      SCROLL_Q8;
}

static int32_t scroll_bottom_limit_q8(void) {
  return
      scroll_anchor_q8(LIST_ROW_COUNT) -
      SCROLL_EDGE_HALF_INTERVAL_PX *
      SCROLL_Q8;
}

static int nearest_snap_index_for_position_q8(
    int32_t position_q8
) {
  const int current =
      clamp_snap_index(
        s_scroll.snap_index
      );

  const int32_t anchor_q8 =
      scroll_anchor_q8(current);

  const int32_t escape_q8 =
      SCROLL_EDGE_HALF_INTERVAL_PX *
      SCROLL_Q8;

  if (
    position_q8 >= anchor_q8 + escape_q8 &&
    current > 0
  ) {
    return current - 1;
  }

  if (
    position_q8 <= anchor_q8 - escape_q8 &&
    current < LIST_ROW_COUNT
  ) {
    return current + 1;
  }

  return current;
}

static int32_t magnet_force_between_q8(
    int32_t upper_anchor_q8,
    int32_t lower_anchor_q8,
    int32_t position_q8
) {
  const int32_t distance_q8 =
      upper_anchor_q8 -
      lower_anchor_q8;

  if (distance_q8 <= 0) {
    return 0;
  }

  const int32_t progress_q8 =
      (
        (
          upper_anchor_q8 -
          position_q8
        ) *
        SCROLL_Q8
      ) /
      distance_q8;

  const int32_t angle =
      (
        progress_q8 *
        TRIG_MAX_ANGLE
      ) /
      SCROLL_Q8;

  const int32_t maximum_force_q8 =
      (
        distance_q8 /
        SCROLL_Q8
      ) *
      SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8;

  return
      (
        maximum_force_q8 *
        sin_lookup(angle)
      ) /
      TRIG_MAX_RATIO;
}

static int32_t edge_magnet_force_q8(
    int32_t anchor_q8,
    int32_t position_q8
) {
  const int32_t delta_q8 =
      position_q8 -
      anchor_q8;

  int32_t progress_q8 =
      (
        abs_int32(delta_q8) *
        SCROLL_Q8
      ) /
      (
        SCROLL_SNAP_REFERENCE_PX *
        SCROLL_Q8
      );

  if (progress_q8 > SCROLL_Q8 / 2) {
    progress_q8 = SCROLL_Q8 / 2;
  }

  const int32_t angle =
      (
        progress_q8 *
        TRIG_MAX_ANGLE
      ) /
      SCROLL_Q8;

  const int32_t magnitude_q8 =
      (
        SCROLL_SNAP_REFERENCE_PX *
        SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8 *
        sin_lookup(angle)
      ) /
      TRIG_MAX_RATIO;

  return
      delta_q8 >= 0
          ? -magnitude_q8
          : magnitude_q8;
}

static int32_t magnet_force_for_position_q8(
    int32_t position_q8
) {
  const int32_t top_anchor_q8 =
      scroll_anchor_q8(0);

  const int32_t bottom_anchor_q8 =
      scroll_anchor_q8(
        LIST_ROW_COUNT
      );

  if (position_q8 >= top_anchor_q8) {
    return edge_magnet_force_q8(
      top_anchor_q8,
      position_q8
    );
  }

  if (position_q8 <= bottom_anchor_q8) {
    return edge_magnet_force_q8(
      bottom_anchor_q8,
      position_q8
    );
  }

  for (
    int index = 0;
    index < LIST_ROW_COUNT;
    index++
  ) {
    const int32_t upper_anchor_q8 =
        scroll_anchor_q8(index);

    const int32_t lower_anchor_q8 =
        scroll_anchor_q8(index + 1);

    if (
      position_q8 <= upper_anchor_q8 &&
      position_q8 >= lower_anchor_q8
    ) {
      return magnet_force_between_q8(
        upper_anchor_q8,
        lower_anchor_q8,
        position_q8
      );
    }
  }

  return 0;
}

static void schedule_scroll_physics(void);

static void start_scroll_snap(
    int target_index,
    bool keep_velocity
);

#if defined(PBL_TOUCH)
static bool touch_step_reached_threshold(void);
static void keep_touch_active_after_step(void);
#endif

static bool step_snap_index(int direction);

static void cancel_scroll_physics(void) {
  cancel_timer(&s_scroll_physics_timer);

  s_scroll.mode = SCROLL_IDLE;
  s_scroll.velocity_q8 = 0;
  s_scroll.breakaway_locked = false;
}

static void apply_scroll_edge_limits(void) {
  const int32_t top_limit_q8 =
      scroll_top_limit_q8();

  const int32_t bottom_limit_q8 =
      scroll_bottom_limit_q8();

  if (s_scroll.position_q8 > top_limit_q8) {
    s_scroll.position_q8 = top_limit_q8;

    if (s_scroll.velocity_q8 > 0) {
      s_scroll.velocity_q8 = 0;
    }
  }

  if (s_scroll.position_q8 < bottom_limit_q8) {
    s_scroll.position_q8 = bottom_limit_q8;

    if (s_scroll.velocity_q8 < 0) {
      s_scroll.velocity_q8 = 0;
    }
  }
}

static bool edge_bounce_reached_limit(void) {
  if (s_scroll.mode != SCROLL_EDGE_BOUNCE) {
    return false;
  }

  return s_scroll.snap_index == 0
      ? s_scroll.position_q8 >= scroll_top_limit_q8()
      : s_scroll.position_q8 <= scroll_bottom_limit_q8();
}

#if defined(PBL_TOUCH)
static bool touch_reached_virtual_edge(void) {
  if (
    s_scroll.mode != SCROLL_TOUCH ||
    !s_touch.pair_selected ||
    s_touch.edge_consumed ||
    s_touch.neighbor_index !=
        s_touch.start_index
  ) {
    return false;
  }

  if (
    s_touch.start_index == 0 &&
    s_touch.pair_direction < 0
  ) {
    return
        s_scroll.position_q8 >=
        scroll_top_limit_q8();
  }

  if (
    s_touch.start_index ==
        LIST_ROW_COUNT &&
    s_touch.pair_direction > 0
  ) {
    return
        s_scroll.position_q8 <=
        scroll_bottom_limit_q8();
  }

  return false;
}
#endif

static void scroll_physics_tick(void *context) {
  s_scroll_physics_timer = NULL;

  if (s_scroll.mode == SCROLL_IDLE) {
    return;
  }

  int32_t force_q8;

  if (s_scroll.mode == SCROLL_TOUCH) {
    force_q8 =
        (
          (
            s_scroll.target_q8 -
            s_scroll.position_q8
          ) *
          SCROLL_FINGER_SPRING_NUM
        ) /
        SCROLL_FINGER_SPRING_DEN;

    if (s_scroll.breakaway_locked) {
      if (
        abs_int32(force_q8) <
        SCROLL_BREAKAWAY_FORCE_Q8
      ) {
        s_scroll.position_q8 =
            s_scroll.breakaway_anchor_q8;

        s_scroll.velocity_q8 = 0;
        schedule_scroll_physics();
        return;
      }

      s_scroll.breakaway_locked = false;
      s_scroll.position_q8 =
          s_scroll.breakaway_anchor_q8;

      s_scroll.velocity_q8 = 0;
    }

    force_q8 +=
        magnet_force_for_position_q8(
          s_scroll.position_q8
        );

    s_scroll.velocity_q8 += force_q8;

    s_scroll.velocity_q8 =
        (
          s_scroll.velocity_q8 *
          SCROLL_FINGER_DAMPING_NUM
        ) /
        SCROLL_FINGER_DAMPING_DEN;
  } else {
    force_q8 =
        (
          (
            s_scroll.target_q8 -
            s_scroll.position_q8
          ) *
          SCROLL_SNAP_SPRING_NUM
        ) /
        SCROLL_SNAP_SPRING_DEN;

    force_q8 =
        clamp_symmetric(
          force_q8,
          SCROLL_SNAP_MAX_FORCE_Q8
        );

    force_q8 +=
        magnet_force_for_position_q8(
          s_scroll.position_q8
        );

    s_scroll.velocity_q8 += force_q8;

    s_scroll.velocity_q8 =
        (
          s_scroll.velocity_q8 *
          SCROLL_SNAP_DAMPING_NUM
        ) /
        SCROLL_SNAP_DAMPING_DEN;
  }

  s_scroll.velocity_q8 =
      clamp_symmetric(
        s_scroll.velocity_q8,
        SCROLL_MAX_VELOCITY_Q8
      );

  s_scroll.position_q8 +=
      s_scroll.velocity_q8;

#if defined(PBL_TOUCH)
  if (touch_step_reached_threshold()) {
    const int direction =
        s_touch.pair_direction;

    step_snap_index(
      direction
    );

    keep_touch_active_after_step();
    mark_scene_dirty();
    return;
  }
#endif

  apply_scroll_edge_limits();

  if (edge_bounce_reached_limit()) {
    start_scroll_snap(
      s_scroll.snap_index,
      false
    );

    mark_scene_dirty();
    return;
  }

#if defined(PBL_TOUCH)
  if (touch_reached_virtual_edge()) {
    s_touch.edge_consumed = true;

    start_scroll_snap(
      s_touch.start_index,
      true
    );

    mark_scene_dirty();
    return;
  }
#endif

  if (
    s_scroll.mode == SCROLL_SNAP &&
    abs_int32(
      s_scroll.target_q8 -
      s_scroll.position_q8
    ) <= SCROLL_STOP_POSITION_Q8 &&
    abs_int32(
      s_scroll.velocity_q8
    ) <= SCROLL_STOP_VELOCITY_Q8
  ) {
    s_scroll.position_q8 =
        s_scroll.target_q8;

    s_scroll.velocity_q8 = 0;
    s_scroll.mode = SCROLL_IDLE;
  }

  mark_scene_dirty();

  if (s_scroll.mode != SCROLL_IDLE) {
    schedule_scroll_physics();
  }
}

static void schedule_scroll_physics(void) {
  if (s_scroll_physics_timer) {
    return;
  }

  s_scroll_physics_timer = app_timer_register(
    SCROLL_FRAME_MS,
    scroll_physics_tick,
    NULL
  );

  if (
    !s_scroll_physics_timer &&
    s_scroll.mode == SCROLL_SNAP
  ) {
    s_scroll.position_q8 =
        s_scroll.target_q8;

    s_scroll.velocity_q8 = 0;
    s_scroll.mode = SCROLL_IDLE;
    mark_scene_dirty();
  }
}

static void start_scroll_snap(
    int target_index,
    bool keep_velocity
) {
  target_index =
      clamp_snap_index(target_index);

#if defined(PBL_TOUCH)
  s_touch.dragging = false;
#endif

  s_scroll.mode = SCROLL_SNAP;
  s_scroll.breakaway_locked = false;
  s_scroll.target_q8 =
      scroll_anchor_q8(target_index);

  if (!keep_velocity) {
    s_scroll.velocity_q8 = 0;
  }

  s_scroll.snap_index = target_index;
  mark_scene_dirty();

  if (
    s_scroll.position_q8 ==
        s_scroll.target_q8 &&
    abs_int32(
      s_scroll.velocity_q8
    ) <= SCROLL_STOP_VELOCITY_Q8
  ) {
    s_scroll.velocity_q8 = 0;
    s_scroll.mode = SCROLL_IDLE;
    return;
  }

  schedule_scroll_physics();
}

static void start_edge_bounce(int direction) {
  cancel_timer(&s_scroll_physics_timer);

#if defined(PBL_TOUCH)
  s_touch.dragging = false;
#endif

  s_scroll.mode = SCROLL_EDGE_BOUNCE;
  s_scroll.breakaway_locked = false;
  s_scroll.velocity_q8 = 0;

  s_scroll.target_q8 =
      scroll_anchor_q8(s_scroll.snap_index) +
      (direction < 0 ? 2 : -2) *
          SCROLL_EDGE_HALF_INTERVAL_PX *
          SCROLL_Q8;

  schedule_scroll_physics();
}

static bool step_snap_index(int direction) {
  const int next_index =
      clamp_snap_index(
        s_scroll.snap_index +
        direction
      );

  if (next_index == s_scroll.snap_index) {
    const bool valid_edge =
        (
          s_scroll.snap_index == 0 &&
          direction < 0
        ) ||
        (
          s_scroll.snap_index ==
              LIST_ROW_COUNT &&
          direction > 0
        );

    if (valid_edge) {
      start_edge_bounce(direction);
    }

    return valid_edge;
  }

  start_scroll_snap(
    next_index,
    true
  );

  return true;
}

static void scroll_up_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (
    !s_confirmed_screen_active &&
    !s_transfer_screen_active &&
    s_confirmation_state == CONFIRM_IDLE
  ) {
    step_snap_index(-1);
  }
}

static void scroll_down_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (
    !s_confirmed_screen_active &&
    !s_transfer_screen_active &&
    s_confirmation_state == CONFIRM_IDLE
  ) {
    step_snap_index(1);
  }
}

static void select_button_down(
    ClickRecognizerRef recognizer,
    void *context
) {
  MedicationSymbol symbol;

  if (
    s_confirmation_state != CONFIRM_IDLE ||
    !confirmation_prompt_is_active(&symbol)
  ) {
    return;
  }

  s_confirmation_symbol = symbol;
  s_confirmation_symbol_set = true;
  s_confirmation_state = CONFIRM_GROWING;
  schedule_confirmation_timer();
}

static void exit_app(void) {
  cancel_timer(&s_transfer_close_timer);
  cancel_timer(&s_transfer_animation_timer);
  s_transfer_screen_active = false;
  s_transfer_animation_state =
      TRANSFER_ANIMATION_IDLE;
  alarm_stop();
  window_stack_pop_all(true);
}

static void schedule_transfer_animation_tick(void);

static void transfer_animation_timer_handler(
    void *context
) {
  (void)context;
  s_transfer_animation_timer = NULL;

  if (!s_transfer_screen_active) {
    return;
  }

  s_transfer_animation_elapsed_ms +=
      TRANSFER_ANIMATION_INTERVAL_MS;

  if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_MORPHING &&
    s_transfer_animation_elapsed_ms >=
        TRANSFER_MORPH_DURATION_MS
  ) {
    s_transfer_animation_state =
        TRANSFER_ANIMATION_SHAFT_DROP;
    s_transfer_animation_elapsed_ms = 0;
  } else if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_SHAFT_DROP &&
    s_transfer_animation_elapsed_ms >=
        TRANSFER_SHAFT_DURATION_MS
  ) {
    s_transfer_animation_state =
        TRANSFER_ANIMATION_READY;
    s_transfer_animation_elapsed_ms = 0;
  } else if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_FALLING
  ) {
    uint16_t progress =
        transfer_progress_for_duration(
          TRANSFER_FALL_DURATION_MS
        );

    const int32_t eased =
        ((int32_t)progress * progress) /
        TRANSFER_PROGRESS_MAX;

    int16_t travel = 320;

    if (s_confirmation_layer) {
      travel =
          layer_get_bounds(
            s_confirmation_layer
          ).size.h + 120;
    }

    s_transfer_fall_offset =
        (int16_t)(
          ((int32_t)travel * eased) /
          TRANSFER_PROGRESS_MAX
        );

    if (progress >= TRANSFER_PROGRESS_MAX) {
      APP_LOG(
        APP_LOG_LEVEL_INFO,
        "Settings transfer complete: icon left screen"
      );
      exit_app();
      return;
    }
  }

  if (s_confirmation_layer) {
    layer_mark_dirty(
      s_confirmation_layer
    );
  }

  if (
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_MORPHING ||
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_SHAFT_DROP ||
    s_transfer_animation_state ==
        TRANSFER_ANIMATION_FALLING
  ) {
    schedule_transfer_animation_tick();
  }
}

static void schedule_transfer_animation_tick(void) {
  if (
    s_transfer_animation_timer ||
    !s_transfer_screen_active
  ) {
    return;
  }

  s_transfer_animation_timer = app_timer_register(
    TRANSFER_ANIMATION_INTERVAL_MS,
    transfer_animation_timer_handler,
    NULL
  );

  if (!s_transfer_animation_timer) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Could not schedule transfer animation"
    );
  }
}

static void start_transfer_animation(void) {
  cancel_timer(&s_transfer_animation_timer);

  s_transfer_animation_state =
      TRANSFER_ANIMATION_MORPHING;
  s_transfer_animation_elapsed_ms = 0;
  s_transfer_fall_offset = 0;

  if (s_confirmation_layer) {
    layer_mark_dirty(
      s_confirmation_layer
    );
  }

  schedule_transfer_animation_tick();
}

static void transfer_close_timer_handler(
    void *context
) {
  (void)context;
  s_transfer_close_timer = NULL;

  if (!s_transfer_screen_active) {
    return;
  }

  APP_LOG(
    APP_LOG_LEVEL_INFO,
    "Settings transfer complete: dropping icon"
  );

  cancel_timer(&s_transfer_animation_timer);
  s_transfer_animation_state =
      TRANSFER_ANIMATION_FALLING;
  s_transfer_animation_elapsed_ms = 0;
  s_transfer_fall_offset = 0;
  schedule_transfer_animation_tick();
}

static void schedule_transfer_close(void) {
  cancel_timer(&s_transfer_close_timer);

  if (!s_transfer_screen_active) {
    return;
  }

  s_transfer_close_timer = app_timer_register(
    TRANSFER_CLOSE_DELAY_MS,
    transfer_close_timer_handler,
    NULL
  );

  if (!s_transfer_close_timer) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Could not schedule transfer close"
    );
  }
}

static void select_button_up(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state == CONFIRM_COMPLETE) {
    if (unconfirmed_medication_group_is_due()) {
      refresh_app_screen_state();
      return;
    }

    if (s_confirmation_symbol_set) {
      exit_app();
      return;
    }

    refresh_app_screen_state();
    return;
  }

  if (s_confirm_radius <= 0) {
    s_confirmation_state = CONFIRM_IDLE;
    s_confirmation_symbol_set = false;
    return;
  }

  s_confirmation_state = CONFIRM_SHRINKING;
  schedule_confirmation_timer();
}

static void back_button_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  exit_app();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(
    BUTTON_ID_UP,
    scroll_up_handler
  );

  window_raw_click_subscribe(
    BUTTON_ID_SELECT,
    select_button_down,
    select_button_up,
    NULL
  );

  window_single_click_subscribe(
    BUTTON_ID_DOWN,
    scroll_down_handler
  );

  window_single_click_subscribe(
    BUTTON_ID_BACK,
    back_button_handler
  );
}

#if defined(PBL_TOUCH)
static void touch_begin(const TouchEvent *event) {
  if (
    s_confirmed_screen_active ||
    s_transfer_screen_active ||
    s_confirmation_state != CONFIRM_IDLE
  ) {
    return;
  }

  cancel_timer(&s_scroll_physics_timer);

  s_touch = (ScrollTouchState) {
    .last_y = event->y,
    .start_time_ms = current_time_ms(),
    .start_index =
        clamp_snap_index(
          s_scroll.snap_index
        ),
    .neighbor_index =
        clamp_snap_index(
          s_scroll.snap_index
        ),
    .dragging = true
  };

  s_scroll.mode = SCROLL_TOUCH;
  s_scroll.target_q8 =
      s_scroll.position_q8;

  s_scroll.breakaway_anchor_q8 =
      scroll_anchor_q8(
        s_touch.start_index
      );

  s_scroll.breakaway_locked =
      abs_int32(
        s_scroll.position_q8 -
        s_scroll.breakaway_anchor_q8
      ) <= SCROLL_Q8;

  s_scroll.velocity_q8 = 0;

  schedule_scroll_physics();
}

static void choose_touch_pair(void) {
  if (
    s_touch.pair_selected ||
    abs_int32(
      s_touch.total_delta_y
    ) < SCROLL_BREAKAWAY_PX
  ) {
    return;
  }

  s_touch.pair_direction =
      s_touch.total_delta_y < 0
          ? 1
          : -1;

  s_touch.neighbor_index =
      clamp_snap_index(
        s_touch.start_index +
        s_touch.pair_direction
      );

  s_touch.pair_selected = true;
}

static void clamp_touch_target_to_pair(void) {
  if (!s_touch.pair_selected) {
    return;
  }

  const int32_t start_q8 =
      scroll_anchor_q8(
        s_touch.start_index
      );

  if (
    s_touch.neighbor_index !=
    s_touch.start_index
  ) {
    const int32_t neighbor_q8 =
        scroll_anchor_q8(
          s_touch.neighbor_index
        );

    const int32_t upper_q8 =
        start_q8 > neighbor_q8
            ? start_q8
            : neighbor_q8;

    const int32_t lower_q8 =
        start_q8 < neighbor_q8
            ? start_q8
            : neighbor_q8;

    if (s_scroll.target_q8 > upper_q8) {
      s_scroll.target_q8 = upper_q8;
    }

    if (s_scroll.target_q8 < lower_q8) {
      s_scroll.target_q8 = lower_q8;
    }

    return;
  }

  if (
    s_touch.pair_direction > 0 &&
    s_scroll.target_q8 > start_q8
  ) {
    s_scroll.target_q8 = start_q8;
  }

  if (
    s_touch.pair_direction < 0 &&
    s_scroll.target_q8 < start_q8
  ) {
    s_scroll.target_q8 = start_q8;
  }
}

static bool touch_step_reached_threshold(void) {
  if (
    s_scroll.mode != SCROLL_TOUCH ||
    !s_touch.dragging ||
    !s_touch.pair_selected ||
    s_touch.neighbor_index ==
        s_touch.start_index
  ) {
    return false;
  }

  const int32_t start_q8 =
      scroll_anchor_q8(
        s_touch.start_index
      );

  const int32_t threshold_q8 =
      start_q8 -
      (
        int32_t
      )s_touch.pair_direction *
      SCROLL_EDGE_HALF_INTERVAL_PX *
      SCROLL_Q8;

  if (s_touch.pair_direction > 0) {
    return
        s_scroll.position_q8 <=
        threshold_q8;
  }

  return
      s_scroll.position_q8 >=
      threshold_q8;
}

static void keep_touch_active_after_step(void) {
  /* Das beim Aufsetzen gewählte Rastpunktpaar bleibt gesperrt. */
  const int8_t previous_index =
      s_touch.start_index;

  const int8_t current_index =
      clamp_snap_index(
        s_scroll.snap_index
      );

  s_touch.dragging = true;
  s_touch.edge_consumed = true;
  s_touch.total_delta_y = 0;
  s_touch.start_time_ms =
      current_time_ms();

  s_touch.start_index =
      current_index;

  s_touch.neighbor_index =
      previous_index;

  s_touch.pair_direction =
      previous_index >
          current_index
          ? 1
          : -1;

  s_touch.pair_selected = true;
}

static void touch_update(
    const TouchEvent *event
) {
  if (!s_touch.dragging) {
    return;
  }

  const int16_t delta_y =
      event->y -
      s_touch.last_y;

  s_touch.last_y = event->y;

  if (delta_y == 0) {
    return;
  }

  if (s_touch.edge_consumed) {
    /* Einen laufenden Snap nicht durch Fingerbewegung verändern. */
    if (s_scroll.mode != SCROLL_IDLE &&
        s_scroll.mode != SCROLL_TOUCH) {
      return;
    }

    if (s_scroll.mode == SCROLL_IDLE) {
      s_scroll.mode = SCROLL_TOUCH;
      s_scroll.target_q8 =
          s_scroll.position_q8;

      s_scroll.breakaway_anchor_q8 =
          scroll_anchor_q8(
            s_touch.start_index
          );

      s_scroll.breakaway_locked =
          abs_int32(
            s_scroll.position_q8 -
            s_scroll.breakaway_anchor_q8
          ) <= SCROLL_Q8;

      s_scroll.velocity_q8 = 0;
    }

    s_touch.total_delta_y +=
        delta_y;

    s_scroll.target_q8 +=
        (int32_t)delta_y *
        SCROLL_Q8;

    clamp_touch_target_to_pair();
    schedule_scroll_physics();
    return;
  }

  s_touch.total_delta_y +=
      delta_y;

  choose_touch_pair();

  s_scroll.target_q8 +=
      (int32_t)delta_y *
      SCROLL_Q8;

  clamp_touch_target_to_pair();
  schedule_scroll_physics();
}

static void touch_end(void) {
  if (!s_touch.dragging) {
    return;
  }

  s_touch.dragging = false;
  s_scroll.breakaway_locked = false;

  if (s_touch.edge_consumed) {
    return;
  }

  const bool quick_swipe =
      current_time_ms() -
          s_touch.start_time_ms <=
          SCROLL_QUICK_SWIPE_MAX_MS &&
      abs_int32(
        s_touch.total_delta_y
      ) >= SCROLL_QUICK_SWIPE_MIN_PX;

  if (quick_swipe) {
    const int direction =
        s_touch.total_delta_y < 0
            ? 1
            : -1;

    start_scroll_snap(
      s_scroll.snap_index +
          direction,
      true
    );
    return;
  }

  s_scroll.velocity_q8 = 0;

  start_scroll_snap(
    nearest_snap_index_for_position_q8(
      s_scroll.position_q8
    ),
    false
  );
}

static void touch_handler(
    const TouchEvent *event,
    void *context
) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      touch_begin(event);
      break;

    case TouchEvent_PositionUpdate:
      touch_update(event);
      break;

    case TouchEvent_Liftoff:
      touch_end();
      break;
  }
}
#endif

static bool load_pill_sheet(void) {
  s_sheet = gbitmap_create_with_resource(
    RESOURCE_ID_IMAGE_PILL_SHEET
  );

  if (!s_sheet) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Spritesheet could not be loaded"
    );
    return false;
  }

  const GRect bounds = gbitmap_get_bounds(s_sheet);

  if (bounds.size.w % FRAME_COUNT != 0) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Spritesheet width is invalid"
    );
    destroy_pill_bitmaps();
    return false;
  }

  s_frame_width = bounds.size.w / FRAME_COUNT;
  s_frame_height = bounds.size.h;

  for (uint8_t index = 0; index < FRAME_COUNT; index++) {
    s_frames[index] = gbitmap_create_as_sub_bitmap(
      s_sheet,
      GRect(
        index * s_frame_width,
        0,
        s_frame_width,
        s_frame_height
      )
    );

    if (!s_frames[index]) {
      APP_LOG(
        APP_LOG_LEVEL_ERROR,
        "Pill frame could not be created"
      );
      destroy_pill_bitmaps();
      return false;
    }
  }

  return true;
}

static void reset_ui_state(GRect bounds) {
  s_animation_tick = 0;
  s_taken_hint_phase = -1;

  const int32_t initial_position_q8 =
      CANVAS_START_OFFSET_Y *
      SCROLL_Q8;

  s_scroll = (ScrollState) {
    .position_q8 = initial_position_q8,
    .target_q8 = initial_position_q8,
    .breakaway_anchor_q8 =
        initial_position_q8,
    .snap_index = 0,
    .mode = SCROLL_IDLE
  };

#if defined(PBL_TOUCH)
  s_touch = (ScrollTouchState) { 0 };
#endif

  cancel_timer(
    &s_band_animation_timer
  );

  s_band = (BandAnimationState) {
    .x_q8 =
        bounds.size.w *
        SCROLL_Q8,
    .target_x_q8 =
        bounds.size.w *
        SCROLL_Q8,
    .target_visible = false,
    .animating = false
  };

  if (s_band_layer) {
    set_band_layer_x_q8(
      s_band.x_q8
    );

    set_band_and_arrow_hidden(
      true
    );
  }

  s_confirm_radius = 0;
  s_confirm_max_radius =
      bounds.size.w +
      CONFIRM_CENTER_OUTSIDE_X +
      bounds.size.h / 2;
  s_confirmation_state = CONFIRM_IDLE;
  s_confirmation_symbol_set = false;

  s_check_size = 0;
  s_check_state = CHECK_HIDDEN;
}

static void show_transfer_screen(void) {
  cancel_timer(&s_transfer_close_timer);
  alarm_stop();

  s_transfer_screen_active = true;
  s_confirmed_screen_active = false;
  pill_physics_update_activity();

  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_timer(&s_band_animation_timer);
  cancel_scroll_physics();

#if defined(PBL_TOUCH)
  s_touch.dragging = false;
#endif

  if (s_canvas_layer) {
    layer_set_hidden(
      s_canvas_layer,
      true
    );
  }

  set_band_and_arrow_hidden(true);

  s_confirmation_state = CONFIRM_IDLE;
  s_confirmation_symbol_set = false;
  s_confirm_radius = 0;
  s_check_size = 0;
  s_check_state = CHECK_HIDDEN;

  start_transfer_animation();

  if (s_confirmation_layer) {
    layer_set_hidden(
      s_confirmation_layer,
      false
    );
    layer_mark_dirty(
      s_confirmation_layer
    );
  }

  APP_LOG(
    APP_LOG_LEVEL_INFO,
    "App screen: transfer"
  );
}

static void refresh_app_screen_state(void) {
  if (s_transfer_screen_active) {
    return;
  }

  reset_medication_confirmations();
  rebuild_medication_rows();
  pill_physics_rebuild();

  const bool show_confirmed_screen =
      alarm_unconfirmed_symbol_mask_at(
        time(NULL)
      ) == 0;
  const bool state_changed =
      s_confirmed_screen_active !=
          show_confirmed_screen;

  s_confirmed_screen_active =
      show_confirmed_screen;

  if (!s_canvas_layer) {
    return;
  }

  if (show_confirmed_screen) {
    cancel_timer(&s_ui_timer);
    cancel_timer(&s_band_animation_timer);
    cancel_scroll_physics();

#if defined(PBL_TOUCH)
    s_touch.dragging = false;
#endif

    layer_set_hidden(
      s_canvas_layer,
      true
    );
    set_band_and_arrow_hidden(true);

    if (s_confirmation_layer) {
      layer_set_hidden(
        s_confirmation_layer,
        false
      );

      /*
       * Wird der letzte offene Eintrag gerade bestätigt, läuft die
       * vorhandene Hakenanimation weiter. Bei einem normalen App-Start
       * wird derselbe Endzustand sofort statisch dargestellt.
       */
      if (
        s_confirmation_state !=
            CONFIRM_COMPLETE
      ) {
        s_confirm_radius =
            s_confirm_max_radius;
        s_confirmation_state =
            CONFIRM_COMPLETE;
        s_confirmation_symbol_set = false;
        s_check_size =
            CHECK_POP_SETTLE_SIZE;
        s_check_state = CHECK_VISIBLE;
      }

      layer_mark_dirty(
        s_confirmation_layer
      );
    }
  } else {
    layer_set_hidden(
      s_canvas_layer,
      false
    );

    if (s_confirmation_layer) {
      layer_set_hidden(
        s_confirmation_layer,
        false
      );
    }

    reset_ui_state(
      layer_get_bounds(s_canvas_layer)
    );
    start_ui_timer();
    mark_scene_dirty();
  }

  pill_physics_update_activity();

  if (state_changed) {
    APP_LOG(
      APP_LOG_LEVEL_INFO,
      "App screen: %s",
      show_confirmed_screen
          ? "confirmed"
          : "alert"
    );
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  s_medication_font =
      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  s_header_font =
      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  if (!load_pill_sheet()) {
    return;
  }

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(
    s_canvas_layer,
    canvas_update_proc
  );
  layer_add_child(root, s_canvas_layer);

  const GRect band_frame = GRect(
    bounds.size.w,
    (
      bounds.size.h -
      MEDICATION_ROW_HEIGHT
    ) /
    2,
    bounds.size.w +
        BAND_OVERSHOOT_COVER_PX,
    MEDICATION_ROW_HEIGHT
  );

  s_band_layer =
      layer_create(band_frame);

  layer_set_clips(
    s_band_layer,
    true
  );

  set_band_and_arrow_hidden(
    true
  );

  layer_set_update_proc(
    s_band_layer,
    band_update_proc
  );

  layer_add_child(
    root,
    s_band_layer
  );

  s_band_arrow_layer =
      layer_create(
        GRect(
          bounds.size.w -
              BAND_ARROW_WIDTH,
          (
            bounds.size.h -
            MEDICATION_ROW_HEIGHT
          ) / 2,
          BAND_ARROW_WIDTH,
          MEDICATION_ROW_HEIGHT
        )
      );

  if (!s_band_arrow_layer) {
    layer_destroy(
      s_band_layer
    );
    s_band_layer = NULL;
    return;
  }

  layer_set_update_proc(
    s_band_arrow_layer,
    band_arrow_update_proc
  );

  layer_add_child(
    root,
    s_band_arrow_layer
  );

  s_confirmation_layer =
      layer_create(bounds);

  layer_set_update_proc(
    s_confirmation_layer,
    confirmation_update_proc
  );

  layer_add_child(
    root,
    s_confirmation_layer
  );

  reset_ui_state(bounds);
}

static void window_appear(Window *window) {
  (void)window;
  s_pill_physics_window_visible = true;

  refresh_medication_rows_for_time();

  if (s_alarm_launch_pending) {
    s_alarm_launch_pending = false;
    alarm_start();
    schedule_next_alarm_wakeup();
  } else {
    refresh_app_screen_state();
  }

  if (
    !s_confirmed_screen_active &&
    !s_transfer_screen_active &&
    s_band.animating
  ) {
    schedule_band_animation();
  }

#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
}

static void window_disappear(Window *window) {
  s_pill_physics_window_visible = false;
  pill_physics_update_activity();

  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_timer(&s_band_animation_timer);
  cancel_scroll_physics();

#if defined(PBL_TOUCH)
  s_touch.dragging = false;
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  pill_physics_stop();
  cancel_timer(&s_transfer_close_timer);
  cancel_timer(&s_transfer_animation_timer);
  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_timer(&s_band_animation_timer);
  cancel_scroll_physics();
  destroy_pill_bitmaps();

  if (s_confirmation_layer) {
    layer_destroy(s_confirmation_layer);
    s_confirmation_layer = NULL;
  }

  if (s_band_arrow_layer) {
    layer_destroy(s_band_arrow_layer);
    s_band_arrow_layer = NULL;
  }

  if (s_band_layer) {
    layer_destroy(s_band_layer);
    s_band_layer = NULL;
  }

  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }
}

static void init(void) {
  settings_init();
  load_medication_appearances();

  wakeup_service_subscribe(
    alarm_wakeup_handler
  );

  WakeupId launch_wakeup_id;
  int32_t launch_cookie;

  s_alarm_launch_pending =
      launch_reason() == APP_LAUNCH_WAKEUP &&
      wakeup_get_launch_event(
        &launch_wakeup_id,
        &launch_cookie
      ) &&
      launch_cookie == ALARM_WAKEUP_COOKIE;

  schedule_next_alarm_wakeup();

  tick_timer_service_subscribe(
    MINUTE_UNIT,
    daypart_tick_handler
  );

  s_window = window_create();
  window_set_background_color(
    s_window,
    theme_background_color()
  );

  window_set_click_config_provider(
    s_window,
    click_config_provider
  );

  window_set_window_handlers(
    s_window,
    (WindowHandlers) {
      .load = window_load,
      .appear = window_appear,
      .disappear = window_disappear,
      .unload = window_unload
    }
  );

  window_stack_push(s_window, true);
}

static void deinit(void) {
  pill_physics_stop();
  cancel_timer(&s_transfer_close_timer);
  cancel_timer(&s_transfer_animation_timer);
  alarm_stop();
  tick_timer_service_unsubscribe();
  settings_deinit();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
