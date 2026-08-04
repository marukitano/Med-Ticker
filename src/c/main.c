#include <pebble.h>

#define FRAME_COUNT 8
#define UI_TICK_MS 110
#define PILL_TICKS_PER_FRAME 2

#define CANVAS_START_OFFSET_Y 0
#define BUTTON_SCROLL_REPEAT_MS 100

#define SCROLL_PHYSICS_Q8 256
#define SCROLL_PHYSICS_FRAME_MS 16

/*
 * Der Finger zieht die sichtbare Position über eine Feder.
 * Je größer dieser Wert, desto direkter folgt der Inhalt.
 */
#define SCROLL_FINGER_SPRING_NUM 18
#define SCROLL_FINGER_SPRING_DEN 100

/*
 * Statische Haftung am Rastpunkt:
 * Der Finger spannt zuerst rund 9 Pixel Federweg auf,
 * bevor sich die sichtbare Position lösen darf.
 */
#define SCROLL_BREAKAWAY_DISTANCE_PX 9

#define SCROLL_QUICK_SWIPE_MIN_DISTANCE_PX 5
#define SCROLL_QUICK_SWIPE_MAX_DURATION_MS 230
#define SCROLL_BREAKAWAY_FORCE_Q8 \
  ( \
    ( \
      SCROLL_BREAKAWAY_DISTANCE_PX * \
      SCROLL_PHYSICS_Q8 * \
      SCROLL_FINGER_SPRING_NUM \
    ) / \
    SCROLL_FINGER_SPRING_DEN \
  )

/*
 * Die Magnetkraft wird proportional zum Abstand der
 * Rastpunkte skaliert. Dadurch fühlt sich auch der große
 * Abstand zwischen Pille und erster Tablette ähnlich an.
 */
#define SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8 12

#define SCROLL_EDGE_OVERSCROLL_LIMIT_PX \
  ((MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP) / 2)

#define SCROLL_TOUCH_DAMPING_NUM 68
#define SCROLL_TOUCH_DAMPING_DEN 100

#define SCROLL_SETTLE_SPRING_NUM 24
#define SCROLL_SETTLE_SPRING_DEN 100

/*
 * Die automatische Einrastfeder darf höchstens so
 * stark beschleunigen wie bei einem normalen Abstand
 * zwischen zwei Medikamentenzeilen.
 *
 * Dadurch bleibt der große Weg Pille -> erste Zeile
 * erhalten, erzeugt am Ziel aber keinen größeren Bounce.
 */
#define SCROLL_SETTLE_REFERENCE_DISTANCE_PX \
  (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP)

#define SCROLL_SETTLE_MAX_FORCE_Q8 \
  ( \
    ( \
      SCROLL_SETTLE_REFERENCE_DISTANCE_PX * \
      SCROLL_PHYSICS_Q8 * \
      SCROLL_SETTLE_SPRING_NUM \
    ) / \
    SCROLL_SETTLE_SPRING_DEN \
  )

#define SCROLL_SETTLE_DAMPING_NUM 62
#define SCROLL_SETTLE_DAMPING_DEN 100

#define SCROLL_MAX_VELOCITY_Q8 \
  (32 * SCROLL_PHYSICS_Q8)

#define SCROLL_STOP_POSITION_Q8 \
  (SCROLL_PHYSICS_Q8 / 4)

#define SCROLL_STOP_VELOCITY_Q8 \
  (SCROLL_PHYSICS_Q8 / 4)

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
static AppTimer *s_scroll_physics_timer;

static GFont s_medication_font;

static int16_t s_frame_index;
static int16_t s_frame_width;
static int16_t s_frame_height;
static uint8_t s_ui_tick;
static uint8_t s_hint_phase;

static int32_t s_canvas_offset_y;
static int8_t s_snap_index;
static int32_t s_scroll_position_q8;
static int32_t s_scroll_velocity_q8;

static int32_t s_scroll_finger_target_q8;
static int32_t s_scroll_settle_target_q8;

static bool s_scroll_touching;
static bool s_scroll_settling;

static bool s_button_edge_bounce_outward;
static int8_t s_button_edge_bounce_snap_index;

static bool s_scroll_breakaway_locked;
static int32_t s_scroll_breakaway_anchor_q8;

#if defined(PBL_TOUCH)
static int16_t s_touch_last_y;
static int16_t s_touch_total_delta_y;
static uint32_t s_touch_start_time_ms;

static int8_t s_touch_start_snap_index;
static int8_t s_touch_neighbor_snap_index;
static int8_t s_touch_pair_direction;

static bool s_touch_pair_selected;
static bool s_touch_gesture_consumed;
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

  if (index > MEDICATION_COUNT) {
    return MEDICATION_COUNT;
  }

  return index;
}

static int32_t visual_canvas_offset_y(void) {
  if (s_scroll_position_q8 >= 0) {
    return
        (
          s_scroll_position_q8 +
          SCROLL_PHYSICS_Q8 / 2
        ) /
        SCROLL_PHYSICS_Q8;
  }

  return
      (
        s_scroll_position_q8 -
        SCROLL_PHYSICS_Q8 / 2
      ) /
      SCROLL_PHYSICS_Q8;
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

static int nearest_snap_index_for_position_q8(
    int32_t position_q8
) {
  const int current_index =
      clamp_snap_index(s_snap_index);

  const int32_t current_anchor_q8 =
      snap_anchor_for_index(current_index) *
      SCROLL_PHYSICS_Q8;

  const int32_t escape_distance_q8 =
      SCROLL_EDGE_OVERSCROLL_LIMIT_PX *
      SCROLL_PHYSICS_Q8;

  /*
   * Jeder Rastpunkt hat denselben Fluchtradius.
   * Das gilt auch für den großen sichtbaren Abstand
   * zwischen Pille und erster Medikamentenzeile.
   *
   * Vor 29 Pixeln gewinnt der aktuelle Rastpunkt.
   * Ab 29 Pixeln gewinnt genau der benachbarte.
   */
  if (
    position_q8 >=
        current_anchor_q8 +
        escape_distance_q8 &&
    current_index > 0
  ) {
    return current_index - 1;
  }

  if (
    position_q8 <=
        current_anchor_q8 -
        escape_distance_q8 &&
    current_index < MEDICATION_COUNT
  ) {
    return current_index + 1;
  }

  return current_index;
}

static int32_t magnet_force_for_position_q8(
    int32_t position_q8
) {
  const int32_t top_anchor_q8 =
      snap_anchor_for_index(0) *
      SCROLL_PHYSICS_Q8;

  const int32_t bottom_anchor_q8 =
      snap_anchor_for_index(MEDICATION_COUNT) *
      SCROLL_PHYSICS_Q8;

  /*
   * Außerhalb der äußeren Rastpunkte wirkt ein
   * starker elastischer Anschlag. Dadurch kann der
   * Inhalt nur ein kleines Stück über den Rand hinaus.
   */
  if (position_q8 >= top_anchor_q8) {
    /*
     * Oberhalb der Pille liegt ein virtueller
     * Rastpunkt im normalen Medikamentenabstand.
     * Erreichbar ist nur dessen erste Hälfte.
     */
    const int32_t virtual_anchor_q8 =
        top_anchor_q8 +
        (
          MEDICATION_ROW_HEIGHT +
          MEDICATION_ROW_GAP
        ) *
        SCROLL_PHYSICS_Q8;

    const int32_t distance_q8 =
        virtual_anchor_q8 -
        top_anchor_q8;

    const int32_t travelled_q8 =
        position_q8 -
        top_anchor_q8;

    int32_t progress_q8 =
        (
          travelled_q8 *
          SCROLL_PHYSICS_Q8
        ) /
        distance_q8;

    if (
      progress_q8 >
      SCROLL_PHYSICS_Q8 / 2
    ) {
      progress_q8 =
          SCROLL_PHYSICS_Q8 / 2;
    }

    const int32_t angle =
        (
          progress_q8 *
          TRIG_MAX_ANGLE
        ) /
        SCROLL_PHYSICS_Q8;

    const int32_t sine =
        sin_lookup(angle);

    const int32_t distance_pixels =
        distance_q8 /
        SCROLL_PHYSICS_Q8;

    const int32_t maximum_force_q8 =
        distance_pixels *
        SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8;

    return
        -(
          (
            maximum_force_q8 *
            sine
          ) /
          TRIG_MAX_RATIO
        );
  }

  if (position_q8 <= bottom_anchor_q8) {
    /*
     * Unter dem letzten echten Rastpunkt liegt ein
     * unsichtbarer virtueller Rastpunkt im gleichen
     * Abstand wie die Medikamentenzeilen.
     *
     * Der Finger darf nur bis zur Mitte dorthin.
     * Deshalb erleben wir exakt die erste Hälfte
     * derselben Magnetkurve wie zwischen zwei echten
     * Medikamenten, können aber nie zum virtuellen
     * Rastpunkt wechseln.
     */
    const int32_t virtual_anchor_q8 =
        bottom_anchor_q8 -
        (
          MEDICATION_ROW_HEIGHT +
          MEDICATION_ROW_GAP
        ) *
        SCROLL_PHYSICS_Q8;

    const int32_t distance_q8 =
        bottom_anchor_q8 -
        virtual_anchor_q8;

    const int32_t travelled_q8 =
        bottom_anchor_q8 -
        position_q8;

    int32_t progress_q8 =
        (
          travelled_q8 *
          SCROLL_PHYSICS_Q8
        ) /
        distance_q8;

    if (
      progress_q8 >
      SCROLL_PHYSICS_Q8 / 2
    ) {
      progress_q8 =
          SCROLL_PHYSICS_Q8 / 2;
    }

    const int32_t angle =
        (
          progress_q8 *
          TRIG_MAX_ANGLE
        ) /
        SCROLL_PHYSICS_Q8;

    const int32_t sine =
        sin_lookup(angle);

    const int32_t distance_pixels =
        distance_q8 /
        SCROLL_PHYSICS_Q8;

    const int32_t maximum_force_q8 =
        distance_pixels *
        SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8;

    return
        (
          maximum_force_q8 *
          sine
        ) /
        TRIG_MAX_RATIO;
  }

  for (
    int index = 0;
    index < MEDICATION_COUNT;
    index++
  ) {
    const int32_t first_anchor_q8 =
        snap_anchor_for_index(index) *
        SCROLL_PHYSICS_Q8;

    const int32_t second_anchor_q8 =
        snap_anchor_for_index(index + 1) *
        SCROLL_PHYSICS_Q8;

    if (
      position_q8 <= first_anchor_q8 &&
      position_q8 >= second_anchor_q8
    ) {
      const int32_t distance_q8 =
          first_anchor_q8 -
          second_anchor_q8;

      if (distance_q8 <= 0) {
        return 0;
      }

      const int32_t travelled_q8 =
          first_anchor_q8 -
          position_q8;

      const int32_t progress_q8 =
          (
            travelled_q8 *
            SCROLL_PHYSICS_Q8
          ) /
          distance_q8;

      const int32_t angle =
          (
            progress_q8 *
            TRIG_MAX_ANGLE
          ) /
          SCROLL_PHYSICS_Q8;

      const int32_t sine =
          sin_lookup(angle);

      const int32_t distance_pixels =
          distance_q8 /
          SCROLL_PHYSICS_Q8;

      const int32_t maximum_force_q8 =
          distance_pixels *
          SCROLL_MAGNET_ACCEL_PER_PIXEL_Q8;

      /*
       * Erste Hälfte: positive Kraft zurück zum
       * ersten Magneten.
       *
       * Zweite Hälfte: negative Kraft vorwärts zum
       * nächsten Magneten.
       *
       * Exakt in der Mitte sind beide Magnetkräfte
       * gleich stark und heben sich auf.
       */
      return
          (
            maximum_force_q8 *
            sine
          ) /
          TRIG_MAX_RATIO;
    }
  }

  return 0;
}

static int32_t clamp_scroll_velocity_q8(
    int32_t velocity_q8
) {
  if (
    velocity_q8 >
    SCROLL_MAX_VELOCITY_Q8
  ) {
    return SCROLL_MAX_VELOCITY_Q8;
  }

  if (
    velocity_q8 <
    -SCROLL_MAX_VELOCITY_Q8
  ) {
    return -SCROLL_MAX_VELOCITY_Q8;
  }

  return velocity_q8;
}

static int32_t clamp_settle_force_q8(
    int32_t force_q8
) {
  if (
    force_q8 >
    SCROLL_SETTLE_MAX_FORCE_Q8
  ) {
    return SCROLL_SETTLE_MAX_FORCE_Q8;
  }

  if (
    force_q8 <
    -SCROLL_SETTLE_MAX_FORCE_Q8
  ) {
    return -SCROLL_SETTLE_MAX_FORCE_Q8;
  }

  return force_q8;
}

static bool scroll_physics_active(void) {
  return
      s_scroll_touching ||
      s_scroll_settling;
}

static void cancel_scroll_physics(void) {
  cancel_timer(&s_scroll_physics_timer);

  s_scroll_touching = false;
  s_scroll_settling = false;
  s_button_edge_bounce_outward = false;
  s_scroll_breakaway_locked = false;
  s_scroll_velocity_q8 = 0;
}

static void schedule_scroll_physics(void);

static void start_scroll_settle_to_index(
    int target_index,
    bool keep_velocity
);

static void start_canonical_snap_bounce(
    int start_index,
    int target_index
);

static void scroll_physics_tick(void *context) {
  s_scroll_physics_timer = NULL;

  if (!scroll_physics_active()) {
    return;
  }

  int32_t acceleration_q8 = 0;

  if (s_scroll_touching) {
    const int32_t finger_force_q8 =
        (
          (
            s_scroll_finger_target_q8 -
            s_scroll_position_q8
          ) *
          SCROLL_FINGER_SPRING_NUM
        ) /
        SCROLL_FINGER_SPRING_DEN;

    /*
     * Solange die statische Haftung nicht überwunden
     * ist, bleibt der Inhalt pixelgenau am Rastpunkt.
     * Das Fingerziel bewegt sich trotzdem weiter und
     * baut dadurch echte Federspannung auf.
     */
    if (s_scroll_breakaway_locked) {
      if (
        abs_int32(finger_force_q8) <
        SCROLL_BREAKAWAY_FORCE_Q8
      ) {
        s_scroll_position_q8 =
            s_scroll_breakaway_anchor_q8;

        s_scroll_velocity_q8 = 0;

        schedule_scroll_physics();
        return;
      }

      /*
       * Losbrechmoment erreicht: Die bereits gespannte
       * Feder bleibt erhalten und setzt die Bewegung in
       * derselben Richtung in Gang.
       */
      s_scroll_breakaway_locked = false;
      s_scroll_position_q8 =
          s_scroll_breakaway_anchor_q8;
      s_scroll_velocity_q8 = 0;
    }

    const int32_t magnet_force_q8 =
        magnet_force_for_position_q8(
          s_scroll_position_q8
        );

    acceleration_q8 =
        finger_force_q8 +
        magnet_force_q8;

    s_scroll_velocity_q8 +=
        acceleration_q8;

    s_scroll_velocity_q8 =
        (
          s_scroll_velocity_q8 *
          SCROLL_TOUCH_DAMPING_NUM
        ) /
        SCROLL_TOUCH_DAMPING_DEN;
  } else {
    const int32_t settle_force_q8 =
        clamp_settle_force_q8(
          (
            (
              s_scroll_settle_target_q8 -
              s_scroll_position_q8
            ) *
            SCROLL_SETTLE_SPRING_NUM
          ) /
          SCROLL_SETTLE_SPRING_DEN
        );

    const int32_t magnet_force_q8 =
        magnet_force_for_position_q8(
          s_scroll_position_q8
        );

    acceleration_q8 =
        settle_force_q8 +
        magnet_force_q8;

    s_scroll_velocity_q8 +=
        acceleration_q8;

    s_scroll_velocity_q8 =
        (
          s_scroll_velocity_q8 *
          SCROLL_SETTLE_DAMPING_NUM
        ) /
        SCROLL_SETTLE_DAMPING_DEN;
  }

  s_scroll_velocity_q8 =
      clamp_scroll_velocity_q8(
        s_scroll_velocity_q8
      );

  s_scroll_position_q8 +=
      s_scroll_velocity_q8;

  /*
   * Auch die sichtbare Position selbst darf den
   * Mittelpunkt zum virtuellen Rastpunkt nicht
   * durch ihre Geschwindigkeit überschießen.
   */
  const int32_t top_limit_q8 =
      snap_anchor_for_index(0) *
      SCROLL_PHYSICS_Q8 +
      SCROLL_EDGE_OVERSCROLL_LIMIT_PX *
      SCROLL_PHYSICS_Q8;

  const int32_t bottom_limit_q8 =
      snap_anchor_for_index(
        MEDICATION_COUNT
      ) *
      SCROLL_PHYSICS_Q8 -
      SCROLL_EDGE_OVERSCROLL_LIMIT_PX *
      SCROLL_PHYSICS_Q8;

  if (
    s_scroll_position_q8 >
    top_limit_q8
  ) {
    s_scroll_position_q8 =
        top_limit_q8;

    if (s_scroll_velocity_q8 > 0) {
      s_scroll_velocity_q8 = 0;
    }
  }

  if (
    s_scroll_position_q8 <
    bottom_limit_q8
  ) {
    s_scroll_position_q8 =
        bottom_limit_q8;

    if (s_scroll_velocity_q8 < 0) {
      s_scroll_velocity_q8 = 0;
    }
  }

  /*
   * Eine Taste am Listenrand fährt zunächst bis zum
   * virtuellen Halb-Rastpunkt. Sobald die sichtbare
   * Randgrenze erreicht ist, übernimmt wieder dieselbe
   * Federphysik und bringt den Inhalt zum echten
   * Rand-Rastpunkt zurück.
   */
  if (s_button_edge_bounce_outward) {
    const bool reached_top_edge =
        s_button_edge_bounce_snap_index == 0 &&
        s_scroll_position_q8 >= top_limit_q8;

    const bool reached_bottom_edge =
        s_button_edge_bounce_snap_index ==
            MEDICATION_COUNT &&
        s_scroll_position_q8 <= bottom_limit_q8;

    if (
      reached_top_edge ||
      reached_bottom_edge
    ) {
      const int return_index =
          s_button_edge_bounce_snap_index;

      s_button_edge_bounce_outward = false;
      s_scroll_velocity_q8 = 0;

      start_scroll_settle_to_index(
        return_index,
        false
      );

      mark_canvas_dirty();
      return;
    }
  }

#if defined(PBL_TOUCH)
  /*
   * Echte Rastpunkt-Paare bleiben während des gesamten
   * Touchs interaktiv. Nur ein virtueller Tabellenrand
   * verbraucht die Geste weiterhin beim Erreichen seiner
   * halben Raststrecke.
   */
  if (
    s_scroll_touching &&
    s_touch_pair_selected &&
    !s_touch_gesture_consumed &&
    s_touch_neighbor_snap_index ==
        s_touch_start_snap_index
  ) {
    const bool reached_virtual_edge =
        (
          s_touch_pair_direction < 0 &&
          s_touch_start_snap_index == 0 &&
          s_scroll_position_q8 >=
              top_limit_q8
        ) ||
        (
          s_touch_pair_direction > 0 &&
          s_touch_start_snap_index ==
              MEDICATION_COUNT &&
          s_scroll_position_q8 <=
              bottom_limit_q8
        );

    if (reached_virtual_edge) {
      s_touch_gesture_consumed = true;

      start_scroll_settle_to_index(
        s_touch_start_snap_index,
        true
      );

      mark_canvas_dirty();
      return;
    }
  }
#endif

  if (
    s_scroll_settling &&
    abs_int32(
      s_scroll_settle_target_q8 -
      s_scroll_position_q8
    ) <= SCROLL_STOP_POSITION_Q8 &&
    abs_int32(
      s_scroll_velocity_q8
    ) <= SCROLL_STOP_VELOCITY_Q8
  ) {
    s_scroll_position_q8 =
        s_scroll_settle_target_q8;

    s_scroll_velocity_q8 = 0;
    s_scroll_settling = false;
  }

  mark_canvas_dirty();

  if (scroll_physics_active()) {
    schedule_scroll_physics();
  }
}

static void schedule_scroll_physics(void) {
  if (s_scroll_physics_timer) {
    return;
  }

  s_scroll_physics_timer = app_timer_register(
    SCROLL_PHYSICS_FRAME_MS,
    scroll_physics_tick,
    NULL
  );

  if (!s_scroll_physics_timer) {
    if (s_scroll_settling) {
      s_scroll_position_q8 =
          s_scroll_settle_target_q8;

      s_scroll_velocity_q8 = 0;
      s_scroll_settling = false;
      mark_canvas_dirty();
    }
  }
}

static void start_scroll_settle_to_index(
    int target_index,
    bool keep_velocity
) {
  target_index =
      clamp_snap_index(target_index);

#if defined(PBL_TOUCH)
  s_dragging = false;
#endif

  s_scroll_touching = false;
  s_scroll_settling = true;
  s_button_edge_bounce_outward = false;
  s_scroll_breakaway_locked = false;

  if (!keep_velocity) {
    s_scroll_velocity_q8 = 0;
  }

  s_scroll_settle_target_q8 =
      snap_anchor_for_index(target_index) *
      SCROLL_PHYSICS_Q8;

  set_canvas_to_snap_index(target_index);

  if (
    s_scroll_position_q8 ==
    s_scroll_settle_target_q8
  ) {
    s_scroll_velocity_q8 = 0;
    s_scroll_settling = false;
    mark_canvas_dirty();
    return;
  }

  schedule_scroll_physics();
}

static void start_button_edge_bounce(
    int direction
) {
  cancel_timer(&s_scroll_physics_timer);

  const int edge_index =
      clamp_snap_index(s_snap_index);

  const int32_t edge_anchor_q8 =
      snap_anchor_for_index(edge_index) *
      SCROLL_PHYSICS_Q8;

  const int32_t half_interval_q8 =
      SCROLL_EDGE_OVERSCROLL_LIMIT_PX *
      SCROLL_PHYSICS_Q8;

  s_scroll_touching = false;
  s_scroll_settling = true;
  s_scroll_breakaway_locked = false;

  s_button_edge_bounce_outward = true;
  s_button_edge_bounce_snap_index =
      edge_index;

  s_scroll_velocity_q8 = 0;

  /*
   * Das physikalische Ziel liegt bewusst hinter der
   * sichtbaren Randgrenze. Dadurch erreicht die Anzeige
   * trotz Magnet- und Federgegenkraft zuverlässig den
   * virtuellen Halb-Rastpunkt. Die sichtbare Position
   * selbst bleibt weiterhin auf die halbe Strecke
   * begrenzt.
   */
  s_scroll_settle_target_q8 =
      edge_anchor_q8 +
      (
        direction < 0
            ? 2 * half_interval_q8
            : -2 * half_interval_q8
      );

  schedule_scroll_physics();
}

/*
 * Erzeugt exakt den gleichen Ziel-Bounce für Tasten,
 * Quick-Swipe und normales Fingerziehen.
 *
 * Dafür wird die bestehende Settle-Feder intern vom
 * Ausgangsrastpunkt bis zum ersten Überqueren des
 * Zielrastpunkts simuliert. Position und Geschwindigkeit
 * dieses Moments werden anschließend als gemeinsamer
 * Startzustand für den sichtbaren Bounce verwendet.
 */
static void start_canonical_snap_bounce(
    int start_index,
    int target_index
) {
  start_index =
      clamp_snap_index(start_index);

  target_index =
      clamp_snap_index(target_index);

  if (start_index == target_index) {
    start_scroll_settle_to_index(
      target_index,
      false
    );
    return;
  }

  const int32_t start_anchor_q8 =
      snap_anchor_for_index(start_index) *
      SCROLL_PHYSICS_Q8;

  const int32_t target_anchor_q8 =
      snap_anchor_for_index(target_index) *
      SCROLL_PHYSICS_Q8;

  int32_t simulated_position_q8 =
      start_anchor_q8;

  int32_t simulated_velocity_q8 = 0;

  for (int step = 0; step < 32; step++) {
    const int32_t settle_force_q8 =
        clamp_settle_force_q8(
          (
            (
              target_anchor_q8 -
              simulated_position_q8
            ) *
            SCROLL_SETTLE_SPRING_NUM
          ) /
          SCROLL_SETTLE_SPRING_DEN
        );

    simulated_velocity_q8 +=
        settle_force_q8;

    simulated_velocity_q8 =
        (
          simulated_velocity_q8 *
          SCROLL_SETTLE_DAMPING_NUM
        ) /
        SCROLL_SETTLE_DAMPING_DEN;

    simulated_velocity_q8 =
        clamp_scroll_velocity_q8(
          simulated_velocity_q8
        );

    simulated_position_q8 +=
        simulated_velocity_q8;

    const bool crossed_target =
        (
          target_anchor_q8 <
          start_anchor_q8 &&
          simulated_position_q8 <=
              target_anchor_q8
        ) ||
        (
          target_anchor_q8 >
          start_anchor_q8 &&
          simulated_position_q8 >=
              target_anchor_q8
        );

    if (crossed_target) {
      break;
    }
  }

  cancel_timer(&s_scroll_physics_timer);

#if defined(PBL_TOUCH)
  s_dragging = false;
#endif

  s_scroll_touching = false;
  s_scroll_settling = true;
  s_button_edge_bounce_outward = false;
  s_scroll_breakaway_locked = false;

  s_scroll_settle_target_q8 =
      target_anchor_q8;

  s_scroll_position_q8 =
      simulated_position_q8;

  s_scroll_velocity_q8 =
      simulated_velocity_q8;

  set_canvas_to_snap_index(target_index);
  mark_canvas_dirty();
  schedule_scroll_physics();
}

static bool step_snap_index(int direction) {
  const int next_index =
      clamp_snap_index(
        s_snap_index + direction
      );

  if (next_index == s_snap_index) {
    const bool at_top_edge =
        s_snap_index == 0 &&
        direction < 0;

    const bool at_bottom_edge =
        s_snap_index == MEDICATION_COUNT &&
        direction > 0;

    if (
      at_top_edge ||
      at_bottom_edge
    ) {
      start_button_edge_bounce(direction);
      return true;
    }

    return false;
  }

  /*
   * Bei den Tasten wird der vollständige Weg zum
   * nächsten Rastpunkt sichtbar mit der normalen
   * Federphysik gefahren. Der Ziel-Bounce entsteht
   * anschließend natürlich aus dieser Bewegung.
   */
  start_scroll_settle_to_index(
    next_index,
    true
  );

  return true;
}

static void scroll_up_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state != CONFIRM_IDLE) {
    return;
  }

  step_snap_index(-1);
}

static void scroll_down_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state != CONFIRM_IDLE) {
    return;
  }

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

      /*
       * Wie beim PebbleOS-ScrollLayer übernimmt der
       * Finger eine laufende Bewegung sofort an der
       * aktuell sichtbaren Position.
       */
      cancel_timer(&s_scroll_physics_timer);

      s_dragging = true;
      s_scroll_touching = true;
      s_scroll_settling = false;

      s_touch_last_y = event->y;
      s_touch_total_delta_y = 0;
      s_touch_start_time_ms = current_time_ms();

      s_touch_start_snap_index =
          clamp_snap_index(s_snap_index);

      s_touch_neighbor_snap_index =
          s_touch_start_snap_index;

      s_touch_pair_direction = 0;
      s_touch_pair_selected = false;
      s_touch_gesture_consumed = false;

      s_scroll_finger_target_q8 =
          s_scroll_position_q8;

      /*
       * Die Haftung wird nur aktiviert, wenn der Inhalt
       * beim Aufsetzen bereits praktisch auf seinem
       * Rastpunkt steht. Eine laufende Bewegung lässt
       * sich weiterhin ohne Sprung direkt einfangen.
       */
      s_scroll_breakaway_anchor_q8 =
          snap_anchor_for_index(
            clamp_snap_index(s_snap_index)
          ) *
          SCROLL_PHYSICS_Q8;

      s_scroll_breakaway_locked =
          abs_int32(
            s_scroll_position_q8 -
            s_scroll_breakaway_anchor_q8
          ) <= SCROLL_PHYSICS_Q8;

      /*
       * Alte Animationsgeschwindigkeit nicht unter
       * dem Finger weiterlaufen lassen.
       */
      s_scroll_velocity_q8 = 0;

      schedule_scroll_physics();
      break;

    case TouchEvent_PositionUpdate:
      if (s_dragging) {
        const int16_t delta_y =
            event->y -
            s_touch_last_y;

        if (s_touch_gesture_consumed) {
          break;
        }

        s_touch_last_y = event->y;
        s_touch_total_delta_y += delta_y;

        if (delta_y == 0) {
          break;
        }

        /*
         * Erst wenn das Losbrechmoment erreicht ist,
         * wird diese Geste fest einem direkten Nachbarn
         * zugeordnet. Kleine Richtungsänderungen davor
         * wählen dadurch nicht versehentlich die falsche
         * Seite.
         */
        if (
          !s_touch_pair_selected &&
          abs_int32(
            s_touch_total_delta_y
          ) >= SCROLL_BREAKAWAY_DISTANCE_PX
        ) {
          s_touch_pair_direction =
              s_touch_total_delta_y < 0
                  ? 1
                  : -1;

          s_touch_neighbor_snap_index =
              clamp_snap_index(
                s_touch_start_snap_index +
                s_touch_pair_direction
              );

          s_touch_pair_selected = true;
        }

        /*
         * Der Finger selbst bewegt sein Ziel exakt 1:1.
         * Die sichtbare Position wird anschließend von
         * Fingerfeder und Magneten gemeinsam bestimmt.
         */
        s_scroll_finger_target_q8 +=
            (int32_t)delta_y *
            SCROLL_PHYSICS_Q8;

        if (s_touch_pair_selected) {
          const int32_t start_anchor_q8 =
              snap_anchor_for_index(
                s_touch_start_snap_index
              ) *
              SCROLL_PHYSICS_Q8;

          if (
            s_touch_neighbor_snap_index !=
            s_touch_start_snap_index
          ) {
            const int32_t neighbor_anchor_q8 =
                snap_anchor_for_index(
                  s_touch_neighbor_snap_index
                ) *
                SCROLL_PHYSICS_Q8;

            const int32_t upper_anchor_q8 =
                start_anchor_q8 >
                    neighbor_anchor_q8
                    ? start_anchor_q8
                    : neighbor_anchor_q8;

            const int32_t lower_anchor_q8 =
                start_anchor_q8 <
                    neighbor_anchor_q8
                    ? start_anchor_q8
                    : neighbor_anchor_q8;

            /*
             * Ein Touch bleibt auf genau dieses Paar
             * begrenzt, darf darin aber beliebig oft
             * vor und zurück fahren.
             */
            if (
              s_scroll_finger_target_q8 >
              upper_anchor_q8
            ) {
              s_scroll_finger_target_q8 =
                  upper_anchor_q8;
            }

            if (
              s_scroll_finger_target_q8 <
              lower_anchor_q8
            ) {
              s_scroll_finger_target_q8 =
                  lower_anchor_q8;
            }
          } else {
            /*
             * Am virtuellen oberen oder unteren Rand
             * gibt es keinen zweiten echten Rastpunkt.
             * Dort bleibt das bisherige Randverhalten
             * unverändert.
             */
            if (
              s_touch_pair_direction > 0 &&
              s_scroll_finger_target_q8 >
                  start_anchor_q8
            ) {
              s_scroll_finger_target_q8 =
                  start_anchor_q8;
            }

            if (
              s_touch_pair_direction < 0 &&
              s_scroll_finger_target_q8 <
                  start_anchor_q8
            ) {
              s_scroll_finger_target_q8 =
                  start_anchor_q8;
            }
          }
        }

        /*
         * An den virtuellen Rändern bleibt das Fingerziel
         * frei genug, um die vollen sichtbaren 29 Pixel
         * gegen Feder und Magnet zu erreichen.
         */
        schedule_scroll_physics();
      }
      break;

    case TouchEvent_Liftoff:
      if (s_dragging) {
        s_dragging = false;
        s_scroll_touching = false;
        s_scroll_breakaway_locked = false;

        /*
         * Wurde am virtuellen Tabellenrand bereits die
         * maximale halbe Raststrecke erreicht, darf die
         * Geste nur noch zurück zum echten Randpunkt
         * federn. Ein neues Weiterbewegen verlangt ein
         * erneutes Aufsetzen.
         */
        if (
          s_touch_gesture_consumed &&
          s_touch_neighbor_snap_index ==
              s_touch_start_snap_index
        ) {
          start_scroll_settle_to_index(
            s_touch_start_snap_index,
            false
          );
          break;
        }

        const uint32_t touch_duration_ms =
            current_time_ms() -
            s_touch_start_time_ms;

        const bool quick_swipe =
            touch_duration_ms <=
                SCROLL_QUICK_SWIPE_MAX_DURATION_MS &&
            abs_int32(
              s_touch_total_delta_y
            ) >=
                SCROLL_QUICK_SWIPE_MIN_DISTANCE_PX;

        if (quick_swipe) {
          /*
           * Kurze Wischer werden als eigenes Ereignis
           * ausgewertet. Dadurch muss der Finger nicht
           * erst die normale magnetische Umschaltgrenze
           * erreichen.
           *
           * Finger nach oben  -> nächster Rastpunkt.
           * Finger nach unten -> vorheriger Rastpunkt.
           */
          const int direction =
              s_touch_total_delta_y < 0
                  ? 1
                  : -1;

          const int target_index =
              clamp_snap_index(
                s_snap_index +
                direction
              );

          /*
           * Der Quick-Swipe verwendet wieder die
           * vollständige Federfahrt vom aktuellen
           * sichtbaren Ort bis zum nächsten Rastpunkt.
           * So bleibt der Übergang so angenehm wie vor
           * der Bounce-Vereinheitlichung.
           */
          start_scroll_settle_to_index(
            target_index,
            true
          );
          break;
        }

        /*
         * Bei langsamem Ziehen bleibt die bestehende
         * Logik unverändert: Der nächstgelegene Magnet
         * der sichtbaren Position gewinnt.
         */
        s_scroll_velocity_q8 = 0;

        const int nearest_index =
            nearest_snap_index_for_position_q8(
              s_scroll_position_q8
            );

        start_scroll_settle_to_index(
          nearest_index,
          false
        );
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
  s_scroll_position_q8 =
      CANVAS_START_OFFSET_Y *
      SCROLL_PHYSICS_Q8;

  s_scroll_velocity_q8 = 0;

  s_scroll_finger_target_q8 =
      s_scroll_position_q8;

  s_scroll_settle_target_q8 =
      s_scroll_position_q8;

  s_scroll_touching = false;
  s_scroll_settling = false;

  s_button_edge_bounce_outward = false;
  s_button_edge_bounce_snap_index = 0;

  s_scroll_breakaway_locked = false;
  s_scroll_breakaway_anchor_q8 =
      s_scroll_position_q8;

#if defined(PBL_TOUCH)
  s_dragging = false;
  s_touch_last_y = 0;
  s_touch_total_delta_y = 0;
  s_touch_start_time_ms = 0;

  s_touch_start_snap_index = 0;
  s_touch_neighbor_snap_index = 0;
  s_touch_pair_direction = 0;
  s_touch_pair_selected = false;
  s_touch_gesture_consumed = false;
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
  cancel_scroll_physics();

#if defined(PBL_TOUCH)
  s_dragging = false;
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_scroll_physics();
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
