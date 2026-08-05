var THEME_STORAGE_KEY = 'pill-reminder-theme';
var LEGACY_MEDICATION_STORAGE_KEY = 'pill-reminder-medication-v1';
var MEDICATIONS_STORAGE_KEY = 'pill-reminder-medications-v2';
var DAYPART_STORAGE_KEY = 'pill-reminder-dayparts-v1';

var MAX_MEDICATIONS = 8;
var MINUTES_PER_DAY = 1440;

var THEME_KEY = 0;
var MED_NAME_KEY = 1;
var MED_QUANTITY_KEY = 2;
var MED_TIME_KEY = 3;
var MED_SCHEDULE_KEY = 4;
var MED_DAY_KEY = 5;
var MED_SYMBOL_KEY = 6;
var MED_ENABLED_KEY = 7;
var MED_INDEX_KEY = 8;
var MED_COUNT_KEY = 9;
var MED_COMMAND_KEY = 10;
var DAYPART_MORNING_KEY = 11;
var DAYPART_NOON_KEY = 12;
var DAYPART_EVENING_KEY = 13;
var DAYPART_NIGHT_KEY = 14;
var MED_SHAPE_KEY = 15;
var MED_COLOR_KEY = 16;
var MED_ICON_SET_KEY = 17;

var COMMAND_RESET = 0;
var COMMAND_ITEM = 1;
var COMMAND_COMMIT = 2;

var DEFAULT_DAYPARTS = {
  morning: 5 * 60,
  noon: 11 * 60,
  evening: 16 * 60,
  night: 21 * 60
};

var DEFAULT_MEDICATION = {
  name: 'Xarelto 20 mg',
  quantity: 1,
  time: 0,
  schedule: 0,
  day: 0,
  symbol: 0,
  shape: 2,
  color: 255,
  iconSet: true,
  enabled: true
};

function currentTheme() {
  return localStorage.getItem(THEME_STORAGE_KEY) || 'dark';
}

function cloneDefaultDayparts() {
  return {
    morning: DEFAULT_DAYPARTS.morning,
    noon: DEFAULT_DAYPARTS.noon,
    evening: DEFAULT_DAYPARTS.evening,
    night: DEFAULT_DAYPARTS.night
  };
}

function daypartsValid(value) {
  return (
    value &&
    integerInRange(
      value.morning,
      0,
      MINUTES_PER_DAY - 1
    ) &&
    integerInRange(
      value.noon,
      0,
      MINUTES_PER_DAY - 1
    ) &&
    integerInRange(
      value.evening,
      0,
      MINUTES_PER_DAY - 1
    ) &&
    integerInRange(
      value.night,
      0,
      MINUTES_PER_DAY - 1
    ) &&
    value.morning < value.noon &&
    value.noon < value.evening &&
    value.evening < value.night
  );
}

function normalizeDayparts(value) {
  if (!daypartsValid(value)) {
    return cloneDefaultDayparts();
  }

  return {
    morning: value.morning,
    noon: value.noon,
    evening: value.evening,
    night: value.night
  };
}

function currentDayparts() {
  var stored = localStorage.getItem(
    DAYPART_STORAGE_KEY
  );

  if (!stored) {
    return cloneDefaultDayparts();
  }

  try {
    return normalizeDayparts(
      JSON.parse(stored)
    );
  } catch (error) {
    console.log(
      'Could not read dayparts: ' +
      error.message
    );

    return cloneDefaultDayparts();
  }
}

function cloneDefaultMedication() {
  return {
    name: DEFAULT_MEDICATION.name,
    quantity: DEFAULT_MEDICATION.quantity,
    time: DEFAULT_MEDICATION.time,
    schedule: DEFAULT_MEDICATION.schedule,
    day: DEFAULT_MEDICATION.day,
    symbol: DEFAULT_MEDICATION.symbol,
    shape: DEFAULT_MEDICATION.shape,
    color: DEFAULT_MEDICATION.color,
    iconSet: true,
    enabled: DEFAULT_MEDICATION.enabled
  };
}

function blankMedication() {
  return {
    name: '',
    quantity: 1,
    time: 0,
    schedule: 0,
    day: 0,
    symbol: -1,
    shape: -1,
    color: -1,
    iconSet: false,
    enabled: false
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

function utf8Length(value) {
  return unescape(
    encodeURIComponent(value)
  ).length;
}

function truncateUtf8(value, maximumBytes) {
  while (
    value.length > 0 &&
    utf8Length(value) > maximumBytes
  ) {
    value = value.slice(0, -1);
  }

  return value;
}

function normalizeMedication(value) {
  if (!value || typeof value !== 'object') {
    return cloneDefaultMedication();
  }

  var name = typeof value.name === 'string'
    ? value.name.trim()
    : '';

  name = truncateUtf8(name, 31);

  if (!name) {
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

  var symbolValid = integerInRange(
    value.symbol,
    0,
    1
  );

  var symbol = symbolValid
    ? value.symbol
    : -1;

  var shapeValid = integerInRange(
    value.shape,
    0,
    3
  );

  var shape = shapeValid
    ? value.shape
    : -1;

  var colorValid = integerInRange(
    value.color,
    192,
    255
  );

  var color = colorValid
    ? value.color
    : -1;

  var iconSet =
      symbolValid &&
      (
        symbol === 1 ||
        (shapeValid && colorValid)
      );

  return {
    name: name,
    quantity: quantity,
    time: time,
    schedule: schedule,
    day: day,
    symbol: symbol,
    shape: shape,
    color: color,
    iconSet: iconSet,
    enabled: value.enabled !== false && iconSet
  };
}

function normalizeMedications(values) {
  if (!Array.isArray(values)) {
    return [cloneDefaultMedication()];
  }

  var result = [];

  for (
    var index = 0;
    index < values.length &&
        result.length < MAX_MEDICATIONS;
    index++
  ) {
    result.push(
      normalizeMedication(values[index])
    );
  }

  return result;
}

function currentMedications() {
  var storedList = localStorage.getItem(
    MEDICATIONS_STORAGE_KEY
  );

  if (storedList) {
    try {
      return normalizeMedications(
        JSON.parse(storedList)
      );
    } catch (error) {
      console.log(
        'Could not read medication list: ' +
        error.message
      );
    }
  }

  var legacy = localStorage.getItem(
    LEGACY_MEDICATION_STORAGE_KEY
  );

  if (legacy) {
    try {
      return [
        normalizeMedication(
          JSON.parse(legacy)
        )
      ];
    } catch (error) {
      console.log(
        'Could not migrate medication: ' +
        error.message
      );
    }
  }

  return [cloneDefaultMedication()];
}

function sendMessage(message, next) {
  Pebble.sendAppMessage(
    message,
    function() {
      if (next) {
        next();
      }
    },
    function(error) {
      console.log(
        'Settings message failed: ' +
        JSON.stringify(error)
      );
    }
  );
}

function sendMedicationList(medications) {
  var reset = {};
  reset[MED_COMMAND_KEY] = COMMAND_RESET;
  reset[MED_COUNT_KEY] = medications.length;

  sendMessage(reset, function() {
    sendMedicationAt(
      medications,
      0
    );
  });
}

function sendMedicationAt(
    medications,
    index
) {
  if (index >= medications.length) {
    var commit = {};
    commit[MED_COMMAND_KEY] = COMMAND_COMMIT;
    commit[MED_COUNT_KEY] = medications.length;
    sendMessage(commit);
    return;
  }

  var medication = medications[index];
  var message = {};

  message[MED_COMMAND_KEY] = COMMAND_ITEM;
  message[MED_INDEX_KEY] = index;
  message[MED_COUNT_KEY] = medications.length;
  message[MED_NAME_KEY] = medication.name;
  message[MED_QUANTITY_KEY] = medication.quantity;
  message[MED_TIME_KEY] = medication.time;
  message[MED_SCHEDULE_KEY] = medication.schedule;
  var iconSet = medication.iconSet === true;

  message[MED_DAY_KEY] = medication.day;
  message[MED_SYMBOL_KEY] = iconSet &&
      integerInRange(medication.symbol, 0, 1)
      ? medication.symbol
      : 0;
  message[MED_SHAPE_KEY] = iconSet &&
      integerInRange(medication.shape, 0, 3)
      ? medication.shape
      : DEFAULT_MEDICATION.shape;
  message[MED_COLOR_KEY] = iconSet &&
      integerInRange(medication.color, 192, 255)
      ? medication.color
      : DEFAULT_MEDICATION.color;
  message[MED_ICON_SET_KEY] = iconSet ? 1 : 0;
  message[MED_ENABLED_KEY] =
      medication.enabled && iconSet ? 1 : 0;

  sendMessage(message, function() {
    sendMedicationAt(
      medications,
      index + 1
    );
  });
}

function sendAllSettings(
    theme,
    dayparts,
    medications
) {
  var settingsMessage = {};

  settingsMessage[THEME_KEY] =
      theme === 'light' ? 1 : 0;
  settingsMessage[DAYPART_MORNING_KEY] =
      dayparts.morning;
  settingsMessage[DAYPART_NOON_KEY] =
      dayparts.noon;
  settingsMessage[DAYPART_EVENING_KEY] =
      dayparts.evening;
  settingsMessage[DAYPART_NIGHT_KEY] =
      dayparts.night;

  sendMessage(settingsMessage, function() {
    sendMedicationList(medications);
  });
}

function safeJsonForScript(value) {
  return JSON.stringify(value)
    .replace(/</g, '\\u003c')
    .replace(/>/g, '\\u003e')
    .replace(/&/g, '\\u0026')
    .replace(/\u2028/g, '\\u2028')
    .replace(/\u2029/g, '\\u2029');
}

function minutesToTime(value) {
  var hours = Math.floor(value / 60);
  var minutes = value % 60;

  return (
    (hours < 10 ? '0' : '') +
    hours +
    ':' +
    (minutes < 10 ? '0' : '') +
    minutes
  );
}

function timeToMinutes(value) {
  if (
    typeof value !== 'string' ||
    !/^\d{2}:\d{2}$/.test(value)
  ) {
    return -1;
  }

  var parts = value.split(':');
  var hours = parseInt(parts[0], 10);
  var minutes = parseInt(parts[1], 10);

  if (
    hours < 0 ||
    hours > 23 ||
    minutes < 0 ||
    minutes > 59
  ) {
    return -1;
  }

  return hours * 60 + minutes;
}

function configurationPage(
    theme,
    dayparts,
    medications
) {
  var initialDayparts =
      safeJsonForScript(dayparts);
  var initialMedications =
      safeJsonForScript(medications);

  var lightSelected =
      theme === 'light' ? ' selected' : '';
  var darkSelected =
      theme === 'dark' ? ' selected' : '';
  var bodyClass =
      theme === 'dark' ? ' class="dark"' : '';

  return [
    '<!doctype html>',
    '<html>',
    '<head>',
    '<meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>Pill Reminder</title>',
    '<style>',
    'body{margin:0;background:#f2f2f2;color:#111;font-family:sans-serif}',
    'main{max-width:520px;margin:auto;padding:20px 14px 36px}',
    'h1{font-size:24px;margin:0 0 18px}',
    'section,.card{background:#fff;border-radius:10px;margin-bottom:13px}',
    '.plain{padding:15px}',
    'h2{font-size:18px;margin:0 0 13px}',
    'h3{font-size:17px;margin:0}',
    '.toggle{box-sizing:border-box;width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;padding:15px;border:0;background:transparent;color:#111;text-align:left;font-size:17px;font-weight:bold}',
    '.summary-main{display:block;overflow:hidden;text-overflow:ellipsis}',
    '.summary-sub{display:block;margin-top:3px;color:#666;font-size:13px;font-weight:normal}',
    '.arrow{font-size:24px;line-height:1;transform:rotate(90deg)}',
    '.collapsed .arrow{transform:none}',
    '.body{padding:0 15px 15px}',
    '.hidden{display:none}',
    'label{display:block;font-size:14px;font-weight:bold;margin-top:13px}',
    'input[type=text],input[type=number],input[type=time],select{box-sizing:border-box;width:100%;margin-top:6px;padding:10px;font-size:16px}',
    '.check{display:flex;align-items:center;gap:10px}',
    '.check input{width:22px;height:22px}',
    '.remove{width:100%;margin-top:16px;border:0;background:#eee;border-radius:7px;padding:10px;font-size:15px}',
    '.add{width:100%;padding:12px;border:1px solid #111;border-radius:8px;background:#fff;color:#111;font-size:16px;font-weight:bold;margin-bottom:14px}',
    '.save{width:100%;padding:13px;border:0;border-radius:8px;background:#111;color:#fff;font-size:17px;font-weight:bold}',
    '.empty{text-align:center;color:#666;padding:18px 6px}',
    '.note{color:#666;font-size:13px;line-height:1.35;margin-top:12px}',
    '.icon-fields{margin-top:14px;padding:12px;border:1px solid #d2d2d2;border-radius:9px}',
    '.icon-preview-row{display:flex;align-items:center;gap:13px;margin-bottom:3px}',
    '.icon-preview-text{font-size:14px;font-weight:bold}',
    '.pill-preview{width:54px;height:44px;display:flex;align-items:center;justify-content:center;background:#e8e8e8;border-radius:8px}',
    '.pill-shape{display:block;border:2px solid rgba(0,0,0,.38);box-shadow:inset 0 1px 2px rgba(255,255,255,.55)}',
    '.shape-0{width:28px;height:28px;border-radius:50%}',
    '.shape-1{width:39px;height:24px;border-radius:50%}',
    '.shape-2{width:42px;height:21px;border-radius:12px}',
    '.shape-3{width:27px;height:27px;border-radius:4px;transform:rotate(45deg)}',
    '.color-grid{display:grid;grid-template-columns:repeat(8,1fr);gap:6px;margin-top:8px}',
    '.color-swatch{aspect-ratio:1;border:2px solid rgba(0,0,0,.28);border-radius:5px;padding:0;min-width:0}',
    '.color-swatch.selected{outline:3px solid #111;outline-offset:1px}',
    '.icon-warning{color:#b00020;font-weight:bold}',
    '.check.disabled{opacity:.55}',
    'body.dark{background:#111;color:#f4f4f4;color-scheme:dark}',
    'body.dark section,body.dark .card{background:#202020}',
    'body.dark .toggle{color:#f4f4f4}',
    'body.dark .summary-sub,body.dark .empty,body.dark .note{color:#aaa}',
    'body.dark input[type=text],body.dark input[type=number],body.dark input[type=time],body.dark select{background:#2b2b2b;color:#fff;border:1px solid #555}',
    'body.dark .remove{background:#333;color:#f4f4f4}',
    'body.dark .add{border-color:#ddd;background:#202020;color:#f4f4f4}',
    'body.dark .save{background:#f2f2f2;color:#111}',
    'body.dark .icon-fields{border-color:#505050}',
    'body.dark .pill-preview{background:#303030}',
    'body.dark .color-swatch.selected{outline-color:#fff}',
    'body.dark .icon-warning{color:#ff8a9a}',
    '</style>',
    '</head>',
    '<body' + bodyClass + '>',
    '<main>',
    '<h1>Pill Reminder</h1>',
    '<form id="settings">',
    '<section class="plain">',
    '<h2>Darstellung</h2>',
    '<label>Theme',
    '<select id="theme">',
    '<option value="light"' + lightSelected + '>Hell</option>',
    '<option value="dark"' + darkSelected + '>Dunkel</option>',
    '</select>',
    '</label>',
    '</section>',
    '<section id="daypart-panel" class="collapsed">',
    '<button id="daypart-toggle" class="toggle" type="button">',
    '<span><span class="summary-main">Tageszeiten</span>',
    '<span class="summary-sub">Früh, Mittag, Abend und Nacht</span></span>',
    '<span class="arrow">›</span>',
    '</button>',
    '<div id="daypart-body" class="body hidden">',
    '<label>Früh beginnt',
    '<input id="morning" type="time" required value="' + minutesToTime(dayparts.morning) + '">',
    '</label>',
    '<label>Mittag beginnt',
    '<input id="noon" type="time" required value="' + minutesToTime(dayparts.noon) + '">',
    '</label>',
    '<label>Abend beginnt',
    '<input id="evening" type="time" required value="' + minutesToTime(dayparts.evening) + '">',
    '</label>',
    '<label>Nacht beginnt',
    '<input id="night" type="time" required value="' + minutesToTime(dayparts.night) + '">',
    '</label>',
    '<div class="note">Die Zeiten müssen in dieser Reihenfolge liegen. Nacht läuft bis zum Beginn von Früh.</div>',
    '</div>',
    '</section>',
    '<section id="medication-panel" class="collapsed">',
    '<button id="medication-toggle" class="toggle" type="button">',
    '<span><span class="summary-main">Medikamente</span>',
    '<span class="summary-sub">Hinzufügen, bearbeiten und deaktivieren</span></span>',
    '<span class="arrow">›</span>',
    '</button>',
    '<div id="medication-body" class="body hidden">',
    '<div id="medications"></div>',
    '<button id="add" class="add" type="button">Medikament hinzufügen</button>',
    '</div>',
    '</section>',
    '<button class="save" type="submit">Speichern</button>',
    '</form>',
    '</main>',
    '<script>',
    'var MAX_MEDICATIONS=' + MAX_MEDICATIONS + ';',
    'var dayparts=' + initialDayparts + ';',
    'var medications=' + initialMedications + ';',
    'var timeNames=["Früh","Mittag","Abend","Nacht"];',
    'function escapeHtml(value){',
    'return String(value).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");',
    '}',
    'function option(value,label,current){',
    'return "<option value=\\""+value+"\\""+(value===current?" selected":"")+">"+label+"</option>";',
    '}',
    'function blankMedication(){',
    'return {name:"",quantity:1,time:0,schedule:0,day:0,symbol:-1,shape:-1,color:-1,iconSet:false,enabled:false};',
    '}',
    'function numberValue(card,name){',
    'return parseInt(card.querySelector("[data-field=\\""+name+"\\"]").value,10);',
    '}',
    'function readMedications(){',
    'var cards=document.querySelectorAll(".card");',
    'var result=[];',
    'for(var i=0;i<cards.length;i++){',
    'var card=cards[i];',
    'var schedule=numberValue(card,"schedule");',
    'var day=schedule===1?numberValue(card,"weekday"):(schedule===2?numberValue(card,"monthday"):0);',
    'var symbol=numberValue(card,"symbol");',
    'var shape=numberValue(card,"shape");',
    'var color=numberValue(card,"color");',
    'var iconSet=symbol===1||(symbol===0&&shape>=0&&shape<=3&&color>=192&&color<=255);',
    'var enabled=card.querySelector("[data-field=\\"enabled\\"]").checked&&iconSet;',
    'result.push({',
    'name:card.querySelector("[data-field=\\"name\\"]").value.trim(),',
    'quantity:numberValue(card,"quantity"),',
    'time:numberValue(card,"time"),',
    'schedule:schedule,',
    'day:day,',
    'symbol:symbol,',
    'shape:shape,',
    'color:color,',
    'iconSet:iconSet,',
    'enabled:enabled',
    '});',
    '}',
    'return result;',
    '}',
    'function updateDayFields(card){',
    'var schedule=numberValue(card,"schedule");',
    'card.querySelector(".weekday").className=schedule===1?"weekday":"weekday hidden";',
    'card.querySelector(".monthday").className=schedule===2?"monthday":"monthday hidden";',
    '}',
    'function pebbleColorHex(value){',
    'var red=((value>>4)&3)*85;',
    'var green=((value>>2)&3)*85;',
    'var blue=(value&3)*85;',
    'function hex(component){var result=component.toString(16);return result.length<2?"0"+result:result;}',
    'return "#"+hex(red)+hex(green)+hex(blue);',
    '}',
    'function paletteHtml(current){',
    'var html="";',
    'for(var value=192;value<=255;value++){',
    'var selected=value===current?" selected":"";',
    'html+="<button class=\\"color-swatch"+selected+"\\" type=\\"button\\" data-color=\\""+value+"\\" title=\\"Pebble ARGB8 0x"+value.toString(16).toUpperCase()+"\\" style=\\"background:"+pebbleColorHex(value)+"\\"></button>";',
    '}',
    'return html;',
    '}',
    'function updateIconFields(card){',
    'var fields=card.querySelector(".icon-fields");',
    'var symbol=numberValue(card,"symbol");',
    'var isTablet=symbol===0;',
    'var shape=numberValue(card,"shape");',
    'var color=numberValue(card,"color");',
    'var iconSet=symbol===1||(isTablet&&shape>=0&&shape<=3&&color>=192&&color<=255);',
    'fields.className=isTablet?"icon-fields":"icon-fields hidden";',
    'if(isTablet){',
    'var preview=card.querySelector(".pill-shape");',
    'preview.className=shape>=0&&shape<=3?"pill-shape shape-"+shape:"pill-shape hidden";',
    'preview.style.backgroundColor=color>=192&&color<=255?pebbleColorHex(color):"transparent";',
    'var swatches=card.querySelectorAll("[data-color]");',
    'for(var i=0;i<swatches.length;i++){',
    'var selected=parseInt(swatches[i].getAttribute("data-color"),10)===color;',
    'swatches[i].className=selected?"color-swatch selected":"color-swatch";',
    '}',
    '}',
    'var active=card.querySelector("[data-field=\\"enabled\\"]");',
    'if(!iconSet){active.checked=false;}',
    'active.disabled=!iconSet;',
    'active.parentNode.className=iconSet?"check":"check disabled";',
    'card.querySelector(".icon-warning").className=iconSet?"note icon-warning hidden":"note icon-warning";',
    '}',
    'function setCardOpen(card,open){',
    'var body=card.querySelector(".body");',
    'var toggle=card.querySelector(".toggle");',
    'body.className=open?"body":"body hidden";',
    'toggle.className=open?"toggle":"toggle collapsed";',
    '}',
    'function setPanelOpen(panelId,bodyId,open){',
    'var panel=document.getElementById(panelId);',
    'var body=document.getElementById(bodyId);',
    'body.className=open?"body":"body hidden";',
    'panel.className=open?"":"collapsed";',
    '}',
    'function render(openIndex){',
    'var host=document.getElementById("medications");',
    'if(medications.length===0){host.innerHTML="<section class=\\"empty\\">Noch kein Medikament angelegt.</section>";}',
    'else{',
    'var html="";',
    'for(var i=0;i<medications.length;i++){',
    'var med=medications[i];',
    'var title=med.name||"Neues Medikament";',
    'var iconReady=med.iconSet===true;',
    'var previewClass=med.shape>=0&&med.shape<=3?"pill-shape shape-"+med.shape:"pill-shape hidden";',
    'var sub=timeNames[med.time]+(med.quantity>1?" · x"+med.quantity:"")+(med.enabled?"":" · aus");',
    'html+="<section class=\\"card\\" data-index=\\""+i+"\\">";',
    'html+="<button class=\\"toggle collapsed\\" type=\\"button\\" data-toggle=\\""+i+"\\">";',
    'html+="<span><span class=\\"summary-main\\">"+escapeHtml(title)+"</span><span class=\\"summary-sub\\">"+escapeHtml(sub)+"</span></span><span class=\\"arrow\\">›</span></button>";',
    'html+="<div class=\\"body hidden\\">";',
    'html+="<label>Name<input data-field=\\"name\\" type=\\"text\\" required maxlength=\\"31\\" value=\\""+escapeHtml(med.name)+"\\"></label>";',
    'html+="<label>Menge<input data-field=\\"quantity\\" type=\\"number\\" min=\\"1\\" max=\\"20\\" required value=\\""+med.quantity+"\\"></label>";',
    'html+="<label>Zeitpunkt<select data-field=\\"time\\">";',
    'html+=option(0,"Früh",med.time)+option(1,"Mittag",med.time)+option(2,"Abend",med.time)+option(3,"Nacht",med.time);',
    'html+="</select></label>";',
    'html+="<label>Rhythmus<select data-field=\\"schedule\\">";',
    'html+=option(0,"Täglich",med.schedule)+option(1,"Wöchentlich",med.schedule)+option(2,"Monatlich",med.schedule);',
    'html+="</select></label>";',
    'html+="<label class=\\"weekday\\">Wochentag<select data-field=\\"weekday\\">";',
    'html+=option(0,"Montag",med.day)+option(1,"Dienstag",med.day)+option(2,"Mittwoch",med.day)+option(3,"Donnerstag",med.day)+option(4,"Freitag",med.day)+option(5,"Samstag",med.day)+option(6,"Sonntag",med.day);',
    'html+="</select></label>";',
    'html+="<label class=\\"monthday\\">Tag im Monat<input data-field=\\"monthday\\" type=\\"number\\" min=\\"1\\" max=\\"31\\" required value=\\""+(med.schedule===2?med.day:1)+"\\"></label>";',
    'html+="<label>Art<select data-field=\\"symbol\\">";',
    'html+=option(-1,"Bitte auswählen",med.symbol)+option(0,"Tablette",med.symbol)+option(1,"Pen / Spritze",med.symbol);',
    'html+="</select></label>";',
    'html+="<div class=\\"icon-fields\\">";',
    'html+="<div class=\\"icon-preview-row\\"><span class=\\"pill-preview\\"><span class=\\""+previewClass+"\\"></span></span><span class=\\"icon-preview-text\\">Tabletten-Icon *</span></div>";',
    'html+="<label>Form<select data-field=\\"shape\\">";',
    'html+=option(-1,"Bitte auswählen",med.shape)+option(0,"Rund",med.shape)+option(1,"Ellipse",med.shape)+option(2,"Pille / Kapsel",med.shape)+option(3,"Rhombus",med.shape);',
    'html+="</select></label>";',
    'html+="<input data-field=\\"color\\" type=\\"hidden\\" value=\\""+med.color+"\\">";',
    'html+="<label>Farbe *</label><div class=\\"color-grid\\">"+paletteHtml(med.color)+"</div>";',
    'html+="</div>";',
    'html+="<label class=\\""+(iconReady?"check":"check disabled")+"\\"><input data-field=\\"enabled\\" type=\\"checkbox\\""+(med.enabled?" checked":"")+(iconReady?"":" disabled")+"><span>Aktiv</span></label>";',
    'html+="<div class=\\""+(iconReady?"note icon-warning hidden":"note icon-warning")+"\\">Bitte zuerst ein vollständiges Icon auswählen. Erst danach kann das Medikament aktiviert werden.</div>";',
    'html+="<button class=\\"remove\\" type=\\"button\\" data-remove=\\""+i+"\\">Medikament löschen</button>";',
    'html+="</div></section>";',
    '}',
    'host.innerHTML=html;',
    '}',
    'var cards=document.querySelectorAll(".card");',
    'for(var c=0;c<cards.length;c++){',
    'var card=cards[c];',
    'updateDayFields(card);',
    'updateIconFields(card);',
    'card.querySelector("[data-field=\\"schedule\\"]").onchange=(function(item){return function(){updateDayFields(item);};})(card);',
    'card.querySelector("[data-field=\\"symbol\\"]").onchange=(function(item){return function(){updateIconFields(item);};})(card);',
    'card.querySelector("[data-field=\\"shape\\"]").onchange=(function(item){return function(){updateIconFields(item);};})(card);',
    'var colorButtons=card.querySelectorAll("[data-color]");',
    'for(var colorIndex=0;colorIndex<colorButtons.length;colorIndex++){',
    'colorButtons[colorIndex].onclick=(function(item){return function(){item.querySelector("[data-field=\\"color\\"]").value=this.getAttribute("data-color");updateIconFields(item);};})(card);',
    '}',
    'card.querySelector("[data-toggle]").onclick=(function(item){return function(){var hidden=item.querySelector(".body").className.indexOf("hidden")>=0;setCardOpen(item,hidden);};})(card);',
    'if(parseInt(card.getAttribute("data-index"),10)===openIndex){setCardOpen(card,true);}',
    '}',
    'var removeButtons=document.querySelectorAll("[data-remove]");',
    'for(var r=0;r<removeButtons.length;r++){',
    'removeButtons[r].onclick=function(){',
    'medications=readMedications();',
    'medications.splice(parseInt(this.getAttribute("data-remove"),10),1);',
    'render(-1);',
    '};',
    '}',
    'document.getElementById("add").disabled=medications.length>=MAX_MEDICATIONS;',
    '}',
    'document.getElementById("theme").onchange=function(){',
    'document.body.className=this.value==="dark"?"dark":"";',
    '};',
    'document.getElementById("daypart-toggle").onclick=function(){',
    'var body=document.getElementById("daypart-body");',
    'setPanelOpen("daypart-panel","daypart-body",body.className.indexOf("hidden")>=0);',
    '};',
    'document.getElementById("medication-toggle").onclick=function(){',
    'var body=document.getElementById("medication-body");',
    'setPanelOpen("medication-panel","medication-body",body.className.indexOf("hidden")>=0);',
    '};',
    'document.getElementById("add").onclick=function(){',
    'medications=readMedications();',
    'if(medications.length<MAX_MEDICATIONS){medications.push(blankMedication());render(medications.length-1);}',
    '};',
    'document.getElementById("settings").onsubmit=function(event){',
    'event.preventDefault();',
    'var values={',
    'morning:window.__timeToMinutes(document.getElementById("morning").value),',
    'noon:window.__timeToMinutes(document.getElementById("noon").value),',
    'evening:window.__timeToMinutes(document.getElementById("evening").value),',
    'night:window.__timeToMinutes(document.getElementById("night").value)',
    '};',
    'if(values.morning<0||values.noon<0||values.evening<0||values.night<0||!(values.morning<values.noon&&values.noon<values.evening&&values.evening<values.night)){',
    'alert("Bitte die Startzeiten in der Reihenfolge Früh, Mittag, Abend und Nacht einstellen.");',
    'document.getElementById("daypart-body").className="body";',
    'document.getElementById("daypart-panel").className="";',
    'return;',
    '}',
    'medications=readMedications();',
    'for(var medicationIndex=0;medicationIndex<medications.length;medicationIndex++){',
    'if(medications[medicationIndex].enabled&&!medications[medicationIndex].iconSet){alert("Ein aktives Medikament benötigt ein vollständiges Icon.");return;}',
    '}',
    'var result={theme:document.getElementById("theme").value,dayparts:values,medications:medications};',
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(result));',
    '};',
    'window.__timeToMinutes=' + timeToMinutes.toString() + ';',
    'render(-1);',
    '</script>',
    '</body>',
    '</html>'
  ].join('');
}

Pebble.addEventListener('ready', function() {
  sendAllSettings(
    currentTheme(),
    currentDayparts(),
    currentMedications()
  );
});

Pebble.addEventListener('showConfiguration', function() {
  var page = configurationPage(
    currentTheme(),
    currentDayparts(),
    currentMedications()
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

    var dayparts = normalizeDayparts(
      settings.dayparts
    );

    var medications = normalizeMedications(
      settings.medications
    );

    localStorage.setItem(
      THEME_STORAGE_KEY,
      settings.theme
    );

    localStorage.setItem(
      DAYPART_STORAGE_KEY,
      JSON.stringify(dayparts)
    );

    localStorage.setItem(
      MEDICATIONS_STORAGE_KEY,
      JSON.stringify(medications)
    );

    sendAllSettings(
      settings.theme,
      dayparts,
      medications
    );
  } catch (error) {
    console.log(
      'Could not save settings: ' +
      error.message
    );
  }
});
