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

static GColor theme_foreground_color(void);
static GColor theme_hint_color(void);
static void destroy_pill_bitmaps(void);
static void draw_scroll_hint(
    GContext *ctx,
    GRect bounds,
    int32_t pill_y
);
static void canvas_update_proc(
    Layer *layer,
    GContext *ctx
);
static void band_arrow_update_proc(
    Layer *layer,
    GContext *ctx
);
static void band_update_proc(
    Layer *layer,
    GContext *ctx
);
static void ui_timer_callback(void *context);
static void start_ui_timer(void);
static void scroll_up_handler(
    ClickRecognizerRef recognizer,
    void *context
);
static void scroll_down_handler(
    ClickRecognizerRef recognizer,
    void *context
);
static void click_config_provider(void *context);
static bool load_pill_sheet(void);
static void reset_ui_state(GRect bounds);
static void window_load(Window *window);
static void window_appear(Window *window);
static void window_disappear(Window *window);
static void window_unload(Window *window);

GColor theme_background_color(void) {
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

void mark_scene_dirty(void) {
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

void apply_theme(
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

int32_t current_pill_y(void) {
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

void refresh_app_screen_state(void) {
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


void medication_ui_init(void) {
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

void medication_ui_deinit(void) {
  cancel_timer(&s_transfer_close_timer);
  cancel_timer(&s_transfer_animation_timer);
  cancel_timer(&s_ui_timer);
  cancel_timer(&s_confirmation_timer);
  cancel_timer(&s_band_animation_timer);
  tick_timer_service_unsubscribe();

  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}
