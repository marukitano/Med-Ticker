var THEME_STORAGE_KEY = 'pill-reminder-theme';
var MEDICATION_STORAGE_KEY = 'pill-reminder-medication-v1';

var THEME_KEY = 0;
var MED_NAME_KEY = 1;
var MED_QUANTITY_KEY = 2;
var MED_TIME_KEY = 3;
var MED_SCHEDULE_KEY = 4;
var MED_DAY_KEY = 5;
var MED_SYMBOL_KEY = 6;
var MED_ENABLED_KEY = 7;

var DEFAULT_MEDICATION = {
  name: 'Xarelto 20 mg',
  quantity: 1,
  time: 0,
  schedule: 0,
  day: 0,
  symbol: 0,
  enabled: true
};

function currentTheme() {
  return localStorage.getItem(THEME_STORAGE_KEY) || 'dark';
}

function cloneDefaultMedication() {
  return {
    name: DEFAULT_MEDICATION.name,
    quantity: DEFAULT_MEDICATION.quantity,
    time: DEFAULT_MEDICATION.time,
    schedule: DEFAULT_MEDICATION.schedule,
    day: DEFAULT_MEDICATION.day,
    symbol: DEFAULT_MEDICATION.symbol,
    enabled: DEFAULT_MEDICATION.enabled
  };
}

function integerInRange(value, minimum, maximum) {
  return (
    typeof value === 'number' &&
    isFinite(value) &&
    Math.floor(value) === value &&
    value >= minimum &&
    value <= maximum
  );
}

function normalizeMedication(value) {
  if (!value || typeof value !== 'object') {
    return cloneDefaultMedication();
  }

  var name = typeof value.name === 'string'
    ? value.name.trim()
    : '';

  if (!name || name.length > 31) {
    name = DEFAULT_MEDICATION.name;
  }

  var quantity = integerInRange(value.quantity, 1, 20)
    ? value.quantity
    : DEFAULT_MEDICATION.quantity;

  var time = integerInRange(value.time, 0, 3)
    ? value.time
    : DEFAULT_MEDICATION.time;

  var schedule = integerInRange(value.schedule, 0, 2)
    ? value.schedule
    : DEFAULT_MEDICATION.schedule;

  var day = 0;

  if (schedule === 1) {
    day = integerInRange(value.day, 0, 6)
      ? value.day
      : 0;
  } else if (schedule === 2) {
    day = integerInRange(value.day, 1, 31)
      ? value.day
      : 1;
  }

  var symbol = integerInRange(value.symbol, 0, 2)
    ? value.symbol
    : DEFAULT_MEDICATION.symbol;

  return {
    name: name,
    quantity: quantity,
    time: time,
    schedule: schedule,
    day: day,
    symbol: symbol,
    enabled: value.enabled !== false
  };
}

function currentMedication() {
  var stored = localStorage.getItem(
    MEDICATION_STORAGE_KEY
  );

  if (!stored) {
    return cloneDefaultMedication();
  }

  try {
    return normalizeMedication(JSON.parse(stored));
  } catch (error) {
    console.log(
      'Could not read medication: ' +
      error.message
    );
    return cloneDefaultMedication();
  }
}

function sendSettings(theme, medication) {
  var message = {};

  message[THEME_KEY] =
      theme === 'light' ? 1 : 0;
  message[MED_NAME_KEY] =
      medication.name;
  message[MED_QUANTITY_KEY] =
      medication.quantity;
  message[MED_TIME_KEY] =
      medication.time;
  message[MED_SCHEDULE_KEY] =
      medication.schedule;
  message[MED_DAY_KEY] =
      medication.day;
  message[MED_SYMBOL_KEY] =
      medication.symbol;
  message[MED_ENABLED_KEY] =
      medication.enabled ? 1 : 0;

  Pebble.sendAppMessage(
    message,
    function() {
      console.log('Settings sent');
    },
    function(error) {
      console.log(
        'Settings could not be sent: ' +
        JSON.stringify(error)
      );
    }
  );
}

function htmlEscape(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function selected(value, expected) {
  return value === expected ? ' selected' : '';
}

function checked(value) {
  return value ? ' checked' : '';
}

function configurationPage(theme, medication) {
  return [
    '<!doctype html>',
    '<html>',
    '<head>',
    '<meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>Pill Reminder</title>',
    '<style>',
    'body{margin:0;background:#f2f2f2;color:#111;font-family:sans-serif}',
    'main{max-width:480px;margin:auto;padding:22px 16px 36px}',
    'h1{font-size:24px;margin:0 0 20px}',
    'section{background:#fff;border-radius:10px;padding:16px;margin-bottom:14px}',
    'h2{font-size:18px;margin:0 0 14px}',
    'label{display:block;font-size:14px;font-weight:bold;margin-top:14px}',
    'label:first-of-type{margin-top:0}',
    'input[type=text],input[type=number],select{box-sizing:border-box;width:100%;margin-top:7px;padding:11px;font-size:16px}',
    '.check{display:flex;align-items:center;gap:10px}',
    '.check input{width:22px;height:22px}',
    '.hidden{display:none}',
    'button{width:100%;padding:13px;border:0;border-radius:8px;background:#111;color:#fff;font-size:17px;font-weight:bold}',
    '</style>',
    '</head>',
    '<body>',
    '<main>',
    '<h1>Pill Reminder</h1>',
    '<section>',
    '<h2>Darstellung</h2>',
    '<label>Theme',
    '<select id="theme">',
    '<option value="light"' + selected(theme, 'light') + '>Hell</option>',
    '<option value="dark"' + selected(theme, 'dark') + '>Dunkel</option>',
    '</select>',
    '</label>',
    '</section>',
    '<section>',
    '<h2>Medikament</h2>',
    '<label>Name',
    '<input id="name" type="text" maxlength="31" value="' + htmlEscape(medication.name) + '">',
    '</label>',
    '<label>Menge',
    '<input id="quantity" type="number" min="1" max="20" value="' + medication.quantity + '">',
    '</label>',
    '<label>Zeitpunkt',
    '<select id="time">',
    '<option value="0"' + selected(medication.time, 0) + '>Früh</option>',
    '<option value="1"' + selected(medication.time, 1) + '>Mittag</option>',
    '<option value="2"' + selected(medication.time, 2) + '>Abend</option>',
    '<option value="3"' + selected(medication.time, 3) + '>Nacht</option>',
    '</select>',
    '</label>',
    '<label>Rhythmus',
    '<select id="schedule">',
    '<option value="0"' + selected(medication.schedule, 0) + '>Täglich</option>',
    '<option value="1"' + selected(medication.schedule, 1) + '>Wöchentlich</option>',
    '<option value="2"' + selected(medication.schedule, 2) + '>Monatlich</option>',
    '</select>',
    '</label>',
    '<label id="weekday-row">Wochentag',
    '<select id="weekday">',
    '<option value="0"' + selected(medication.day, 0) + '>Montag</option>',
    '<option value="1"' + selected(medication.day, 1) + '>Dienstag</option>',
    '<option value="2"' + selected(medication.day, 2) + '>Mittwoch</option>',
    '<option value="3"' + selected(medication.day, 3) + '>Donnerstag</option>',
    '<option value="4"' + selected(medication.day, 4) + '>Freitag</option>',
    '<option value="5"' + selected(medication.day, 5) + '>Samstag</option>',
    '<option value="6"' + selected(medication.day, 6) + '>Sonntag</option>',
    '</select>',
    '</label>',
    '<label id="monthday-row">Tag im Monat',
    '<input id="monthday" type="number" min="1" max="31" value="' + (medication.schedule === 2 ? medication.day : 1) + '">',
    '</label>',
    '<label>Symbol',
    '<select id="symbol">',
    '<option value="0"' + selected(medication.symbol, 0) + '>Pille</option>',
    '<option value="1"' + selected(medication.symbol, 1) + '>Pen / Spritze</option>',
    '<option value="2"' + selected(medication.symbol, 2) + '>Tube / Creme</option>',
    '</select>',
    '</label>',
    '<label class="check">',
    '<input id="enabled" type="checkbox"' + checked(medication.enabled) + '>',
    '<span>Aktiv</span>',
    '</label>',
    '</section>',
    '<button id="save" type="button">Speichern</button>',
    '</main>',
    '<script>',
    'function number(id){return parseInt(document.getElementById(id).value,10);}',
    'function updateDayFields(){',
    'var schedule=number("schedule");',
    'document.getElementById("weekday-row").className=schedule===1?"":"hidden";',
    'document.getElementById("monthday-row").className=schedule===2?"":"hidden";',
    '}',
    'document.getElementById("schedule").onchange=updateDayFields;',
    'updateDayFields();',
    'document.getElementById("save").onclick=function(){',
    'var schedule=number("schedule");',
    'var day=schedule===1?number("weekday"):(schedule===2?number("monthday"):0);',
    'var result={',
    'theme:document.getElementById("theme").value,',
    'medication:{',
    'name:document.getElementById("name").value.trim(),',
    'quantity:number("quantity"),',
    'time:number("time"),',
    'schedule:schedule,',
    'day:day,',
    'symbol:number("symbol"),',
    'enabled:document.getElementById("enabled").checked',
    '}',
    '};',
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(result));',
    '};',
    '</script>',
    '</body>',
    '</html>'
  ].join('');
}

Pebble.addEventListener('ready', function() {
  sendSettings(
    currentTheme(),
    currentMedication()
  );
});

Pebble.addEventListener('showConfiguration', function() {
  var page = configurationPage(
    currentTheme(),
    currentMedication()
  );

  Pebble.openURL(
    'data:text/html;charset=utf-8,' +
    encodeURIComponent(page)
  );
});

Pebble.addEventListener('webviewclosed', function(event) {
  if (!event || !event.response) {
    return;
  }

  try {
    var settings = JSON.parse(
      decodeURIComponent(event.response)
    );

    if (
      settings.theme !== 'light' &&
      settings.theme !== 'dark'
    ) {
      return;
    }

    var medication = normalizeMedication(
      settings.medication
    );

    localStorage.setItem(
      THEME_STORAGE_KEY,
      settings.theme
    );

    localStorage.setItem(
      MEDICATION_STORAGE_KEY,
      JSON.stringify(medication)
    );

    sendSettings(
      settings.theme,
      medication
    );
  } catch (error) {
    console.log(
      'Could not save settings: ' +
      error.message
    );
  }
});
