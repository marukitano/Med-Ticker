#include <pebble.h>

#define FRAME_COUNT 8
#define UI_TICK_MS 110
#define PILL_TICKS_PER_FRAME 2

#define CANVAS_START_OFFSET_Y 0
#define BUTTON_SCROLL_REPEAT_MS 100

#define SCROLL_PIXELS_PER_DETENT 32
#define SCROLL_DETENT_THRESHOLD_PX   (SCROLL_PIXELS_PER_DETENT / 2)

#define SCROLL_ROLL_FRAME_MS 16
#define SCROLL_ROLL_STRENGTH_NUM 48
#define SCROLL_ROLL_STRENGTH_DEN 100

#define SCROLL_DRAG_PREVIEW_DIVISOR 2
#define SCROLL_DRAG_PREVIEW_MAX_PX 12

#define SCROLL_FLING_FRAME_MS 28
#define SCROLL_FLING_Q8 256
#define SCROLL_FLING_MIN_START_Q8   (4 * SCROLL_FLING_Q8)
#define SCROLL_FLING_MAX_SPEED_Q8   (36 * SCROLL_FLING_Q8)
#define SCROLL_FLING_STOP_SPEED_Q8   (SCROLL_FLING_Q8 / 2)
#define SCROLL_FLING_FRICTION_NUM 92
#define SCROLL_FLING_FRICTION_DEN 100

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

#define HINT_SPACING 14
#define HINT_HALF_WIDTH 9
#define HINT_HEIGHT 10
#define HINT_POSITION_ADJUST_Y -18

#define SIDE_HANDLE_RADIUS 6

#define MEDICATION_GAP 54
#define MEDICATION_HEADER_HEIGHT 28
#define MEDICATION_ROW_HEIGHT 50
#define MEDICATION_ROW_GAP 8
#define MEDICATION_COUNT 3

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

static const char *const s_medications[MEDICATION_COUNT] = {
  "Xarelto 20 mg",
  "Metformin 1000 mg",
  "Pantoprazol 40 mg"
};

static Window *s_window;
static Layer *s_canvas_layer;

static GBitmap *s_sheet;
static GBitmap *s_frame;

static AppTimer *s_ui_timer;
static AppTimer *s_confirmation_timer;
static AppTimer *s_scroll_roll_timer;
static AppTimer *s_scroll_fling_timer;

static GFont s_medication_font;

static int16_t s_frame_index;
static int16_t s_frame_width;
static int16_t s_frame_height;
static uint8_t s_ui_tick;
static uint8_t s_hint_phase;

static int32_t s_canvas_offset_y;
static int8_t s_snap_index;
static int16_t s_scroll_drag_accumulator;
static int16_t s_scroll_drag_preview_y;
static int16_t s_scroll_roll_offset_y;
static int32_t s_scroll_fling_velocity_q8;
static int32_t s_scroll_fling_position_q8;
static int32_t s_recent_drag_velocity_q8;
static bool s_scroll_touching;

#if defined(PBL_TOUCH)
static int16_t s_touch_last_y;
static bool s_dragging;
#endif

static int16_t s_confirm_radius;
static int16_t s_confirm_max_radius;
static ConfirmationState s_confirmation_state;

static int16_t s_check_size;
static CheckState s_check_state;

static void mark_canvas_dirty(void) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void cancel_timer(AppTimer **timer) {
  if (!*timer) {
    return;
  }

  app_timer_cancel(*timer);
  *timer = NULL;
}

static int32_t abs_int32(int32_t value) {
  return value < 0 ? -value : value;
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

  if (index > MEDICATION_COUNT) {
    return MEDICATION_COUNT;
  }

  return index;
}

static int32_t visual_canvas_offset_y(void) {
  return
      s_canvas_offset_y +
      s_scroll_roll_offset_y +
      s_scroll_drag_preview_y;
}

static void set_canvas_to_snap_index(int index) {
  s_snap_index = clamp_snap_index(index);
  s_canvas_offset_y =
      snap_anchor_for_index(s_snap_index);
  mark_canvas_dirty();
}

static GColor medication_background(int index) {
  switch (index) {
    case 0:
      return GColorRed;

    case 1:
      return GColorBlue;

    case 2:
      return GColorYellow;

    default:
      return GColorDarkGray;
  }
}

static GColor medication_text_color(int index) {
  return index == 2 ? GColorBlack : GColorWhite;
}

static void destroy_frame(void) {
  if (!s_frame) {
    return;
  }

  gbitmap_destroy(s_frame);
  s_frame = NULL;
}

static void set_frame(int index) {
  destroy_frame();

  s_frame = gbitmap_create_as_sub_bitmap(
    s_sheet,
    GRect(
      index * s_frame_width,
      0,
      s_frame_width,
      s_frame_height
    )
  );
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
      s_hint_offsets[s_hint_phase];

  if (hint_y < -HINT_HEIGHT - 2 ||
      hint_y > bounds.size.h + HINT_HEIGHT + 2) {
    return;
  }

  const int16_t x = bounds.size.w / 2;
  const int16_t y = (int16_t)hint_y;

  graphics_context_set_stroke_color(ctx, GColorLightGray);

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

static void draw_medication_text(
    GContext *ctx,
    GRect row,
    const char *text,
    int index
) {
  graphics_context_set_text_color(
    ctx,
    medication_text_color(index)
  );

  graphics_draw_text(
    ctx,
    text,
    s_medication_font,
    GRect(
      row.origin.x + 8,
      row.origin.y + 3,
      row.size.w - 16,
      row.size.h - 3
    ),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL
  );
}

static void draw_medications(
    GContext *ctx,
    GRect bounds,
    int32_t pill_y
) {
  const int32_t label_y =
      pill_y +
      s_frame_height +
      MEDICATION_GAP - 7 +
      HINT_POSITION_ADJUST_Y +
      HINT_HEIGHT +
      15;

  const int32_t rows_y =
      label_y +
      MEDICATION_HEADER_HEIGHT;

  const int32_t list_bottom =
      rows_y +
      MEDICATION_COUNT *
          (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP);

  if (list_bottom < 0 || label_y > bounds.size.h) {
    return;
  }

  graphics_context_set_text_color(ctx, GColorLightGray);

  graphics_draw_text(
    ctx,
    "MEDICATIONS",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(
      10,
      (int16_t)label_y,
      bounds.size.w - 20,
      MEDICATION_HEADER_HEIGHT
    ),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL
  );

  for (int index = 0; index < MEDICATION_COUNT; index++) {
    const int32_t row_y =
        rows_y +
        index * (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP);

    if (row_y + MEDICATION_ROW_HEIGHT < 0 ||
        row_y > bounds.size.h) {
      continue;
    }

    const GRect row = GRect(
      0,
      (int16_t)row_y,
      bounds.size.w,
      MEDICATION_ROW_HEIGHT
    );

    graphics_context_set_fill_color(
      ctx,
      medication_background(index)
    );
    graphics_fill_rect(ctx, row, 0, GCornerNone);

    draw_medication_text(
      ctx,
      row,
      s_medications[index],
      index
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

static void draw_side_handles(
    GContext *ctx,
    GRect bounds
) {
  graphics_context_set_fill_color(ctx, GColorWhite);

  graphics_fill_circle(
    ctx,
    GPoint(
      -1,
      bounds.size.h / 3 - 36
    ),
    SIDE_HANDLE_RADIUS
  );

  graphics_fill_circle(
    ctx,
    GPoint(
      bounds.size.w,
      bounds.size.h / 2
    ),
    SIDE_HANDLE_RADIUS
  );
}

static void canvas_update_proc(
    Layer *layer,
    GContext *ctx
) {
  const GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_frame) {
    const int32_t pill_y =
        ((int32_t)bounds.size.h - s_frame_height) / 2 +
        visual_canvas_offset_y();

    if (pill_y > -s_frame_height &&
        pill_y < bounds.size.h) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(
        ctx,
        s_frame,
        GRect(
          (bounds.size.w - s_frame_width) / 2,
          (int16_t)pill_y,
          s_frame_width,
          s_frame_height
        )
      );
    }

    draw_scroll_hint(ctx, bounds, pill_y);
    draw_medications(ctx, bounds, pill_y);
  }

  draw_side_handles(ctx, bounds);
  draw_confirmation_circle(ctx, bounds);
  draw_confirmation_checkmark(ctx, bounds);
}

static void ui_timer_callback(void *context) {
  s_ui_timer = NULL;

  if (!s_canvas_layer) {
    return;
  }

  s_hint_phase =
      (s_hint_phase + 1) %
      ARRAY_LENGTH(s_hint_offsets);

  s_ui_tick++;

  if (s_ui_tick >= PILL_TICKS_PER_FRAME) {
    s_ui_tick = 0;
    s_frame_index = (s_frame_index + 1) % FRAME_COUNT;
    set_frame(s_frame_index);
  }

  mark_canvas_dirty();

  s_ui_timer = app_timer_register(
    UI_TICK_MS,
    ui_timer_callback,
    NULL
  );
}

static void start_ui_timer(void) {
  cancel_timer(&s_ui_timer);

  s_ui_timer = app_timer_register(
    UI_TICK_MS,
    ui_timer_callback,
    NULL
  );
}

/*
 * This is the actual confirmation moment.
 * The pending 15-minute repeat wakeup will be cancelled here later.
 */
static void confirm_medication_taken(void) {
  /* TODO: Cancel the pending repeat wakeup. */
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
    confirm_medication_taken();
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
  mark_canvas_dirty();

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

static void cancel_scroll_roll(void) {
  cancel_timer(&s_scroll_roll_timer);
  s_scroll_roll_offset_y = 0;
  s_scroll_drag_preview_y = 0;
}

static void scroll_roll_tick(void *context) {
  s_scroll_roll_timer = NULL;

  if (s_scroll_roll_offset_y == 0) {
    return;
  }

  int16_t movement =
      (
        -s_scroll_roll_offset_y *
        SCROLL_ROLL_STRENGTH_NUM
      ) /
      SCROLL_ROLL_STRENGTH_DEN;

  if (movement == 0) {
    movement =
        s_scroll_roll_offset_y > 0
            ? -1
            : 1;
  }

  s_scroll_roll_offset_y += movement;

  if (abs_int32(s_scroll_roll_offset_y) <= 1) {
    s_scroll_roll_offset_y = 0;
  }

  mark_canvas_dirty();

  if (s_scroll_roll_offset_y != 0) {
    s_scroll_roll_timer = app_timer_register(
      SCROLL_ROLL_FRAME_MS,
      scroll_roll_tick,
      NULL
    );

    if (!s_scroll_roll_timer) {
      s_scroll_roll_offset_y = 0;
      mark_canvas_dirty();
    }
  }
}

static void schedule_scroll_roll_timer(void) {
  if (s_scroll_roll_timer) {
    return;
  }

  s_scroll_roll_timer = app_timer_register(
    SCROLL_ROLL_FRAME_MS,
    scroll_roll_tick,
    NULL
  );

  if (!s_scroll_roll_timer) {
    s_scroll_roll_offset_y = 0;
    mark_canvas_dirty();
  }
}

static void update_scroll_drag_preview(void) {
  int16_t preview =
      s_scroll_drag_accumulator /
      SCROLL_DRAG_PREVIEW_DIVISOR;

  if (preview > SCROLL_DRAG_PREVIEW_MAX_PX) {
    preview = SCROLL_DRAG_PREVIEW_MAX_PX;
  } else if (
    preview < -SCROLL_DRAG_PREVIEW_MAX_PX
  ) {
    preview = -SCROLL_DRAG_PREVIEW_MAX_PX;
  }

  s_scroll_drag_preview_y = preview;
  mark_canvas_dirty();
}

static void bounce_scroll_drag_preview_back(void) {
  if (s_scroll_drag_preview_y == 0) {
    return;
  }

  /*
   * Transfer the visible finger displacement into the existing
   * 48-percent roll animation. The content therefore continues
   * from its current position and snaps back without a jump.
   */
  s_scroll_roll_offset_y +=
      s_scroll_drag_preview_y;

  s_scroll_drag_preview_y = 0;
  schedule_scroll_roll_timer();
}

static void start_scroll_roll(
    int previous_index,
    int next_index
) {
  const int32_t previous_anchor =
      snap_anchor_for_index(previous_index);

  const int32_t next_anchor =
      snap_anchor_for_index(next_index);

  /*
   * Keep the old item visually in place for the first frame.
   * The accumulated offset then rolls quickly into the new detent.
   */
  s_scroll_roll_offset_y +=
      previous_anchor - next_anchor;

  set_canvas_to_snap_index(next_index);

  schedule_scroll_roll_timer();
}

static void cancel_scroll_fling(void) {
  cancel_timer(&s_scroll_fling_timer);
  s_scroll_fling_velocity_q8 = 0;
  s_scroll_fling_position_q8 = 0;
  s_recent_drag_velocity_q8 = 0;
}

static bool step_snap_index(int direction) {
  const int previous_index = s_snap_index;

  const int next_index =
      clamp_snap_index(
        previous_index + direction
      );

  if (next_index == previous_index) {
    return false;
  }

  start_scroll_roll(
    previous_index,
    next_index
  );

  return true;
}

static bool apply_scroll_drag_delta(int16_t delta_y) {
  if (delta_y == 0) {
    return false;
  }

  s_scroll_drag_accumulator += delta_y;

  if (delta_y < 0) {
    while (
      s_scroll_drag_accumulator <=
      -SCROLL_DETENT_THRESHOLD_PX
    ) {
      if (s_snap_index >= MEDICATION_COUNT) {
        s_scroll_drag_accumulator =
            -SCROLL_DETENT_THRESHOLD_PX + 1;
        return true;
      }

      s_scroll_drag_accumulator +=
          SCROLL_PIXELS_PER_DETENT;

      step_snap_index(1);
    }
  } else {
    while (
      s_scroll_drag_accumulator >=
      SCROLL_DETENT_THRESHOLD_PX
    ) {
      if (s_snap_index <= 0) {
        s_scroll_drag_accumulator =
            SCROLL_DETENT_THRESHOLD_PX - 1;
        return true;
      }

      s_scroll_drag_accumulator -=
          SCROLL_PIXELS_PER_DETENT;

      step_snap_index(-1);
    }
  }

  return false;
}

static void finish_scroll_fling(void) {
  cancel_scroll_fling();
  s_scroll_drag_accumulator = 0;
}

static void scroll_fling_tick(void *context) {
  s_scroll_fling_timer = NULL;

  if (
    !s_canvas_layer ||
    s_scroll_touching ||
    s_confirmation_state != CONFIRM_IDLE
  ) {
    cancel_scroll_fling();
    return;
  }

  s_scroll_fling_position_q8 +=
      s_scroll_fling_velocity_q8;

  const int16_t movement =
      (int16_t)(
        s_scroll_fling_position_q8 /
        SCROLL_FLING_Q8
      );

  s_scroll_fling_position_q8 -=
      (int32_t)movement *
      SCROLL_FLING_Q8;

  const bool reached_boundary =
      movement != 0 &&
      apply_scroll_drag_delta(movement);

  s_scroll_fling_velocity_q8 =
      (
        s_scroll_fling_velocity_q8 *
        SCROLL_FLING_FRICTION_NUM
      ) /
      SCROLL_FLING_FRICTION_DEN;

  if (
    reached_boundary ||
    abs_int32(s_scroll_fling_velocity_q8) <
        SCROLL_FLING_STOP_SPEED_Q8
  ) {
    finish_scroll_fling();
    return;
  }

  mark_canvas_dirty();

  s_scroll_fling_timer = app_timer_register(
    SCROLL_FLING_FRAME_MS,
    scroll_fling_tick,
    NULL
  );

  if (!s_scroll_fling_timer) {
    finish_scroll_fling();
  }
}

static bool start_scroll_fling(void) {
  const int32_t requested_velocity =
      s_recent_drag_velocity_q8;

  if (
    abs_int32(requested_velocity) <
        SCROLL_FLING_MIN_START_Q8
  ) {
    cancel_scroll_fling();
    return false;
  }

  cancel_timer(&s_scroll_fling_timer);

  if (requested_velocity >
      SCROLL_FLING_MAX_SPEED_Q8) {
    s_scroll_fling_velocity_q8 =
        SCROLL_FLING_MAX_SPEED_Q8;
  } else if (
    requested_velocity <
    -SCROLL_FLING_MAX_SPEED_Q8
  ) {
    s_scroll_fling_velocity_q8 =
        -SCROLL_FLING_MAX_SPEED_Q8;
  } else {
    s_scroll_fling_velocity_q8 =
        requested_velocity;
  }

  s_scroll_fling_position_q8 = 0;

  s_scroll_fling_timer = app_timer_register(
    SCROLL_FLING_FRAME_MS,
    scroll_fling_tick,
    NULL
  );

  if (!s_scroll_fling_timer) {
    cancel_scroll_fling();
    return false;
  }

  return true;
}

static void scroll_up_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state != CONFIRM_IDLE) {
    return;
  }

  cancel_scroll_fling();
  s_scroll_drag_accumulator = 0;
  step_snap_index(-1);
}

static void scroll_down_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state != CONFIRM_IDLE) {
    return;
  }

  cancel_scroll_fling();
  s_scroll_drag_accumulator = 0;
  step_snap_index(1);
}

static void select_button_down(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state != CONFIRM_IDLE) {
    return;
  }

  s_confirmation_state = CONFIRM_GROWING;
  schedule_confirmation_timer();
}

static void exit_app(void) {
  window_stack_pop_all(true);
}

static void select_button_up(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state == CONFIRM_COMPLETE) {
    exit_app();
    return;
  }

  if (s_confirm_radius <= 0) {
    s_confirmation_state = CONFIRM_IDLE;
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
  window_single_repeating_click_subscribe(
    BUTTON_ID_UP,
    BUTTON_SCROLL_REPEAT_MS,
    scroll_up_handler
  );

  window_raw_click_subscribe(
    BUTTON_ID_SELECT,
    select_button_down,
    select_button_up,
    NULL
  );

  window_single_repeating_click_subscribe(
    BUTTON_ID_DOWN,
    BUTTON_SCROLL_REPEAT_MS,
    scroll_down_handler
  );

  window_single_click_subscribe(
    BUTTON_ID_BACK,
    back_button_handler
  );
}

#if defined(PBL_TOUCH)
static void touch_handler(
    const TouchEvent *event,
    void *context
) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      if (s_confirmation_state != CONFIRM_IDLE) {
        return;
      }

      cancel_scroll_fling();
      cancel_scroll_roll();
      s_dragging = true;
      s_scroll_touching = true;
      s_touch_last_y = event->y;
      s_scroll_drag_accumulator = 0;
      s_recent_drag_velocity_q8 = 0;
      break;

    case TouchEvent_PositionUpdate:
      if (s_dragging) {
        const int16_t delta_y =
            event->y - s_touch_last_y;

        s_touch_last_y = event->y;

        if (delta_y == 0) {
          break;
        }

        /*
         * Same weighted velocity estimate as Swiss Chronograph:
         * newest movement counts strongly, but the last tiny event
         * cannot erase a fast swipe.
         */
        s_recent_drag_velocity_q8 =
            (
              s_recent_drag_velocity_q8 +
              (
                (int32_t)delta_y *
                SCROLL_FLING_Q8 *
                3
              )
            ) /
            4;

        apply_scroll_drag_delta(delta_y);
        update_scroll_drag_preview();
      }
      break;

    case TouchEvent_Liftoff:
      if (s_dragging) {
        s_dragging = false;
        s_scroll_touching = false;

        const bool fling_started =
            start_scroll_fling();

        if (fling_started) {
          /*
           * A fast swipe continues through real detents.
           * The small preview displacement must not remain.
           */
          s_scroll_drag_preview_y = 0;
        } else {
          /*
           * A short pull that did not cross the threshold
           * visibly snaps back to the current detent.
           */
          bounce_scroll_drag_preview_back();
          s_recent_drag_velocity_q8 = 0;
        }

        s_scroll_drag_accumulator = 0;
        mark_canvas_dirty();
      }
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

  const GRect sheet_bounds = gbitmap_get_bounds(s_sheet);

  if (sheet_bounds.size.w % FRAME_COUNT == 0) {
    s_frame_width = sheet_bounds.size.w / FRAME_COUNT;
    s_frame_height = sheet_bounds.size.h;
    return true;
  }

  APP_LOG(
    APP_LOG_LEVEL_ERROR,
    "Spritesheet width is invalid"
  );

  gbitmap_destroy(s_sheet);
  s_sheet = NULL;
  return false;
}

static void reset_ui_state(GRect bounds) {
  s_frame_index = 0;
  s_ui_tick = 0;
  s_hint_phase = 0;
  s_canvas_offset_y = CANVAS_START_OFFSET_Y;
  s_snap_index = 0;
  s_scroll_drag_accumulator = 0;
  s_scroll_drag_preview_y = 0;
  s_scroll_roll_offset_y = 0;
  s_scroll_fling_velocity_q8 = 0;
  s_scroll_fling_position_q8 = 0;
  s_recent_drag_velocity_q8 = 0;
  s_scroll_touching = false;

#if defined(PBL_TOUCH)
  s_dragging = false;
  s_touch_last_y = 0;
#endif

  s_confirm_radius = 0;
  s_confirm_max_radius =
      bounds.size.w +
      CONFIRM_CENTER_OUTSIDE_X +
      bounds.size.h / 2;
  s_confirmation_state = CONFIRM_IDLE;

  s_check_size = 0;
  s_check_state = CHECK_HIDDEN;
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  s_medication_font =
      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  if (!load_pill_sheet()) {
    return;
  }

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(
    s_canvas_layer,
    canvas_update_proc
  );
  layer_add_child(root, s_canvas_layer);

  reset_ui_state(bounds);
  set_frame(s_frame_index);
}

static void window_appear(Window *window) {
  if (s_canvas_layer) {
    start_ui_timer();
  }

#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
}

static void window_disappear(Window *window) {
  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_scroll_fling();
  cancel_scroll_roll();

#if defined(PBL_TOUCH)
  s_dragging = false;
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_scroll_fling();
  cancel_scroll_roll();
  destroy_frame();

  if (s_sheet) {
    gbitmap_destroy(s_sheet);
    s_sheet = NULL;
  }

  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);

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
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
