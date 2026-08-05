var STORAGE_KEY = 'pill-reminder-theme';

function currentTheme() {
  return localStorage.getItem(STORAGE_KEY) || 'dark';
}

function configurationPage(theme) {
  var lightSelected = theme === 'light' ? ' selected' : '';
  var darkSelected = theme === 'dark' ? ' selected' : '';

  return [
    '<!doctype html>',
    '<html>',
    '<head>',
    '<meta charset="utf-8">',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>Pill Reminder</title>',
    '<style>',
    'body{margin:0;background:#f2f2f2;color:#111;font-family:sans-serif}',
    'main{max-width:480px;margin:auto;padding:24px 18px}',
    'h1{font-size:24px;margin:0 0 22px}',
    'label{display:block;font-size:15px;font-weight:bold}',
    'select{box-sizing:border-box;width:100%;margin-top:8px;padding:12px;font-size:17px}',
    'button{width:100%;margin-top:24px;padding:13px;border:0;border-radius:8px;background:#111;color:#fff;font-size:17px;font-weight:bold}',
    '</style>',
    '</head>',
    '<body>',
    '<main>',
    '<h1>Pill Reminder</h1>',
    '<label>Theme',
    '<select id="theme">',
    '<option value="light"' + lightSelected + '>Hell</option>',
    '<option value="dark"' + darkSelected + '>Dunkel</option>',
    '</select>',
    '</label>',
    '<button id="save" type="button">Speichern</button>',
    '</main>',
    '<script>',
    'document.getElementById("save").onclick=function(){',
    'var value=document.getElementById("theme").value;',
    'var result=encodeURIComponent(JSON.stringify({theme:value}));',
    'document.location="pebblejs://close#"+result;',
    '};',
    '</script>',
    '</body>',
    '</html>'
  ].join('');
}

Pebble.addEventListener('showConfiguration', function() {
  var page = configurationPage(currentTheme());

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
      settings.theme === 'light' ||
      settings.theme === 'dark'
    ) {
      localStorage.setItem(
        STORAGE_KEY,
        settings.theme
      );
    }
  } catch (error) {
    console.log(
      'Could not save theme: ' +
      error.message
    );
  }
});
