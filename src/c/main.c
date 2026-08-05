#include <pebble.h>

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

#define HINT_SPACING 14
#define HINT_HALF_WIDTH 9
#define HINT_HEIGHT 10
#define HINT_POSITION_ADJUST_Y -18

#define MEDICATION_GAP 54
#define MEDICATION_HEADER_HEIGHT 28
#define MEDICATION_ROW_HEIGHT 50
#define MEDICATION_ROW_GAP 8
#define MEDICATION_COUNT 3
#define BAND_OVERSHOOT_COVER_PX 32

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
static Layer *s_band_layer;
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
static GBitmap *s_frame;

static AppTimer *s_ui_timer;
static AppTimer *s_confirmation_timer;
static AppTimer *s_scroll_physics_timer;
static AppTimer *s_band_animation_timer;

static GFont s_medication_font;

static int16_t s_frame_index;
static int16_t s_frame_width;
static int16_t s_frame_height;
static uint8_t s_ui_tick;
static uint8_t s_hint_phase;

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
  int8_t edge_return_index;

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

static int16_t s_check_size;
static CheckState s_check_state;

static void update_band_animation_target(void);

static void mark_canvas_dirty(void) {
  update_band_animation_target();

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

  if (s_band_layer) {
    layer_mark_dirty(s_band_layer);
  }

  if (s_confirmation_layer) {
    layer_mark_dirty(s_confirmation_layer);
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

static void set_canvas_to_snap_index(int index) {
  s_scroll.snap_index =
      clamp_snap_index(index);

  mark_canvas_dirty();
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

static bool medication_band_can_enter(void) {
  /*
   * Das Band startet nicht während der Bewegung.
   * Erst wenn mindestens der erste Medikamentenpunkt
   * eingerastet ist und der gesamte Bounce beendet
   * wurde, darf es von rechts hereinfahren.
   */
  return
      s_scroll.snap_index >= 1 &&
      s_scroll.mode == SCROLL_IDLE;
}

static bool scrolling_back_to_pill(void) {
#if defined(PBL_TOUCH)
  /*
   * Das Paar 1 <-> 0 bleibt nach dem Snap absichtlich
   * aktiv, damit derselbe Finger zurückwischen kann.
   * Das Paar allein bedeutet aber noch nicht, dass
   * tatsächlich zur Pille gescrollt wird.
   *
   * Erst eine reale Fingerbewegung nach unten startet
   * die Ausfahrt des weißen Bandes.
   */
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

static bool pill_is_visible(void) {
  if (
    !s_canvas_layer ||
    s_frame_height <= 0
  ) {
    return false;
  }

  const GRect bounds =
      layer_get_bounds(
        s_canvas_layer
      );

  const int32_t pill_y =
      current_pill_y();

  return
      pill_y < bounds.size.h &&
      pill_y + s_frame_height > 0;
}

static int32_t clamp_band_force_q8(
    int32_t force_q8
) {
  if (
    force_q8 >
    SCROLL_SNAP_MAX_FORCE_Q8
  ) {
    return SCROLL_SNAP_MAX_FORCE_Q8;
  }

  if (
    force_q8 <
    -SCROLL_SNAP_MAX_FORCE_Q8
  ) {
    return -SCROLL_SNAP_MAX_FORCE_Q8;
  }

  return force_q8;
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

  layer_set_frame(
    s_band_layer,
    frame
  );
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

  force_q8 =
      clamp_band_force_q8(
        force_q8
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
      layer_set_hidden(
        s_band_layer,
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

static void finish_band_exit(void) {
  if (
    !s_band_layer ||
    !s_canvas_layer
  ) {
    return;
  }

  cancel_timer(
    &s_band_animation_timer
  );

  const GRect bounds =
      layer_get_bounds(
        s_canvas_layer
      );

  s_band.target_visible = false;
  s_band.animating = false;
  s_band.velocity_q8 = 0;

  s_band.x_q8 =
      bounds.size.w *
      SCROLL_Q8;

  s_band.target_x_q8 =
      s_band.x_q8;

  set_band_layer_x_q8(
    s_band.x_q8
  );

  layer_set_hidden(
    s_band_layer,
    true
  );
}

static void update_band_animation_target(void) {
  if (
    !s_band_layer ||
    !s_canvas_layer
  ) {
    return;
  }

  const GRect canvas_bounds =
      layer_get_bounds(
        s_canvas_layer
      );

  /*
   * Die Sichtbarkeitsgrenze gilt nur während der
   * Ausfahrt zur Pille. Beim Einrasten des ersten
   * Medikamentes darf sie den Start des Bandes nicht
   * mehr blockieren.
   */
  if (
    pill_is_visible() &&
    !s_band.target_visible &&
    (
      s_band.animating ||
      !layer_get_hidden(
        s_band_layer
      )
    )
  ) {
    finish_band_exit();
    return;
  }

  const bool should_exit =
      scrolling_back_to_pill();

  const bool should_enter =
      !should_exit &&
      medication_band_can_enter();

  if (should_exit) {
    if (
      !s_band.target_visible &&
      s_band.target_x_q8 ==
          canvas_bounds.size.w *
          SCROLL_Q8
    ) {
      return;
    }

    s_band.target_visible = false;
    s_band.target_x_q8 =
        canvas_bounds.size.w *
        SCROLL_Q8;

    s_band.animating = true;
    schedule_band_animation();
    return;
  }

  if (!should_enter) {
    return;
  }

  if (
    s_band.target_visible &&
    s_band.target_x_q8 == 0
  ) {
    return;
  }

  s_band.target_visible = true;
  s_band.target_x_q8 = 0;
  s_band.animating = true;

  layer_set_hidden(
    s_band_layer,
    false
  );

  schedule_band_animation();
}

static void draw_medication_text(
    GContext *ctx,
    GRect row,
    const char *text,
    GColor text_color
) {
  graphics_context_set_text_color(
    ctx,
    text_color
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

  const int32_t rows_y =
      label_y +
      MEDICATION_HEADER_HEIGHT;

  const int32_t list_bottom =
      rows_y +
      MEDICATION_COUNT *
          (
            MEDICATION_ROW_HEIGHT +
            MEDICATION_ROW_GAP
          );

  if (
    list_bottom < 0 ||
    label_y > bounds.size.h
  ) {
    return;
  }

  graphics_context_set_text_color(
    ctx,
    text_color
  );

  graphics_draw_text(
    ctx,
    "MEDICATIONS",
    fonts_get_system_font(
      FONT_KEY_GOTHIC_18_BOLD
    ),
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

  for (
    int index = 0;
    index < MEDICATION_COUNT;
    index++
  ) {
    const int32_t row_y =
        rows_y +
        index *
            (
              MEDICATION_ROW_HEIGHT +
              MEDICATION_ROW_GAP
            );

    if (
      row_y + MEDICATION_ROW_HEIGHT < 0 ||
      row_y > bounds.size.h
    ) {
      continue;
    }

    draw_medication_text(
      ctx,
      GRect(
        0,
        (int16_t)row_y,
        bounds.size.w,
        MEDICATION_ROW_HEIGHT
      ),
      s_medications[index],
      text_color
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

static int32_t pill_y_for_bounds(
    GRect bounds
) {
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

static void draw_pill_if_visible(
    GContext *ctx,
    GRect bounds,
    int32_t pill_y
) {
  if (
    !s_frame ||
    pill_y <= -s_frame_height ||
    pill_y >= bounds.size.h
  ) {
    return;
  }

  graphics_context_set_compositing_mode(
    ctx,
    GCompOpSet
  );

  graphics_draw_bitmap_in_rect(
    ctx,
    s_frame,
    GRect(
      (
        bounds.size.w -
        s_frame_width
      ) /
      2,
      (int16_t)pill_y,
      s_frame_width,
      s_frame_height
    )
  );
}

static void canvas_update_proc(
    Layer *layer,
    GContext *ctx
) {
  const GRect bounds =
      layer_get_bounds(layer);

  graphics_context_set_fill_color(
    ctx,
    GColorBlack
  );

  graphics_fill_rect(
    ctx,
    bounds,
    0,
    GCornerNone
  );

  if (!s_frame) {
    return;
  }

  const int32_t pill_y =
      pill_y_for_bounds(bounds);

  draw_pill_if_visible(
    ctx,
    bounds,
    pill_y
  );

  draw_scroll_hint(
    ctx,
    bounds,
    pill_y
  );

  draw_medications(
    ctx,
    bounds,
    pill_y,
    GColorWhite
  );
}

static void band_update_proc(
    Layer *layer,
    GContext *ctx
) {
  if (
    !s_frame ||
    !s_canvas_layer
  ) {
    return;
  }

  const GRect layer_bounds =
      layer_get_bounds(layer);

  const GRect frame =
      layer_get_frame(layer);

  const GRect canvas_bounds =
      layer_get_bounds(
        s_canvas_layer
      );

  const GRect content_bounds =
      GRect(
        0,
        0,
        canvas_bounds.size.w,
        layer_bounds.size.h
      );

  const int32_t screen_pill_y =
      pill_y_for_bounds(
        canvas_bounds
      );

  const int32_t local_pill_y =
      screen_pill_y -
      frame.origin.y;

  graphics_context_set_fill_color(
    ctx,
    GColorWhite
  );

  graphics_fill_rect(
    ctx,
    layer_bounds,
    0,
    GCornerNone
  );

  draw_medications(
    ctx,
    content_bounds,
    local_pill_y,
    GColorBlack
  );

  draw_pill_if_visible(
    ctx,
    content_bounds,
    local_pill_y
  );

  draw_scroll_hint(
    ctx,
    content_bounds,
    local_pill_y
  );
}

static void confirmation_update_proc(
    Layer *layer,
    GContext *ctx
) {
  const GRect bounds =
      layer_get_bounds(layer);

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
      scroll_anchor_q8(MEDICATION_COUNT) -
      SCROLL_EDGE_HALF_INTERVAL_PX *
      SCROLL_Q8;
}

static int32_t clamp_symmetric(
    int32_t value,
    int32_t limit
) {
  if (value > limit) {
    return limit;
  }

  if (value < -limit) {
    return -limit;
  }

  return value;
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
    current < MEDICATION_COUNT
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
        MEDICATION_COUNT
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
    index < MEDICATION_COUNT;
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

static bool scroll_is_active(void) {
  return s_scroll.mode != SCROLL_IDLE;
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

  if (s_scroll.edge_return_index == 0) {
    return
        s_scroll.position_q8 >=
        scroll_top_limit_q8();
  }

  return
      s_scroll.position_q8 <=
      scroll_bottom_limit_q8();
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
        MEDICATION_COUNT &&
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

  if (!scroll_is_active()) {
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

    /*
     * Exakt derselbe Schritt wie bei der entsprechenden
     * Hoch-/Runtertaste. Keine eigene Touch-Animation.
     */
    step_snap_index(
      direction
    );

    keep_touch_active_after_step();
    mark_canvas_dirty();
    return;
  }
#endif

  apply_scroll_edge_limits();

  if (edge_bounce_reached_limit()) {
    const int return_index =
        s_scroll.edge_return_index;

    start_scroll_snap(
      return_index,
      false
    );

    mark_canvas_dirty();
    return;
  }

#if defined(PBL_TOUCH)
  if (touch_reached_virtual_edge()) {
    s_touch.edge_consumed = true;

    start_scroll_snap(
      s_touch.start_index,
      true
    );

    mark_canvas_dirty();
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

  mark_canvas_dirty();

  if (scroll_is_active()) {
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
    mark_canvas_dirty();
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

  set_canvas_to_snap_index(target_index);

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
  s_scroll.edge_return_index =
      s_scroll.snap_index;

  s_scroll.target_q8 =
      scroll_anchor_q8(
        s_scroll.edge_return_index
      ) +
      (
        direction < 0
            ? 2
            : -2
      ) *
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
              MEDICATION_COUNT &&
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
  if (s_confirmation_state == CONFIRM_IDLE) {
    step_snap_index(-1);
  }
}

static void scroll_down_handler(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirmation_state == CONFIRM_IDLE) {
    step_snap_index(1);
  }
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
  if (s_confirmation_state != CONFIRM_IDLE) {
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
  /*
   * Das beim Aufsetzen gewählte Rastpunktpaar bleibt
   * während der gesamten Geste erhalten.
   *
   * Nach einem Schritt werden nur Start und Nachbar
   * vertauscht. So kann derselbe Finger exakt zum
   * vorherigen Punkt zurück, aber niemals zu einem
   * dritten Rastpunkt weiterlaufen.
   */
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
    /*
     * Während der vorhandene Tasten-Snap läuft, wird
     * die Fingerposition nur nachgeführt. Die Bewegung
     * darf den laufenden Bounce nicht beeinflussen.
     */
    if (s_scroll.mode != SCROLL_IDLE &&
        s_scroll.mode != SCROLL_TOUCH) {
      return;
    }

    /*
     * Nach beendetem Snap übernimmt derselbe Finger
     * wieder die normale Touch-Feder. Das festgelegte
     * Paar wurde in keep_touch_active_after_step()
     * lediglich umgedreht und bleibt damit gesperrt.
     */
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

    /*
     * Diese bestehende Begrenzung hält das Ziel exakt
     * zwischen den beiden beim Aufsetzen gewählten
     * Rastpunkten. Hinter dem Rand sammelt sich kein
     * unsichtbarer Fingerweg an.
     */
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

  /*
   * Ein bereits durch 29 px ausgelöster normaler
   * Tastenschritt läuft beim Abheben unverändert weiter.
   */
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
  const int32_t initial_position_q8 =
      CANVAS_START_OFFSET_Y *
      SCROLL_Q8;

  s_scroll = (ScrollState) {
    .position_q8 = initial_position_q8,
    .target_q8 = initial_position_q8,
    .breakaway_anchor_q8 =
        initial_position_q8,
    .snap_index = 0,
    .edge_return_index = 0,
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

    layer_set_hidden(
      s_band_layer,
      true
    );
  }

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

  layer_set_hidden(
    s_band_layer,
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
  set_frame(s_frame_index);
}

static void window_appear(Window *window) {
  if (s_canvas_layer) {
    start_ui_timer();
  }

  if (s_band.animating) {
    schedule_band_animation();
  }

#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
}

static void window_disappear(Window *window) {
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
  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_timer(&s_band_animation_timer);
  cancel_scroll_physics();
  destroy_frame();

  if (s_sheet) {
    gbitmap_destroy(s_sheet);
    s_sheet = NULL;
  }

  if (s_confirmation_layer) {
    layer_destroy(
      s_confirmation_layer
    );

    s_confirmation_layer = NULL;
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
