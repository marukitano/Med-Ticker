var STORAGE_KEY = 'pill-reminder-plans-v1';
var CONFIG_DATA_KEY = 0;
var MAX_PLANS = 8;

var DEFAULT_PLANS = [
  {
    name: 'Xarelto 20 mg',
    quantity: 1,
    time: 0,
    schedule: 0,
    day: 0,
    symbol: 0,
    enabled: true
  },
  {
    name: 'Metformin 1000 mg',
    quantity: 1,
    time: 0,
    schedule: 0,
    day: 0,
    symbol: 0,
    enabled: true
  },
  {
    name: 'Pantoprazol 40 mg',
    quantity: 1,
    time: 0,
    schedule: 0,
    day: 0,
    symbol: 0,
    enabled: true
  }
];

function cloneDefaults() {
  return JSON.parse(JSON.stringify(DEFAULT_PLANS));
}

function loadPlans() {
  try {
    var saved = localStorage.getItem(STORAGE_KEY);
    return saved ? JSON.parse(saved) : cloneDefaults();
  } catch (error) {
    return cloneDefaults();
  }
}

function normalizePlan(plan) {
  return {
    name: String(plan.name || '').slice(0, 40),
    quantity: Math.max(1, Math.min(20, Number(plan.quantity) || 1)),
    time: Math.max(0, Math.min(3, Number(plan.time) || 0)),
    schedule: Math.max(0, Math.min(2, Number(plan.schedule) || 0)),
    day: Number(plan.day) || 0,
    symbol: Math.max(0, Math.min(2, Number(plan.symbol) || 0)),
    enabled: plan.enabled !== false
  };
}

function normalizePlans(plans) {
  var result = [];

  if (!Array.isArray(plans)) {
    return cloneDefaults();
  }

  for (var index = 0; index < plans.length && index < MAX_PLANS; index++) {
    var plan = normalizePlan(plans[index]);

    if (plan.name.trim()) {
      result.push(plan);
    }
  }

  return result;
}

function serializePlans(plans) {
  var records = [];

  for (var index = 0; index < plans.length; index++) {
    var plan = normalizePlan(plans[index]);
    var day = plan.schedule === 0
      ? 0
      : plan.schedule === 1
        ? Math.max(0, Math.min(6, plan.day))
        : Math.max(1, Math.min(31, plan.day || 1));

    records.push([
      plan.enabled ? 1 : 0,
      plan.quantity,
      plan.time,
      plan.schedule,
      day,
      plan.symbol,
      encodeURIComponent(plan.name.trim())
    ].join(String.fromCharCode(31)));
  }

  return records.join(String.fromCharCode(30));
}

function sendPlans(plans) {
  var dictionary = {};
  dictionary[CONFIG_DATA_KEY] = serializePlans(plans);

  Pebble.sendAppMessage(
    dictionary,
    function() {
      console.log('Medication configuration sent');
    },
    function(error) {
      console.log(
        'Medication configuration failed: ' +
        JSON.stringify(error)
      );
    }
  );
}

function htmlForPlans(plans) {
  var initial = JSON.stringify(normalizePlans(plans))
    .replace(/</g, '\\u003c');

  return [
    '<!doctype html>',
    '<html><head><meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>Pill Reminder</title>',
    '<style>',
    'body{margin:0;background:#111;color:#eee;font-family:system-ui,-apple-system,sans-serif}',
    'main{max-width:620px;margin:auto;padding:18px}',
    'h1{font-size:25px;margin:0 0 6px}',
    '.intro{color:#aaa;margin:0 0 18px}',
    '.card{background:#222;border:1px solid #3a3a3a;border-radius:14px;padding:15px;margin:12px 0}',
    '.head{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}',
    '.head strong{font-size:17px}',
    'label{display:block;font-size:13px;color:#bbb;margin-top:10px}',
    'input,select{box-sizing:border-box;width:100%;margin-top:5px;padding:11px;border:1px solid #555;border-radius:9px;background:#181818;color:#fff;font-size:16px}',
    '.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}',
    '.enabled{display:flex;align-items:center;gap:8px;margin-top:12px;color:#ddd}',
    '.enabled input{width:auto;margin:0}',
    'button{border:0;border-radius:10px;padding:12px 14px;font-size:16px;font-weight:700}',
    '.remove{background:#4a2525;color:#ffb8b8;padding:8px 10px}',
    '#add{width:100%;background:#333;color:#fff;margin-top:8px}',
    '#save{position:sticky;bottom:12px;width:100%;background:#f3f3f3;color:#111;margin-top:18px;box-shadow:0 4px 18px #000}',
    '.hidden{display:none}',
    '</style></head><body><main>',
    '<h1>Pill Reminder</h1>',
    '<p class="intro">Einnahmepläne auf der Uhr speichern. Die genauen Uhrzeiten für Früh, Mittag, Abend und Nacht folgen später.</p>',
    '<div id="plans"></div>',
    '<button id="add" type="button">+ Medikament hinzufügen</button>',
    '<button id="save" type="button">Speichern</button>',
    '</main><script>',
    'var MAX_PLANS=' + MAX_PLANS + ';',
    'var plans=' + initial + ';',
    'var root=document.getElementById("plans");',
    'function option(value,label,current){return "<option value=\\"" + value + "\\"" + (Number(current)===value?" selected":"") + ">" + label + "</option>";}',
    'function esc(value){return String(value).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/"/g,"&quot;");}',
    'function findCard(node){while(node&&node!==root){if((" "+node.className+" ").indexOf(" card ")>=0)return node;node=node.parentNode;}return null;}',
    'function hasClass(node,name){return (" "+node.className+" ").indexOf(" "+name+" ")>=0;}',
    'function render(){',
    'root.innerHTML="";',
    'for(var i=0;i<plans.length;i++){',
    'var p=plans[i],card=document.createElement("section");card.className="card";card.dataset.index=i;',
    'var weekly=p.schedule===1?"":" hidden",monthly=p.schedule===2?"":" hidden";',
    'card.innerHTML="<div class=\\"head\\"><strong>Einnahmeplan "+(i+1)+"</strong><button class=\\"remove\\" type=\\"button\\">Entfernen</button></div>"+',
    '"<label>Name<input class=\\"name\\" maxlength=\\"40\\" value=\\""+esc(p.name)+"\\"></label>"+',
    '"<div class=\\"row\\"><label>Menge<input class=\\"quantity\\" type=\\"number\\" min=\\"1\\" max=\\"20\\" value=\\""+p.quantity+"\\"></label>"+',
    '"<label>Symbol<select class=\\"symbol\\">"+option(0,"Pille",p.symbol)+option(1,"Spritze / Pen",p.symbol)+option(2,"Tube / Creme",p.symbol)+"</select></label></div>"+',
    '"<label>Zeitpunkt<select class=\\"time\\">"+option(0,"Früh",p.time)+option(1,"Mittag",p.time)+option(2,"Abend",p.time)+option(3,"Nacht",p.time)+"</select></label>"+',
    '"<label>Rhythmus<select class=\\"schedule\\">"+option(0,"Täglich",p.schedule)+option(1,"Wöchentlich",p.schedule)+option(2,"Monatlich",p.schedule)+"</select></label>"+',
    '"<label class=\\"weekly"+weekly+"\\">Wochentag<select class=\\"weekday\\">"+option(0,"Montag",p.day)+option(1,"Dienstag",p.day)+option(2,"Mittwoch",p.day)+option(3,"Donnerstag",p.day)+option(4,"Freitag",p.day)+option(5,"Samstag",p.day)+option(6,"Sonntag",p.day)+"</select></label>"+',
    '"<label class=\\"monthly"+monthly+"\\">Tag im Monat<input class=\\"monthday\\" type=\\"number\\" min=\\"1\\" max=\\"31\\" value=\\""+(p.day||1)+"\\"></label>"+',
    '"<label class=\\"enabled\\"><input class=\\"active\\" type=\\"checkbox\\" "+(p.enabled?"checked":"")+"> Aktiv</label>";',
    'root.appendChild(card);',
    '}',
    'document.getElementById("add").disabled=plans.length>=MAX_PLANS;',
    '}',
    'function readCard(card){',
    'var schedule=Number(card.querySelector(".schedule").value);',
    'return {',
    'name:card.querySelector(".name").value.trim(),',
    'quantity:Number(card.querySelector(".quantity").value),',
    'time:Number(card.querySelector(".time").value),',
    'schedule:schedule,',
    'day:schedule===0?0:schedule===1?Number(card.querySelector(".weekday").value):Number(card.querySelector(".monthday").value),',
    'symbol:Number(card.querySelector(".symbol").value),',
    'enabled:card.querySelector(".active").checked',
    '};',
    '}',
    'root.addEventListener("change",function(event){',
    'var card=findCard(event.target);if(!card)return;',
    'var index=Number(card.getAttribute("data-index"));plans[index]=readCard(card);',
    'if(hasClass(event.target,"schedule"))render();',
    '});',
    'root.addEventListener("click",function(event){',
    'if(!hasClass(event.target,"remove"))return;',
    'var card=findCard(event.target);plans.splice(Number(card.getAttribute("data-index")),1);render();',
    '});',
    'document.getElementById("add").onclick=function(){',
    'if(plans.length>=MAX_PLANS)return;',
    'plans.push({name:"",quantity:1,time:0,schedule:0,day:0,symbol:0,enabled:true});render();',
    '};',
    'document.getElementById("save").onclick=function(){',
    'var cards=root.querySelectorAll(".card"),result=[];',
    'for(var i=0;i<cards.length;i++){',
    'var plan=readCard(cards[i]);',
    'if(!plan.name){alert("Bitte bei jedem Eintrag einen Namen angeben.");return;}',
    'if(plan.quantity<1||plan.quantity>20){alert("Die Menge muss zwischen 1 und 20 liegen.");return;}',
    'if(plan.schedule===2&&(plan.day<1||plan.day>31)){alert("Der Monatstag muss zwischen 1 und 31 liegen.");return;}',
    'result.push(plan);',
    '}',
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(result));',
    '};',
    'render();',
    '</script></body></html>'
  ].join('');
}

function parseResponse(response) {
  if (!response) {
    return null;
  }

  try {
    return JSON.parse(decodeURIComponent(response));
  } catch (firstError) {
    try {
      return JSON.parse(response);
    } catch (secondError) {
      return null;
    }
  }
}

Pebble.addEventListener('ready', function() {
  sendPlans(normalizePlans(loadPlans()));
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(
    'data:text/html;charset=utf-8,' +
    encodeURIComponent(htmlForPlans(loadPlans()))
  );
});

Pebble.addEventListener('webviewclosed', function(event) {
  var plans = parseResponse(event && event.response);

  if (!plans) {
    return;
  }

  plans = normalizePlans(plans);
  localStorage.setItem(STORAGE_KEY, JSON.stringify(plans));
  sendPlans(plans);
});
