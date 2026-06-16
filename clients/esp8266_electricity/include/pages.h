#pragma once

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Medidor de Energia</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f4f7fc;color:#2c3e50;min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:#fff;border-radius:12px;padding:24px;max-width:360px;width:90%;text-align:center;box-shadow:0 4px 20px rgba(0,0,0,.06)}
h1{font-size:1.3rem;margin-bottom:4px;color:#b2cefe;font-weight:700}
.sub{color:#7a8ba3;font-size:.85rem;margin-bottom:20px}
.valor{font-size:3.5rem;font-weight:700;margin:8px 0;color:#2c3e50}
.valor .unit{font-size:1.2rem;color:#7a8ba3;font-weight:400}
.label{color:#7a8ba3;font-size:.85rem;margin-bottom:16px}
.badge{display:inline-block;padding:6px 16px;border-radius:999px;font-size:.82rem;font-weight:700;margin-bottom:16px}
.badge.low{background:rgba(46,125,50,.12);color:#2e7d32}
.badge.med{background:rgba(255,152,0,.12);color:#e65100}
.badge.high{background:rgba(198,40,40,.12);color:#c62828}
.meta{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px}
.meta-item{background:#f9fbff;border-radius:10px;padding:12px;border:1px solid #e6edf7}
.meta-label{font-size:.75rem;color:#7a8ba3;text-transform:uppercase;letter-spacing:.04em}
.meta-value{font-size:1rem;font-weight:600;margin-top:4px;color:#2c3e50}
.meta-value .small{font-size:.75rem;color:#7a8ba3;font-weight:400}
.footer{margin-top:20px;color:#8fa0b8;font-size:.8rem}
</style>
</head>
<body>
<div class="card">
<h1>Medidor de Energia</h1>
<div class="sub" id="sub">carregando...</div>
<div class="label">Corrente AC</div>
<div class="valor" id="current"><span class="unit">--- mA</span></div>
<div class="badge" id="badge">—</div>
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
let ma=d.current_ma||0;
let el=document.getElementById('current');
el.innerHTML=ma+' <span class="unit">mA</span>';
let badge=document.getElementById('badge');
if(ma<200){badge.textContent='BAIXO';badge.className='badge low';}
else if(ma<1000){badge.textContent='MEDIO';badge.className='badge med';}
else{badge.textContent='ALTO';badge.className='badge high';}
document.getElementById('dev-id').textContent=d.device_id||'—';
let bs=document.getElementById('bridge-status');
if(d.bridge_connected)bs.innerHTML='<a href="http://'+d.bridge_ip+':'+d.bridge_port+'" target="_blank" style="color:#3498db;text-decoration:none">conectado</a>';
else bs.textContent='desconectado';
document.getElementById('rssi').textContent=d.rssi+' dBm';
let up=d.uptime_s||0;
document.getElementById('uptime').textContent=Math.floor(up/3600)+'h'+Math.floor((up%3600)/60)+'m';
document.getElementById('sub').textContent='IP: '+(d.ip||'—');
 let ls=d.last_send_s;let lastSend=ls==null?'nunca':ls<60?ls+'s':ls<3600?Math.floor(ls/60)+'m':Math.floor(ls/3600)+'h';
 document.getElementById('footer').innerHTML=lastSend?'Ultima coleta: '+lastSend:'';
}catch(e){document.getElementById('current').innerHTML='<span class="unit">ERRO</span>';}
}
load();setInterval(load,5000);
</script>
</body>
</html>
)=====";
