#include <pebble.h>

#include "watch_settings.h"
#include "medication_alarm.h"
#include "pill_physics.h"
#include "medication_ui.h"

int main(void) {
  watch_settings_init();
  medication_alarm_init();
  pill_physics_init();
  medication_ui_init();

  app_event_loop();

  pill_physics_deinit();
  medication_alarm_deinit();
  medication_ui_deinit();
  watch_settings_deinit();
}
