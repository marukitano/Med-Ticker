#include <pebble.h>

#define FRAME_COUNT 8
#define FRAME_DURATION_MS 220

static Window *s_window;
static Layer *s_layer;
static GBitmap *s_sheet;
static GBitmap *s_frame;
static AppTimer *s_timer;

static int s_index;
static int s_frame_width;
static int s_frame_height;

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

static void layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (!s_frame) {
    return;
  }

  GRect destination = GRect(
    (bounds.size.w - s_frame_width) / 2,
    (bounds.size.h - s_frame_height) / 2,
    s_frame_width,
    s_frame_height
  );

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_frame, destination);
}

static void timer_callback(void *context) {
  s_index = (s_index + 1) % FRAME_COUNT;
  set_frame(s_index);
  layer_mark_dirty(s_layer);

  s_timer = app_timer_register(
    FRAME_DURATION_MS,
    timer_callback,
    NULL
  );
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_sheet = gbitmap_create_with_resource(
    RESOURCE_ID_IMAGE_PILL_SHEET
  );

  if (!s_sheet) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Spritesheet konnte nicht geladen werden");
    return;
  }

  GRect sheet_bounds = gbitmap_get_bounds(s_sheet);

  if (sheet_bounds.size.w % FRAME_COUNT != 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Spritesheet hat falsche Breite");
    return;
  }

  s_frame_width = sheet_bounds.size.w / FRAME_COUNT;
  s_frame_height = sheet_bounds.size.h;

  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, layer_update_proc);
  layer_add_child(root, s_layer);

  s_index = 0;
  set_frame(s_index);

  s_timer = app_timer_register(
    FRAME_DURATION_MS,
    timer_callback,
    NULL
  );
}

static void window_unload(Window *window) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }

  destroy_frame();

  if (s_sheet) {
    gbitmap_destroy(s_sheet);
    s_sheet = NULL;
  }

  if (s_layer) {
    layer_destroy(s_layer);
    s_layer = NULL;
  }
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);

  window_set_window_handlers(
    s_window,
    (WindowHandlers) {
      .load = window_load,
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
