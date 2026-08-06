#pragma once

#include "app_types.h"

/* Medication and pill rendering. */
void draw_medications(
    GContext *ctx,
    GRect bounds,
    int32_t pill_y,
    GColor text_color
);
void draw_physics_pills(
    GContext *ctx,
    GRect bounds,
    int32_t arena_y
);
