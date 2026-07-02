#pragma once

static const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DHT + Gas (ESP-NOW)</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#010102;color:#f7f8f8;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#0f1011;border-radius:12px;padding:24px;text-align:center;max-width:360px;width:90%;border:1px solid #23252a}
h1{font-size:1.3rem;color:#5e6ad2;margin-bottom:4px}
.ip-badge{background:#141516;color:#8a8f98;font-size:.75rem;padding:3px 10px;border-radius:12px;display:inline-block;margin-bottom:12px;font-family:monospace}
.valor-sensor{font-size:2.5rem;margin:.25rem 0;color:#f7f8f8}
.label{color:#8a8f98;font-size:.85rem;margin-bottom:4px}
.valor-gas{font-size:2.5rem;margin:.25rem 0;transition:color .3s}
.valor-gas.safe{color:#27a644}
.valor-gas.warn{color:#f5a623}
.valor-gas.alarm{color:#e5484d}
.alarm-badge{display:inline-block;padding:4px 14px;border-radius:20px;font-size:.8rem;font-weight:700;margin-top:4px;margin-bottom:8px;transition:background .3s}
.alarm-badge.safe{background:rgba(39,166,68,0.15);color:#27a644}
.alarm-badge.alert{background:rgba(255,152,0,0.15);color:#f5a623}
.alarm-badge.danger{background:rgba(229,72,77,0.15);color:#e5484d}
.divider{border:none;border-top:1px solid #23252a;margin:16px 0}
.info{color:#62666d;font-size:.8rem;margin-top:16px;word-break:break-all}
</style>
</head>
<body>
<div class="card">
<h1>Temp & Gas</h1>
<div class="ip-badge" id="ipBadge">http://--.---.---.---</div>
<div class="label">Temperatura</div>
<div class="valor-sensor" id="temp">--.-°C</div>
<div class="label">Umidade</div>
<div class="valor-sensor" id="humid">--.-%</div>
<hr class="divider">
<div class="label">Nivel de Gas</div>
<div class="valor-gas safe" id="gasLevel">---%</div>
<div class="alarm-badge safe" id="alarmBadge">Seguro</div>
<div class="info" id="info"></div>
</div>
<script>
const ipEl=document.getElementById('ipBadge');
const tempEl=document.getElementById('temp');
const humEl=document.getElementById('humid');
const glEl=document.getElementById('gasLevel');
const abEl=document.getElementById('alarmBadge');
const inf=document.getElementById('info');
async function fetchState(){
try{
const r=await fetch('/api/state');const d=await r.json();
ipEl.textContent='http://'+d.ip;
tempEl.textContent=d.temperature!=null?d.temperature.toFixed(1)+'\u00B0C':'--.-°C';
humEl.textContent=d.humidity!=null?d.humidity.toFixed(1)+'%':'--.-%';
const lvl=d.gas_level|0;
glEl.textContent=lvl+'%';
if(lvl>=30){glEl.className='valor-gas alarm';abEl.textContent='\u26A0 VAZAMENTO';abEl.className='alarm-badge danger'}
else if(lvl>=15){glEl.className='valor-gas warn';abEl.textContent='Atencao';abEl.className='alarm-badge alert'}
else{glEl.className='valor-gas safe';abEl.textContent='Seguro';abEl.className='alarm-badge safe'}
let u=d.uptime_s|0,upt=Math.floor(u/86400)+'d '+Math.floor((u%86400)/3600)+'h '+Math.floor((u%3600)/60)+'m '+u%60+'s';
let ls=d.last_send_s;let lastSend=ls==null?'nunca':ls<60?ls+'s':ls<3600?Math.floor(ls/60)+'m':Math.floor(ls/3600)+'h';
inf.innerHTML='RSSI: '+d.rssi+'dBm | Up: '+upt+'<br>Ultima coleta: '+lastSend;
}catch{glEl.textContent='\u26A0';glEl.className='valor-gas alarm';inf.textContent='Erro de conex\u00E3o'}
}
fetchState();setInterval(fetchState,3000)
</script>
</body>
</html>
)rawliteral";
