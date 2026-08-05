#pragma once

#include <pebble.h>

#define MEDICATION_MAX_PLANS 8
#define MEDICATION_NAME_LENGTH 48
#define MEDICATION_LABEL_LENGTH 56

typedef enum {
  MEDICATION_TIME_MORNING,
  MEDICATION_TIME_NOON,
  MEDICATION_TIME_EVENING,
  MEDICATION_TIME_NIGHT
} MedicationTime;

typedef enum {
  MEDICATION_SCHEDULE_DAILY,
  MEDICATION_SCHEDULE_WEEKLY,
  MEDICATION_SCHEDULE_MONTHLY
} MedicationSchedule;

typedef enum {
  MEDICATION_SYMBOL_PILL,
  MEDICATION_SYMBOL_PEN,
  MEDICATION_SYMBOL_TUBE
} MedicationSymbol;

typedef struct {
  char name[MEDICATION_NAME_LENGTH];
  uint8_t quantity;
  MedicationTime time;
  MedicationSchedule schedule;
  uint8_t schedule_day;
  MedicationSymbol symbol;
  bool enabled;
} MedicationPlan;

void medication_config_init(void);
void medication_config_deinit(void);

uint8_t medication_config_count(void);
const MedicationPlan *medication_config_plan(uint8_t index);
const char *medication_config_label(uint8_t index);
