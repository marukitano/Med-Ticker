#pragma once

#include "app_types.h"

/* Scroll snapping, touch input and band animation. */
void cancel_scroll_physics(void);
void schedule_band_animation(void);
void set_band_and_arrow_hidden(
    bool hidden
);
void set_band_layer_x_q8(
    int32_t x_q8
);
bool step_snap_index(int direction);
void update_band_animation_target(void);
int32_t visual_canvas_offset_y(void);

#if defined(PBL_TOUCH)
void touch_handler(
    const TouchEvent *event,
    void *context
);
#endif
