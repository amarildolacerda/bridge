#pragma once

static const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP8266 Sensor</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#010102;color:#f7f8f8;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#0f1011;border-radius:12px;padding:24px;text-align:center;max-width:360px;width:90%;border:1px solid #23252a}
h1{font-size:1.3rem;color:#5e6ad2;margin-bottom:4px}
.ip-badge{background:#141516;color:#8a8f98;font-size:.75rem;padding:3px 10px;border-radius:12px;display:inline-block;margin-bottom:12px;font-family:monospace}
.valor-sensor{font-size:2.5rem;margin:.25rem 0;color:#f7f8f8}
.label{color:#8a8f98;font-size:.85rem;margin-bottom:4px}
.info{color:#62666d;font-size:.8rem;margin-top:16px;word-break:break-all}
</style>
</head>
<body>
<div class="card">
<h1>Temperatura Sala</h1>
<div class="ip-badge" id="ipBadge">http://--.---.---.---</div>
<div class="label">Temperatura</div>
<div class="valor-sensor" id="temp">--.-°C</div>
<div class="label">Umidade</div>
<div class="valor-sensor" id="humid">--.-%</div>
<div class="info" id="info"></div>
</div>
<script>
const ipEl=document.getElementById('ipBadge');
const tempEl=document.getElementById('temp');
const humEl=document.getElementById('humid');
const inf=document.getElementById('info');
async function fetchState(){
try{
const r=await fetch('/api/state');const d=await r.json();
ipEl.textContent='http://'+d.ip;
tempEl.textContent=d.temperature.toFixed(1)+'\u00B0C';
humEl.textContent=d.humidity.toFixed(1)+'%';
    let u=d.uptime_s|0,upt=Math.floor(u/86400)+'d '+Math.floor((u%86400)/3600)+'h '+Math.floor((u%3600)/60)+'m '+u%60+'s';
    let ls=d.last_send_s;let lastSend=ls==null?'nunca':ls<60?ls+'s':ls<3600?Math.floor(ls/60)+'m':Math.floor(ls/3600)+'h';
    inf.innerHTML='RSSI: '+d.rssi+'dBm | Up: '+upt+'<br>Última coleta: '+lastSend;
}catch{inf.textContent='Erro de conex\u00E3o'}
}
fetchState();setInterval(fetchState,3000)
</script>
</body>
</html>
)rawliteral";
