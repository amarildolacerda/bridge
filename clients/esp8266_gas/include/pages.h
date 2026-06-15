#pragma once

static const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Detector de Gas</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#F4F7FC;color:#2C3E50;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#FFFFFF;border-radius:12px;padding:24px;text-align:center;max-width:360px;width:90%}
h1{font-size:1.3rem;color:#B2CEfE;margin-bottom:4px}
.ip-badge{background:#E8EDF5;color:#4A6A8F;font-size:.75rem;padding:3px 10px;border-radius:12px;display:inline-block;margin-bottom:12px;font-family:monospace}
.valor-gas{font-size:3rem;margin:.5rem 0;transition:color .3s}
.valor-gas.safe{color:#2E7D32}
.valor-gas.warn{color:#E65100}
.valor-gas.alarm{color:#C62828}
.label{color:#7A8BA3;font-size:.85rem;margin-bottom:4px}
.alarm-badge{display:inline-block;padding:6px 16px;border-radius:20px;font-size:.9rem;font-weight:700;margin-top:8px;transition:background .3s}
.alarm-badge.safe{background:#E8F5E9;color:#2E7D32}
.alarm-badge.alert{background:#FFF3E0;color:#E65100}
.alarm-badge.danger{background:#FFEBEE;color:#C62828}
.info{color:#8FA0B8;font-size:.8rem;margin-top:16px;word-break:break-all}
</style>
</head>
<body>
<div class="card">
<h1>Detector de Gas</h1>
<div class="ip-badge" id="ipBadge">http://--.---.---.---</div>
<div class="label">Nivel de Gas</div>
<div class="valor-gas" id="gasLevel">---%</div>
<div class="alarm-badge safe" id="alarmBadge">Seguro</div>
<div class="info" id="info"></div>
</div>
<script>
const ipEl=document.getElementById('ipBadge');
const glEl=document.getElementById('gasLevel');
const abEl=document.getElementById('alarmBadge');
const inf=document.getElementById('info');
async function fetchState(){
try{
const r=await fetch('/api/state');const d=await r.json();
ipEl.textContent='http://'+d.ip;
const lvl=d.gas_level|0;
glEl.textContent=lvl+'%';
if(lvl>=30){glEl.className='valor-gas alarm';abEl.textContent='\u26A0 VAZAMENTO';abEl.className='alarm-badge danger'}
else if(lvl>=15){glEl.className='valor-gas warn';abEl.textContent='Atencao';abEl.className='alarm-badge alert'}
else{glEl.className='valor-gas safe';abEl.textContent='Seguro';abEl.className='alarm-badge safe'}
let u=d.uptime_s|0,upt=Math.floor(u/86400)+'d '+Math.floor((u%86400)/3600)+'h '+Math.floor((u%3600)/60)+'m '+u%60+'s';
inf.textContent='RSSI: '+d.rssi+'dBm | Up: '+upt;
}catch{glEl.textContent='\u26A0';glEl.className='valor-gas alarm';inf.textContent='Erro de conex\u00E3o'}
}
fetchState();setInterval(fetchState,3000)
</script>
</body>
</html>
)rawliteral";
