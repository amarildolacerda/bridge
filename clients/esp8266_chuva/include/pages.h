#pragma once

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensor de Chuva</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f4f7fc;color:#2c3e50;min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:#fff;border-radius:12px;padding:24px;max-width:360px;width:90%;text-align:center;box-shadow:0 4px 20px rgba(0,0,0,.06)}
h1{font-size:1.3rem;margin-bottom:4px;color:#b2cefe;font-weight:700}
.sub{color:#7a8ba3;font-size:.85rem;margin-bottom:20px}
.level{font-size:4rem;font-weight:700;margin:12px 0}
.level.dry{color:#4caf50}
.level.light{color:#ff9800}
.level.wet{color:#1976d2}
.level.heavy{color:#c62828}
.badge{display:inline-block;padding:4px 14px;border-radius:999px;font-size:.78rem;font-weight:700;text-transform:uppercase;letter-spacing:.04em;margin-bottom:12px}
.badge.dry{background:rgba(76,175,80,.12);color:#2e7d32}
.badge.light{background:rgba(255,152,0,.12);color:#e65100}
.badge.wet{background:rgba(25,118,210,.12);color:#1565c0}
.badge.heavy{background:rgba(198,40,40,.12);color:#c62828}
.dig{font-size:1rem;font-weight:600;padding:6px 14px;border-radius:999px;margin-bottom:16px;display:inline-flex;align-items:center;gap:6px}
.dig.wet{background:rgba(25,118,210,.12);color:#1565c0}
.dig.dry{background:rgba(76,175,80,.12);color:#2e7d32}
.meta{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px}
.meta-item{background:#f9fbff;border-radius:10px;padding:12px;border:1px solid #e6edf7}
.meta-label{font-size:.75rem;color:#7a8ba3;text-transform:uppercase;letter-spacing:.04em}
.meta-value{font-size:1rem;font-weight:600;margin-top:4px;color:#2c3e50}
.footer{margin-top:20px;color:#8fa0b8;font-size:.8rem}
</style>
</head>
<body>
<div class="card">
<h1>Sensor de Chuva</h1>
<div class="sub" id="sub">carregando...</div>
<div id="status">
<div class="badge" id="badge">—</div>
<div class="level" id="level">—</div>
<div class="dig" id="digital">—</div>
</div>
<div class="meta">
<div class="meta-item"><div class="meta-label">Device</div><div class="meta-value" id="dev-id">—</div></div>
<div class="meta-item"><div class="meta-label">Bridge</div><div class="meta-value" id="bridge-status">—</div></div>
<div class="meta-item"><div class="meta-label">RSSI</div><div class="meta-value" id="rssi">—</div></div>
<div class="meta-item"><div class="meta-label">Uptime</div><div class="meta-value" id="uptime">—</div></div>
</div>
<div class="footer" id="footer"></div>
</div>
<script>
async function load(){
try{
let r=await fetch('/api/state');
let d=await r.json();
let lvl=d.rain_level||0;
let el=document.getElementById('level');
el.textContent=lvl+'%';
el.className='level';
let badge=document.getElementById('badge');
if(lvl<=10){el.classList.add('dry');badge.textContent='SECO';badge.className='badge dry';}
else if(lvl<=40){el.classList.add('light');badge.textContent='CHUVISCO';badge.className='badge light';}
else if(lvl<=70){el.classList.add('wet');badge.textContent='CHUVENDO';badge.className='badge wet';}
else{badge.textContent='CHUVA FORTE';badge.className='badge heavy';el.classList.add('heavy');}
 let dig=document.getElementById('digital');
 if(d.rain_digital){dig.innerHTML='\u2614 CHUVA';dig.className='dig wet';}
 else{dig.innerHTML='\u2600\ufe0f SECO';dig.className='dig dry';}
 document.getElementById('dev-id').textContent=d.device_id||'—';
document.getElementById('bridge-status').innerHTML=d.bridge_connected?'<a href="http://'+d.bridge_ip+':'+d.bridge_port+'" target="_blank" style="color:#3498db;text-decoration:none">conectado</a>':'desconectado';
document.getElementById('rssi').textContent=d.rssi+' dBm';
let up=d.uptime_s||0;
document.getElementById('uptime').textContent=Math.floor(up/3600)+'h'+Math.floor((up%3600)/60)+'m';
document.getElementById('sub').textContent='IP: '+(d.ip||'—');
 let ls=d.last_send_s;let lastSend=ls==null?'nunca':ls<60?ls+'s':ls<3600?Math.floor(ls/60)+'m':Math.floor(ls/3600)+'h';
 document.getElementById('footer').innerHTML=lastSend?'Ultima coleta: '+lastSend:'';
}catch(e){document.getElementById('level').textContent='ERRO';}
}
load();setInterval(load,5000);
</script>
</body>
</html>
)=====";