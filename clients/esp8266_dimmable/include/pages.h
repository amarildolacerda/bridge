#pragma once

static const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Luz Dimerizavel</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#010102;color:#f7f8f8;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#0f1011;border-radius:12px;padding:24px;text-align:center;max-width:360px;width:90%;border:1px solid #23252a}
h1{font-size:1.3rem;color:#5e6ad2;margin-bottom:4px}
.ip-badge{background:#141516;color:#8a8f98;font-size:.75rem;padding:3px 10px;border-radius:12px;display:inline-block;margin-bottom:12px;font-family:monospace}
.level-bar{width:100%;height:40px;background:#141516;border-radius:8px;margin:16px 0;position:relative;overflow:hidden}
.level-fill{height:100%;background:linear-gradient(90deg,#5e6ad2,#8a8f98);border-radius:8px;transition:width .3s;display:flex;align-items:center;justify-content:center;font-size:1.5rem;font-weight:700;color:#010102}
.btn{width:100%;padding:14px;border:none;border-radius:8px;font-size:1.1rem;font-weight:700;cursor:pointer;margin-top:8px;transition:opacity .2s}
.btn:active{opacity:.8}
.btn-on{background:#27a644;color:#fff}
.btn-off{background:#e5484d;color:#fff}
.info{color:#62666d;font-size:.8rem;margin-top:16px;word-break:break-all}
</style>
</head>
<body>
<div class="card">
<h1>Luz Dimerizavel</h1>
<div class="ip-badge" id="ipBadge">http://--.---.---.---</div>
<div class="level-bar"><div class="level-fill" id="levelFill" style="width:0%">0%</div></div>
<button class="btn btn-on" id="btnOn" onclick="setLight(100)">LIGAR (100%)</button>
<button class="btn btn-off" id="btnOff" onclick="setLight(0)">DESLIGAR</button>
<div class="info" id="info"></div>
</div>
<script>
const ipEl=document.getElementById('ipBadge');
const fillEl=document.getElementById('levelFill');
const infoEl=document.getElementById('info');
let currentLevel=0;
async function fetchState(){
try{
const r=await fetch('/api/state');const d=await r.json();
ipEl.textContent='http://'+d.ip;
currentLevel=d.level||0;
fillEl.style.width=currentLevel+'%';
fillEl.textContent=currentLevel+'%';
let u=d.uptime_s|0,upt=Math.floor(u/86400)+'d '+Math.floor((u%86400)/3600)+'h '+Math.floor((u%3600)/60)+'m '+u%60+'s';
let ls=d.last_send_s;let lastSend=ls==null?'nunca':ls<60?ls+'s':ls<3600?Math.floor(ls/60)+'m':Math.floor(ls/3600)+'h';
infoEl.innerHTML='RSSI: '+d.rssi+'dBm | Up: '+upt+'<br>Ultima coleta: '+lastSend+'<br>Bridge: '+(d.bridge_connected?'conectado':'desconectado');
}catch{fillEl.textContent='ERRO';fillEl.style.background='#e5484d';infoEl.textContent='Erro de conexao'}
}
async function setLight(level){
try{
await fetch('/api/set',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({level})});
setTimeout(fetchState,500);
}catch(e){console.error(e)}
}
fetchState();setInterval(fetchState,3000);
</script>
</body>
</html>
)rawliteral";