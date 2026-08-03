#include <pebble.h>

#define FRAME_COUNT 8
#define PILL_FRAME_DURATION_MS 220
#define UI_TICK_MS 110
#define CANVAS_START_OFFSET_Y -18

#define CONFIRM_ANIMATION_INTERVAL_MS 30
#define CONFIRM_GROW_STEP 5
#define CONFIRM_SHRINK_MIN_STEP 4
#define CONFIRM_SHRINK_DIVISOR 7
#define CONFIRM_CENTER_OUTSIDE_X 8

#define CHECK_STROKE_RADIUS 8

#define CHECK_POP_GROW_STEP 140
#define CHECK_POP_SHRINK_STEP 30

#define CHECK_POP_SETTLE_SIZE 80
#define CHECK_POP_OVERSHOOT_SIZE 140

#define HINT_SPACING 14
#define HINT_HALF_WIDTH 9
#define HINT_HEIGHT 10

#define MEDICATION_GAP 54
#define MEDICATION_HEADER_HEIGHT 28
#define MEDICATION_ROW_HEIGHT 50
#define MEDICATION_ROW_GAP 8
#define MEDICATION_COUNT 3


static const int s_hint_offsets[] = {0, 1, 3, 5, 3, 1, 0, 0};

static const char *s_medications[MEDICATION_COUNT] = {
  "Xarelto 20 mg",
  "Metformin 1000 mg",
  "Pantoprazol 40 mg"
};

static Window *s_window;
static Layer *s_canvas_layer;

static GBitmap *s_sheet;
static GBitmap *s_frame;
static AppTimer *s_timer;
static AppTimer *s_confirm_timer;

static GFont s_medication_font;

static int s_frame_index;
static int s_frame_width;
static int s_frame_height;
static int s_tick_count;
static int s_hint_phase;


static int32_t s_canvas_offset_y;
static int32_t s_drag_start_offset_y;
static int16_t s_touch_start_y;
static bool s_dragging;
static bool s_window_visible;

static int16_t s_confirm_radius;
static int16_t s_confirm_max_radius;
static bool s_confirm_holding;
static bool s_confirm_shrinking;
static bool s_confirm_complete;

static int16_t s_check_size;
static uint8_t s_check_pop_phase;

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
  if (s_frame) {
    gbitmap_destroy(s_frame);
    s_frame = NULL;
  }
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
      HINT_SPACING;

  const int32_t rows_y =
      pill_y +
      s_frame_height +
      MEDICATION_GAP +
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

  for (int i = 0; i < MEDICATION_COUNT; i++) {
    const int32_t row_y =
        rows_y +
        i * (MEDICATION_ROW_HEIGHT + MEDICATION_ROW_GAP);

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
      medication_background(i)
    );

    graphics_fill_rect(
      ctx,
      row,
      0,
      GCornerNone
    );

    draw_medication_text(
      ctx,
      row,
      s_medications[i],
      i
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

  const GPoint center = GPoint(
    bounds.size.w + CONFIRM_CENTER_OUTSIDE_X,
    bounds.size.h / 2
  );

  graphics_context_set_fill_color(ctx, GColorGreen);
  graphics_fill_circle(
    ctx,
    center,
    (uint16_t)s_confirm_radius
  );
}

static void draw_thick_line(
    GContext *ctx,
    GPoint start,
    GPoint end
) {
  const int16_t dx = end.x - start.x;
  const int16_t dy = end.y - start.y;

  const int16_t abs_dx = dx < 0 ? -dx : dx;
  const int16_t abs_dy = dy < 0 ? -dy : dy;
  const int16_t steps = abs_dx > abs_dy ? abs_dx : abs_dy;

  int16_t stroke_radius =
      ((int32_t)CHECK_STROKE_RADIUS * s_check_size +
       CHECK_POP_SETTLE_SIZE / 2) /
      CHECK_POP_SETTLE_SIZE;

  if (stroke_radius < 1) {
    stroke_radius = 1;
  }

  if (steps <= 0) {
    graphics_fill_circle(
      ctx,
      start,
      stroke_radius
    );
    return;
  }

  for (int16_t step = 0; step <= steps; step++) {
    const GPoint point = GPoint(
      start.x + ((int32_t)dx * step) / steps,
      start.y + ((int32_t)dy * step) / steps
    );

    graphics_fill_circle(
      ctx,
      point,
      stroke_radius
    );
  }
}

static void draw_confirmation_checkmark(
    GContext *ctx,
    GRect bounds
) {
  if (!s_confirm_complete || s_check_size <= 0) {
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
  draw_thick_line(ctx, start, middle);
  draw_thick_line(ctx, middle, end);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (!s_frame) {
    return;
  }

  const int32_t pill_y =
      ((int32_t)bounds.size.h - s_frame_height) / 2 +
      s_canvas_offset_y;

  if (pill_y > -s_frame_height &&
      pill_y < bounds.size.h) {
    const GRect destination = GRect(
      (bounds.size.w - s_frame_width) / 2,
      (int16_t)pill_y,
      s_frame_width,
      s_frame_height
    );

    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_frame, destination);
  }

  draw_scroll_hint(ctx, bounds, pill_y);
  draw_medications(ctx, bounds, pill_y);
  draw_confirmation_circle(ctx, bounds);
  draw_confirmation_checkmark(ctx, bounds);
}

static void stop_timer(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

static void timer_callback(void *context) {
  s_timer = NULL;

  if (!s_window_visible) {
    return;
  }

  s_tick_count++;
  s_hint_phase =
      (s_hint_phase + 1) %
      ((int)(sizeof(s_hint_offsets) / sizeof(s_hint_offsets[0])));

  if ((s_tick_count * UI_TICK_MS) %
      PILL_FRAME_DURATION_MS == 0) {
    s_frame_index = (s_frame_index + 1) % FRAME_COUNT;
    set_frame(s_frame_index);
  }

  layer_mark_dirty(s_canvas_layer);

  s_timer = app_timer_register(
    UI_TICK_MS,
    timer_callback,
    NULL
  );
}

static void start_timer(void) {
  stop_timer();

  s_timer = app_timer_register(
    UI_TICK_MS,
    timer_callback,
    NULL
  );
}

static void stop_confirmation_timers(void) {
  if (s_confirm_timer) {
    app_timer_cancel(s_confirm_timer);
    s_confirm_timer = NULL;
  }
}

/*
 * Dies ist der eigentliche Bestätigungsmoment.
 *
 * Sobald die Wakeup-Erinnerung eingebaut ist, wird hier der bereits
 * vorgemerkte 15-Minuten-Wakeup gelöscht. Das Schließen der App ist
 * davon bewusst getrennt.
 */
static void confirm_medication_taken(void) {
  /* Anschlussstelle für wakeup_cancel(...) folgt später. */
}

static void confirmation_timer_callback(void *context) {
  s_confirm_timer = NULL;

  if (s_confirm_holding && !s_confirm_complete) {
    s_confirm_radius += CONFIRM_GROW_STEP;

    if (s_confirm_radius >= s_confirm_max_radius) {
      s_confirm_radius = s_confirm_max_radius;
      s_confirm_holding = false;
      s_confirm_shrinking = false;
      s_confirm_complete = true;
      s_check_size = 8;
      s_check_pop_phase = 1;

      confirm_medication_taken();
      vibes_short_pulse();
    }
  } else if (s_confirm_shrinking) {
    int16_t shrink_step =
        s_confirm_radius / CONFIRM_SHRINK_DIVISOR;

    if (shrink_step < CONFIRM_SHRINK_MIN_STEP) {
      shrink_step = CONFIRM_SHRINK_MIN_STEP;
    }

    s_confirm_radius -= shrink_step;

    if (s_confirm_radius <= 0) {
      s_confirm_radius = 0;
      s_confirm_shrinking = false;
    }
  } else if (s_confirm_complete && s_check_pop_phase == 1) {
    s_check_size += CHECK_POP_GROW_STEP;

    if (s_check_size >= CHECK_POP_OVERSHOOT_SIZE) {
      s_check_size = CHECK_POP_OVERSHOOT_SIZE;
      s_check_pop_phase = 2;
    }
  } else if (s_confirm_complete && s_check_pop_phase == 2) {
    s_check_size -= CHECK_POP_SHRINK_STEP;

    if (s_check_size <= CHECK_POP_SETTLE_SIZE) {
      s_check_size = CHECK_POP_SETTLE_SIZE;
      s_check_pop_phase = 3;
    }
  }

  layer_mark_dirty(s_canvas_layer);

  if ((s_confirm_holding && !s_confirm_complete) ||
      s_confirm_shrinking ||
      s_check_pop_phase == 1 ||
      s_check_pop_phase == 2) {
    s_confirm_timer = app_timer_register(
      CONFIRM_ANIMATION_INTERVAL_MS,
      confirmation_timer_callback,
      NULL
    );
  }
}

static void ensure_confirmation_timer(void) {
  if (!s_confirm_timer) {
    s_confirm_timer = app_timer_register(
      CONFIRM_ANIMATION_INTERVAL_MS,
      confirmation_timer_callback,
      NULL
    );
  }
}

static void select_button_down(
    ClickRecognizerRef recognizer,
    void *context
) {
  if (s_confirm_complete) {
    return;
  }

  s_confirm_holding = true;
  s_confirm_shrinking = false;
  ensure_confirmation_timer();
}

static void select_button_up(
    ClickRecognizerRef recognizer,
    void *context
) {
  s_confirm_holding = false;

  if (s_confirm_complete) {
    window_stack_pop_all(true);
    return;
  }

  if (s_confirm_radius > 0) {
    s_confirm_shrinking = true;
    ensure_confirmation_timer();
  }
}

static void click_config_provider(void *context) {
  window_raw_click_subscribe(
    BUTTON_ID_SELECT,
    select_button_down,
    select_button_up,
    NULL
  );
}

#if defined(PBL_TOUCH)
static void update_drag_position(int16_t current_y) {
  const int32_t delta_y =
      (int32_t)current_y - (int32_t)s_touch_start_y;

  s_canvas_offset_y =
      s_drag_start_offset_y + delta_y;

  if (s_canvas_offset_y > CANVAS_START_OFFSET_Y) {
    s_canvas_offset_y = CANVAS_START_OFFSET_Y;
  }

  layer_mark_dirty(s_canvas_layer);
}

static void touch_handler(
    const TouchEvent *event,
    void *context
) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_dragging = true;
      s_touch_start_y = event->y;
      s_drag_start_offset_y = s_canvas_offset_y;
      break;

    case TouchEvent_PositionUpdate:
      if (s_dragging) {
        update_drag_position(event->y);
      }
      break;

    case TouchEvent_Liftoff:
      if (s_dragging) {
        update_drag_position(event->y);
        s_dragging = false;
      }
      break;
  }
}
#endif

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);

  s_medication_font =
      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  s_sheet = gbitmap_create_with_resource(
    RESOURCE_ID_IMAGE_PILL_SHEET
  );

  if (!s_sheet) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Spritesheet konnte nicht geladen werden"
    );
    return;
  }

  const GRect sheet_bounds = gbitmap_get_bounds(s_sheet);

  if (sheet_bounds.size.w % FRAME_COUNT != 0) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "Spritesheet hat falsche Breite"
    );
    return;
  }

  s_frame_width = sheet_bounds.size.w / FRAME_COUNT;
  s_frame_height = sheet_bounds.size.h;

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(
    s_canvas_layer,
    canvas_update_proc
  );
  layer_add_child(root, s_canvas_layer);

  s_frame_index = 0;
  s_tick_count = 0;
  s_hint_phase = 0;
  s_canvas_offset_y = CANVAS_START_OFFSET_Y;
  s_dragging = false;

  s_confirm_radius = 0;
  s_confirm_max_radius =
      bounds.size.w +
      CONFIRM_CENTER_OUTSIDE_X +
      bounds.size.h / 2;
  s_confirm_holding = false;
  s_confirm_shrinking = false;
  s_confirm_complete = false;

  s_check_size = 0;
  s_check_pop_phase = 0;

  set_frame(s_frame_index);
}

static void window_appear(Window *window) {
  s_window_visible = true;
  start_timer();

#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
}

static void window_disappear(Window *window) {
  s_window_visible = false;
  s_dragging = false;
  stop_timer();
  stop_confirmation_timers();

#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
}

static void window_unload(Window *window) {
  s_window_visible = false;
  s_dragging = false;
  stop_timer();
  stop_confirmation_timers();

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
