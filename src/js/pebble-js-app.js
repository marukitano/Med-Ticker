var THEME_STORAGE_KEY = 'pill-reminder-theme';
var LEGACY_MEDICATION_STORAGE_KEY = 'pill-reminder-medication-v1';
var MEDICATIONS_STORAGE_KEY = 'pill-reminder-medications-v2';

var MAX_MEDICATIONS = 8;

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

var COMMAND_RESET = 0;
var COMMAND_ITEM = 1;
var COMMAND_COMMIT = 2;

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

function blankMedication() {
  return {
    name: '',
    quantity: 1,
    time: 0,
    schedule: 0,
    day: 0,
    symbol: 0,
    enabled: true
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
  message[MED_DAY_KEY] = medication.day;
  message[MED_SYMBOL_KEY] = medication.symbol;
  message[MED_ENABLED_KEY] =
      medication.enabled ? 1 : 0;

  sendMessage(message, function() {
    sendMedicationAt(
      medications,
      index + 1
    );
  });
}

function sendAllSettings(theme, medications) {
  var themeMessage = {};
  themeMessage[THEME_KEY] =
      theme === 'light' ? 1 : 0;

  sendMessage(themeMessage, function() {
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

function configurationPage(theme, medications) {
  var initialMedications =
      safeJsonForScript(medications);

  var lightSelected =
      theme === 'light' ? ' selected' : '';
  var darkSelected =
      theme === 'dark' ? ' selected' : '';

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
    'section,.card{background:#fff;border-radius:10px;padding:15px;margin-bottom:13px}',
    'h2{font-size:18px;margin:0 0 13px}',
    'h3{font-size:17px;margin:0}',
    '.card-head{display:flex;align-items:center;justify-content:space-between;margin-bottom:13px}',
    'label{display:block;font-size:14px;font-weight:bold;margin-top:13px}',
    'input[type=text],input[type=number],select{box-sizing:border-box;width:100%;margin-top:6px;padding:10px;font-size:16px}',
    '.check{display:flex;align-items:center;gap:10px}',
    '.check input{width:22px;height:22px}',
    '.hidden{display:none}',
    '.remove{border:0;background:#eee;border-radius:7px;padding:8px 10px;font-size:14px}',
    '.add{width:100%;padding:12px;border:1px solid #111;border-radius:8px;background:#fff;color:#111;font-size:16px;font-weight:bold;margin-bottom:14px}',
    '.save{width:100%;padding:13px;border:0;border-radius:8px;background:#111;color:#fff;font-size:17px;font-weight:bold}',
    '.empty{text-align:center;color:#666;padding:18px 6px}',
    '</style>',
    '</head>',
    '<body>',
    '<main>',
    '<h1>Pill Reminder</h1>',
    '<form id="settings">',
    '<section>',
    '<h2>Darstellung</h2>',
    '<label>Theme',
    '<select id="theme">',
    '<option value="light"' + lightSelected + '>Hell</option>',
    '<option value="dark"' + darkSelected + '>Dunkel</option>',
    '</select>',
    '</label>',
    '</section>',
    '<div id="medications"></div>',
    '<button id="add" class="add" type="button">Medikament hinzufügen</button>',
    '<button class="save" type="submit">Speichern</button>',
    '</form>',
    '</main>',
    '<script>',
    'var MAX_MEDICATIONS=' + MAX_MEDICATIONS + ';',
    'var medications=' + initialMedications + ';',
    'function escapeHtml(value){',
    'return String(value).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");',
    '}',
    'function option(value,label,current){',
    'return "<option value=\\""+value+"\\""+(value===current?" selected":"")+">"+label+"</option>";',
    '}',
    'function blankMedication(){',
    'return {name:"",quantity:1,time:0,schedule:0,day:0,symbol:0,enabled:true};',
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
    'result.push({',
    'name:card.querySelector("[data-field=\\"name\\"]").value.trim(),',
    'quantity:numberValue(card,"quantity"),',
    'time:numberValue(card,"time"),',
    'schedule:schedule,',
    'day:day,',
    'symbol:numberValue(card,"symbol"),',
    'enabled:card.querySelector("[data-field=\\"enabled\\"]").checked',
    '});',
    '}',
    'return result;',
    '}',
    'function updateDayFields(card){',
    'var schedule=numberValue(card,"schedule");',
    'card.querySelector(".weekday").className=schedule===1?"weekday":"weekday hidden";',
    'card.querySelector(".monthday").className=schedule===2?"monthday":"monthday hidden";',
    '}',
    'function render(){',
    'var host=document.getElementById("medications");',
    'if(medications.length===0){host.innerHTML="<section class=\\"empty\\">Noch kein Medikament angelegt.</section>";}',
    'else{',
    'var html="";',
    'for(var i=0;i<medications.length;i++){',
    'var med=medications[i];',
    'html+="<section class=\\"card\\" data-index=\\""+i+"\\">";',
    'html+="<div class=\\"card-head\\"><h3>Einnahmeplan "+(i+1)+"</h3><button class=\\"remove\\" type=\\"button\\" data-remove=\\""+i+"\\">Löschen</button></div>";',
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
    'html+="<label>Symbol<select data-field=\\"symbol\\">";',
    'html+=option(0,"Pille",med.symbol)+option(1,"Pen / Spritze",med.symbol)+option(2,"Tube / Creme",med.symbol);',
    'html+="</select></label>";',
    'html+="<label class=\\"check\\"><input data-field=\\"enabled\\" type=\\"checkbox\\""+(med.enabled?" checked":"")+"><span>Aktiv</span></label>";',
    'html+="</section>";',
    '}',
    'host.innerHTML=html;',
    '}',
    'var cards=document.querySelectorAll(".card");',
    'for(var c=0;c<cards.length;c++){',
    'updateDayFields(cards[c]);',
    'cards[c].querySelector("[data-field=\\"schedule\\"]").onchange=(function(card){return function(){updateDayFields(card);};})(cards[c]);',
    '}',
    'var removeButtons=document.querySelectorAll("[data-remove]");',
    'for(var r=0;r<removeButtons.length;r++){',
    'removeButtons[r].onclick=function(){',
    'medications=readMedications();',
    'medications.splice(parseInt(this.getAttribute("data-remove"),10),1);',
    'render();',
    '};',
    '}',
    'document.getElementById("add").disabled=medications.length>=MAX_MEDICATIONS;',
    '}',
    'document.getElementById("add").onclick=function(){',
    'medications=readMedications();',
    'if(medications.length<MAX_MEDICATIONS){medications.push(blankMedication());render();}',
    '};',
    'document.getElementById("settings").onsubmit=function(event){',
    'event.preventDefault();',
    'medications=readMedications();',
    'var result={theme:document.getElementById("theme").value,medications:medications};',
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(result));',
    '};',
    'render();',
    '</script>',
    '</body>',
    '</html>'
  ].join('');
}

Pebble.addEventListener('ready', function() {
  sendAllSettings(
    currentTheme(),
    currentMedications()
  );
});

Pebble.addEventListener('showConfiguration', function() {
  var page = configurationPage(
    currentTheme(),
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

    var medications = normalizeMedications(
      settings.medications
    );

    localStorage.setItem(
      THEME_STORAGE_KEY,
      settings.theme
    );

    localStorage.setItem(
      MEDICATIONS_STORAGE_KEY,
      JSON.stringify(medications)
    );

    sendAllSettings(
      settings.theme,
      medications
    );
  } catch (error) {
    console.log(
      'Could not save settings: ' +
      error.message
    );
  }
});
