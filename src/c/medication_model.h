#pragma once

#include "app_types.h"

/* Medication grouping, dayparts and visible rows. */
MedicationTime current_medication_time(void);
void mark_medication_group_confirmed(
    MedicationSymbol symbol
);
bool medication_group_is_due(
    MedicationSymbol symbol
);
void rebuild_medication_rows(void);
void rebuild_all_medication_rows(void);
void refresh_medication_rows_for_time(void);
void reset_medication_confirmations(void);
bool unconfirmed_medication_group_is_due(void);
void daypart_tick_handler(
    struct tm *tick_time,
    TimeUnits units_changed
);
