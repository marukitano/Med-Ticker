#include "medication_config.h"

#include "message_keys.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PERSIST_KEY 100
#define CONFIG_PAYLOAD_MAX 768
#define RECORD_SEPARATOR '\x1e'
#define FIELD_SEPARATOR '\x1f'

static MedicationPlan s_plans[MEDICATION_MAX_PLANS];
static char s_labels[MEDICATION_MAX_PLANS][MEDICATION_LABEL_LENGTH];
static uint8_t s_plan_count;

static const MedicationPlan s_defaults[] = {
  {
    .name = "Xarelto 20 mg",
    .quantity = 1,
    .time = MEDICATION_TIME_MORNING,
    .schedule = MEDICATION_SCHEDULE_DAILY,
    .schedule_day = 0,
    .symbol = MEDICATION_SYMBOL_PILL,
    .enabled = true
  },
  {
    .name = "Metformin 1000 mg",
    .quantity = 1,
    .time = MEDICATION_TIME_MORNING,
    .schedule = MEDICATION_SCHEDULE_DAILY,
    .schedule_day = 0,
    .symbol = MEDICATION_SYMBOL_PILL,
    .enabled = true
  },
  {
    .name = "Pantoprazol 40 mg",
    .quantity = 1,
    .time = MEDICATION_TIME_MORNING,
    .schedule = MEDICATION_SCHEDULE_DAILY,
    .schedule_day = 0,
    .symbol = MEDICATION_SYMBOL_PILL,
    .enabled = true
  }
};

static void rebuild_labels(void) {
  for (uint8_t index = 0; index < s_plan_count; index++) {
    const MedicationPlan *plan = &s_plans[index];

    if (plan->quantity > 1) {
      snprintf(
        s_labels[index],
        sizeof(s_labels[index]),
        "%s  x%u",
        plan->name,
        (unsigned int)plan->quantity
      );
    } else {
      snprintf(
        s_labels[index],
        sizeof(s_labels[index]),
        "%s",
        plan->name
      );
    }
  }
}

static void load_defaults(void) {
  s_plan_count = ARRAY_LENGTH(s_defaults);
  memcpy(s_plans, s_defaults, sizeof(s_defaults));
  rebuild_labels();
}

static bool parse_uint8(
    const char *text,
    uint8_t minimum,
    uint8_t maximum,
    uint8_t *value
) {
  if (!text || !*text) {
    return false;
  }

  char *end;
  const long parsed = strtol(text, &end, 10);

  if (
    *end != '\0' ||
    parsed < minimum ||
    parsed > maximum
  ) {
    return false;
  }

  *value = (uint8_t)parsed;
  return true;
}

static int8_t hex_value(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }

  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }

  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }

  return -1;
}

static bool decode_name(
    const char *encoded,
    char destination[MEDICATION_NAME_LENGTH]
) {
  size_t output = 0;

  while (*encoded) {
    char character = *encoded++;

    if (character == '%') {
      const int8_t high = hex_value(encoded[0]);
      const int8_t low = hex_value(encoded[1]);

      if (high < 0 || low < 0) {
        return false;
      }

      character = (char)((high << 4) | low);
      encoded += 2;
    } else if (character == '+') {
      character = ' ';
    }

    if (
      character == RECORD_SEPARATOR ||
      character == FIELD_SEPARATOR ||
      output + 1 >= MEDICATION_NAME_LENGTH
    ) {
      return false;
    }

    destination[output++] = character;
  }

  destination[output] = '\0';
  return output > 0;
}

static char *next_field(char **cursor) {
  if (!cursor || !*cursor) {
    return NULL;
  }

  char *field = *cursor;
  char *separator = strchr(field, FIELD_SEPARATOR);

  if (separator) {
    *separator = '\0';
    *cursor = separator + 1;
  } else {
    *cursor = NULL;
  }

  return field;
}

static bool parse_plan(
    char *record,
    MedicationPlan *plan
) {
  char *cursor = record;
  char *fields[7];

  for (uint8_t index = 0; index < ARRAY_LENGTH(fields); index++) {
    fields[index] = next_field(&cursor);

    if (!fields[index]) {
      return false;
    }
  }

  if (cursor) {
    return false;
  }

  uint8_t enabled;
  uint8_t quantity;
  uint8_t time;
  uint8_t schedule;
  uint8_t day;
  uint8_t symbol;

  if (
    !parse_uint8(fields[0], 0, 1, &enabled) ||
    !parse_uint8(fields[1], 1, 20, &quantity) ||
    !parse_uint8(fields[2], 0, 3, &time) ||
    !parse_uint8(fields[3], 0, 2, &schedule) ||
    !parse_uint8(fields[5], 0, 2, &symbol)
  ) {
    return false;
  }

  if (schedule == MEDICATION_SCHEDULE_DAILY) {
    if (!parse_uint8(fields[4], 0, 0, &day)) {
      return false;
    }
  } else if (schedule == MEDICATION_SCHEDULE_WEEKLY) {
    if (!parse_uint8(fields[4], 0, 6, &day)) {
      return false;
    }
  } else if (!parse_uint8(fields[4], 1, 31, &day)) {
    return false;
  }

  MedicationPlan parsed = {
    .quantity = quantity,
    .time = (MedicationTime)time,
    .schedule = (MedicationSchedule)schedule,
    .schedule_day = day,
    .symbol = (MedicationSymbol)symbol,
    .enabled = enabled == 1
  };

  if (!decode_name(fields[6], parsed.name)) {
    return false;
  }

  *plan = parsed;
  return true;
}

static bool apply_payload(
    const char *payload,
    bool persist
) {
  if (!payload) {
    return false;
  }

  const size_t length = strlen(payload);

  if (length >= CONFIG_PAYLOAD_MAX) {
    return false;
  }

  char buffer[CONFIG_PAYLOAD_MAX];
  memcpy(buffer, payload, length + 1);

  MedicationPlan parsed[MEDICATION_MAX_PLANS];
  uint8_t count = 0;
  char *cursor = buffer;

  while (*cursor) {
    if (count >= MEDICATION_MAX_PLANS) {
      return false;
    }

    char *record = cursor;
    char *separator = strchr(record, RECORD_SEPARATOR);

    if (separator) {
      *separator = '\0';
      cursor = separator + 1;
    } else {
      cursor += strlen(cursor);
    }

    if (!parse_plan(record, &parsed[count])) {
      return false;
    }

    count++;
  }

  memcpy(s_plans, parsed, count * sizeof(MedicationPlan));
  s_plan_count = count;
  rebuild_labels();

  if (persist) {
    persist_write_string(CONFIG_PERSIST_KEY, payload);
  }

  return true;
}

static void load_persisted_config(void) {
  if (!persist_exists(CONFIG_PERSIST_KEY)) {
    load_defaults();
    return;
  }

  const int size = persist_get_size(CONFIG_PERSIST_KEY);

  if (size <= 0 || size > CONFIG_PAYLOAD_MAX) {
    load_defaults();
    return;
  }

  char payload[CONFIG_PAYLOAD_MAX];

  if (
    persist_read_string(
      CONFIG_PERSIST_KEY,
      payload,
      sizeof(payload)
    ) <= 0 ||
    !apply_payload(payload, false)
  ) {
    load_defaults();
  }
}

static void inbox_received_handler(
    DictionaryIterator *iterator,
    void *context
) {
  (void)context;

  Tuple *tuple = dict_find(
    iterator,
    MESSAGE_KEY_CONFIG_DATA
  );

  if (
    !tuple ||
    tuple->type != TUPLE_CSTRING ||
    !apply_payload(tuple->value->cstring, true)
  ) {
    APP_LOG(
      APP_LOG_LEVEL_WARNING,
      "Invalid medication configuration"
    );
  }
}

void medication_config_init(void) {
  load_persisted_config();

  app_message_register_inbox_received(
    inbox_received_handler
  );

  const AppMessageResult result =
      app_message_open(CONFIG_PAYLOAD_MAX, 64);

  if (result != APP_MSG_OK) {
    APP_LOG(
      APP_LOG_LEVEL_ERROR,
      "AppMessage open failed: %d",
      result
    );
  }
}

void medication_config_deinit(void) {
  app_message_deregister_callbacks();
}

uint8_t medication_config_count(void) {
  return s_plan_count;
}

const MedicationPlan *medication_config_plan(
    uint8_t index
) {
  return index < s_plan_count
      ? &s_plans[index]
      : NULL;
}

const char *medication_config_label(
    uint8_t index
) {
  return index < s_plan_count
      ? s_labels[index]
      : NULL;
}
