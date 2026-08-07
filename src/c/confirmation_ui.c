#include <pebble.h>

#include <stdio.h>
#include <string.h>

#include "app_state.h"
#include "app_util.h"
#include "medication_model.h"
#include "watch_settings.h"
#include "medication_alarm.h"
#include "pill_physics.h"
#include "pill_renderer.h"
#include "scroll_controller.h"
#include "confirmation_ui.h"
#include "medication_ui.h"

static const uint32_t s_impact_vibration_durations[] = {
  50
};

static const VibePattern s_impact_vibration_pattern = {
  .durations = s_impact_vibration_durations,
  .num_segments = ARRAY_LENGTH(s_impact_vibration_durations)
};

static void draw_confirmation_circle(
    GContext *ctx,
    GRect bounds
);
static int16_t current_check_stroke_radius(void);
static void draw_round_line_with_radius(
    GContext *ctx,
    GPoint start,
    GPoint end,
    int16_t radius
);
static void draw_checkmark(
    GContext *ctx,
    GRect bounds,
    int16_t size,
    int16_t radius
);
static void draw_static_checkmark(
    GContext *ctx,
    GRect bounds
);
static void draw_confirmation_checkmark(
    GContext *ctx,
    GRect bounds
);
static bool first_unconfirmed_due_symbol(
    MedicationSymbol *symbol
);
static bool selected_confirmation_symbol(
    MedicationSymbol *symbol
);
static bool confirmation_prompt_is_active(
    MedicationSymbol *symbol
);
static int16_t transfer_lerp_int16(
    int16_t from,
    int16_t to,
    uint16_t progress
);
static GPoint transfer_lerp_point(
    GPoint from,
    GPoint to,
    uint16_t progress
);
static void draw_transfer_round_line(
    GContext *ctx,
    GPoint start,
    GPoint end,
    int16_t radius
);
static uint16_t transfer_progress_for_duration(
    uint16_t duration_ms
);
static int16_t transfer_shaft_bounce_offset(
    uint16_t progress
);
static void draw_transfer_icon(
    GContext *ctx,
    GRect bounds
);
static void confirm_medication_group(
    MedicationSymbol symbol
);
static bool confirmation_animation_active(void);
static void update_confirmation_circle(void);
static void update_checkmark(void);
static void confirmation_timer_callback(void *context);
static void schedule_confirmation_timer(void);
static void exit_app(void);
static void transfer_animation_timer_handler(
    void *context
);
static void schedule_transfer_animation_tick(void);
static void start_transfer_animation(void);
static void transfer_close_timer_handler(
    void *context
);

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
      bounds.origin.x +
          bounds.size.w +
          CONFIRM_CENTER_OUTSIDE_X,
      bounds.origin.y +
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

static void draw_round_line_with_radius(
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

static void draw_checkmark(
    GContext *ctx,
    GRect bounds,
    int16_t size,
    int16_t radius
) {
  const int16_t center_x =
      bounds.origin.x +
      bounds.size.w / 2;
  const int16_t center_y =
      bounds.origin.y +
      bounds.size.h / 2;

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
  draw_round_line_with_radius(
    ctx,
    start,
    middle,
    radius
  );
  draw_round_line_with_radius(
    ctx,
    middle,
    end,
    radius
  );
}

static void draw_static_checkmark(
    GContext *ctx,
    GRect bounds
) {
  const int16_t size = CHECK_POP_SETTLE_SIZE;
  const int16_t radius = CHECK_STROKE_RADIUS;
  const int16_t center_x =
      bounds.origin.x +
      bounds.size.w / 2;
  const int16_t center_y =
      bounds.origin.y +
      bounds.size.h / 2;

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

  /*
   * The animated checkmark is rasterized as many overlapping circles.
   * That is acceptable for a short confirmation animation, but far too
   * expensive for the 16 ms touch-scroll redraw loop. The static page uses
   * two thick strokes and only three circles for rounded joins and caps.
   */
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(
    ctx,
    (uint8_t)(radius * 2)
  );
  graphics_draw_line(ctx, start, middle);
  graphics_draw_line(ctx, middle, end);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, start, radius);
  graphics_fill_circle(ctx, middle, radius);
  graphics_fill_circle(ctx, end, radius);

  graphics_context_set_stroke_width(ctx, 1);
}

static void draw_confirmation_checkmark(
    GContext *ctx,
    GRect bounds
) {
  if (s_check_state == CHECK_HIDDEN || s_check_size <= 0) {
    return;
  }

  draw_checkmark(
    ctx,
    bounds,
    s_check_size,
    current_check_stroke_radius()
  );
}

void draw_confirmed_page(
    GContext *ctx,
    GRect bounds
) {
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

  draw_static_checkmark(
    ctx,
    bounds
  );
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

void update_taken_button_hint_pulse(void) {
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

void draw_taken_button_hint(
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

void confirmation_update_proc(
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

void select_button_down(
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

void schedule_transfer_close(void) {
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

void select_button_up(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (
    s_confirmed_screen_active &&
    !s_transfer_screen_active
  ) {
    return;
  }

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

void back_button_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  exit_app();
}

void show_transfer_screen(void) {
  cancel_timer(&s_transfer_close_timer);
  alarm_stop();

  s_transfer_screen_active = true;
  s_confirmed_screen_active = false;

  if (s_confirmation_layer) {
    GRect frame =
        layer_get_frame(s_confirmation_layer);
    frame.origin.x = 0;
    frame.origin.y = 0;
    layer_set_frame(
      s_confirmation_layer,
      frame
    );
  }

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
