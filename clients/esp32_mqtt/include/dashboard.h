#pragma once

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>)rawliteral" DASHBOARD_TITLE R"rawliteral(</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#010102;color:#f7f8f8;padding:20px;max-width:900px;margin:0 auto}
h1{font-size:1.5rem;margin-bottom:4px;color:#5e6ad2}
.sub{color:#8a8f98;font-size:.85rem;margin-bottom:20px}
.stats{display:flex;gap:12px;margin-bottom:20px}
.stat{background:#0f1011;border-radius:12px;padding:16px 24px;flex:1;text-align:center;border:1px solid #23252a}
.stat .num{font-size:2rem;font-weight:700;color:#f7f8f8}
.stat .label{font-size:.75rem;color:#8a8f98;text-transform:uppercase;letter-spacing:.5px}
.refresh{text-align:right;margin-bottom:8px;font-size:.8rem;color:#8a8f98}
table{width:100%;border-collapse:collapse;background:#0f1011;border-radius:12px;overflow:hidden;border:1px solid #23252a}
th{background:#141516;padding:12px 16px;text-align:left;font-size:.8rem;text-transform:uppercase;letter-spacing:.5px;color:#8a8f98}
td{padding:12px 16px;border-top:1px solid #23252a;font-size:.9rem}
td.id{color:#5e6ad2;font-weight:600;font-family:monospace}
td.type{color:#8a8f98}
td.name{color:#f7f8f8}
.badge{display:inline-block;padding:2px 10px;border-radius:99px;font-size:.75rem;font-weight:600}
.badge.on{background:rgba(39,166,68,0.15);color:#27a644}
.badge.off{background:rgba(229,72,77,0.15);color:#e5484d}
.empty{text-align:center;padding:40px 16px;color:#8a8f98}
</style>
</head>
<body>
<h1>📡 )rawliteral" DASHBOARD_TITLE R"rawliteral(</h1>
<div class="sub">Clientes MQTT vistos nos &uacute;ltimos 5 minutos</div>
<div class="stats">
<div class="stat"><div class="num" id="total">—</div><div class="label">Total</div></div>
<div class="stat"><div class="num" id="online">—</div><div class="label">Online</div></div>
<div class="stat"><div class="num" id="offline">—</div><div class="label">Offline</div></div>
<div class="stat"><div class="num" id="uptime">—</div><div class="label">Uptime</div></div>
</div>
<div class="refresh">Atualizado: <span id="ts">carregando...</span></div>
<table>
<thead><tr><th>ID</th><th>Tipo</th><th>Nome</th><th>Status</th><th>Visto</th></tr></thead>
<tbody id="tbody"></tbody>
</table>
<script>
function fmtUptime(s){let d=Math.floor(s/86400),h=Math.floor((s%86400)/3600),m=Math.floor((s%3600)/60);return(d?d+'d ':'')+h+'h '+m+'m '+s%60+'s'}
async function load(){
try{
let r=await fetch('/api/info');
let b=await r.json();
document.getElementById('uptime').textContent=fmtUptime(b.uptime_s|0);
}catch(e){}
try{
const r=await fetch('/api/devices');
const d=await r.json();
let h='',tot=d.length,on=d.filter(x=>x.online).length,off=tot-on;
document.getElementById('total').textContent=tot;
document.getElementById('online').textContent=on;
document.getElementById('offline').textContent=off;
if(tot===0){h='<tr><td colspan="5" class="empty">Nenhum cliente nos &uacute;ltimos 5 min</td></tr>';}
else{
d.forEach(x=>{
const ago=x.last_seen_sec<60?x.last_seen_sec+'s':Math.floor(x.last_seen_sec/60)+'m '+x.last_seen_sec%60+'s';
const st=x.online?'<span class="badge on">Online</span>':'<span class="badge off">Offline</span>';
h+='<tr><td class="id">'+x.id+'</td><td class="type">'+x.type+'</td><td class="name">'+x.name+'</td><td>'+st+'</td><td>'+ago+' atr&aacute;s</td></tr>';
});}
document.getElementById('tbody').innerHTML=h;
document.getElementById('ts').textContent=new Date().toLocaleTimeString();
}catch(e){document.getElementById('ts').textContent='Erro de conex\u00E3o'}
}
load();
setInterval(load,3000);
</script>
</body>
</html>
)rawliteral";
