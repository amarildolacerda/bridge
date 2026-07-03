#ifndef PAGES_H
#define PAGES_H

const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP-NOW Gateway</title>
<style>
:root { --bg:#0b0f1a; --card:#111827; --border:#1f2937; --text:#e5e7eb; --muted:#9ca3af; --primary:#22d3ee; --success:#22c55e; --danger:#ef4444; --warn:#f59e0b; }
* { box-sizing:border-box; margin:0; padding:0; }
body { font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif; background:var(--bg); color:var(--text); min-height:100vh; }
.container { max-width:900px; margin:0 auto; padding:20px; }
.header { display:flex; justify-content:space-between; align-items:center; margin-bottom:24px; flex-wrap:wrap; gap:16px; }
h1 { font-size:1.5rem; font-weight:600; }
.badge { display:inline-block; padding:4px 10px; border-radius:9999px; font-size:0.75rem; font-weight:600; }
.badge-online { background:#064e3b; color:#6ee7b7; }
.badge-offline { background:#450a0a; color:#fca5a5; }
.badge-pairing { background:#451a03; color:#fcd34d; }
.card { background:var(--card); border:1px solid var(--border); border-radius:12px; padding:20px; margin-bottom:16px; }
.card h2 { font-size:1rem; font-weight:600; margin-bottom:16px; display:flex; align-items:center; gap:8px; }
.grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(280px,1fr)); gap:16px; }
.sensor { background:#0c1222; border:1px solid var(--border); border-radius:8px; padding:16px; transition:border-color .2s; }
.sensor:hover { border-color:var(--primary); }
.sensor-header { display:flex; justify-content:space-between; align-items:flex-start; margin-bottom:12px; }
.sensor-name { font-weight:600; font-size:0.95rem; }
.sensor-type { font-size:0.7rem; color:var(--muted); text-transform:uppercase; letter-spacing:.05em; }
.sensor-meta { display:grid; grid-template-columns:repeat(2,1fr); gap:8px; font-size:0.8rem; }
.sensor-meta div { display:flex; flex-direction:column; }
.sensor-meta .label { color:var(--muted); font-size:0.65rem; }
.sensor-meta .value { font-weight:500; }
.sensor-state { margin-top:12px; padding-top:12px; border-top:1px solid var(--border); display:flex; flex-wrap:wrap; gap:8px; }
.state-item { background:#0b0f1a; padding:4px 10px; border-radius:6px; font-size:0.75rem; }
.state-temp { color:#f87171; }
.state-hum { color:#60a5fa; }
.state-contact { color:#a78bfa; }
.state-motion { color:#fbbf24; }
.state-gas { color:#f87171; }
.state-rain { color:#60a5fa; }
.state-tank { color:#4ade80; }
.state-battery { color:#fcd34d; }
.btn { padding:10px 18px; border:none; border-radius:8px; font-weight:600; cursor:pointer; transition:all .2s; font-size:0.85rem; }
.btn-primary { background:var(--primary); color:#0b0f1a; }
.btn-primary:hover { filter:brightness(1.1); }
.btn-danger { background:var(--danger); color:white; }
.btn-danger:hover { filter:brightness(1.1); }
.btn-secondary { background:#374151; color:var(--text); }
.btn-secondary:hover { background:#4b5563; }
.btn:disabled { opacity:0.5; cursor:not-allowed; }
.btn-group { display:flex; gap:8px; flex-wrap:wrap; margin-top:16px; }
.stats { display:grid; grid-template-columns:repeat(4,1fr); gap:12px; margin-bottom:24px; }
.stat { background:var(--card); border:1px solid var(--border); border-radius:10px; padding:16px; text-align:center; }
.stat-value { font-size:1.5rem; font-weight:700; color:var(--primary); }
.stat-label { font-size:0.7rem; color:var(--muted); text-transform:uppercase; letter-spacing:.05em; margin-top:4px; }
.toast { position:fixed; bottom:20px; right:20px; padding:12px 20px; border-radius:8px; background:#1f2937; color:white; box-shadow:0 10px 25px rgba(0,0,0,.3); z-index:1000; display:none; }
.toast.show { display:block; animation:slideIn .3s; }
@keyframes slideIn { from { opacity:0; transform:translateY(20px); } to { opacity:1; transform:translateY(0); } }
.empty { text-align:center; color:var(--muted); padding:40px 20px; }
.btn-pairing { background:var(--warn); color:#000; animation:pulse 1.5s infinite; }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.6} }
.modal { position:fixed; inset:0; background:rgba(0,0,0,.7); display:none; align-items:center; justify-content:center; z-index:100; }
.modal.show { display:flex; }
.modal-content { background:var(--card); border:1px solid var(--border); border-radius:12px; padding:24px; width:90%; max-width:400px; }
.modal h3 { margin-bottom:16px; }
.form-group { margin-bottom:16px; }
.form-group label { display:block; margin-bottom:6px; font-size:0.85rem; }
.form-group input { width:100%; padding:10px 12px; border:1px solid var(--border); border-radius:8px; background:#0b0f1a; color:var(--text); }
@media (max-width:600px) {
    .stats { grid-template-columns:repeat(2,1fr); }
    .sensor-meta { grid-template-columns:1fr; }
}
</style>
</head>
<body>
<div class="container">
    <div class="header">
        <h1>ESP-NOW Gateway</h1>
        <div class="btn-group">
            <button class="btn btn-primary" id="btn-pair" onclick="enterPairingMode()">+ Adicionar Sensor</button>
            <button class="btn btn-secondary" onclick="broadcastReregister()">Re-registrar Tudo</button>
            <button class="btn btn-secondary" onclick="loadData()">Atualizar</button>
        </div>
    </div>

    <div class="stats">
        <div class="stat"><div class="stat-value" id="stat-paired">0</div><div class="stat-label">Pareados</div></div>
        <div class="stat"><div class="stat-value" id="stat-online">0</div><div class="stat-label">Online</div></div>
        <div class="stat"><div class="stat-value" id="stat-rx">0</div><div class="stat-label">RX Total</div></div>
        <div class="stat"><div class="stat-value" id="stat-uptime">0s</div><div class="stat-label">Uptime</div></div>
    </div>

    <div class="card">
        <h2>Sensores Virtuais</h2>
        <div id="sensors-grid" class="grid"></div>
    </div>
</div>

<div class="modal" id="name-modal">
    <div class="modal-content">
        <h3>Nomear Sensor</h3>
        <div class="form-group">
            <label>Nome do sensor (slot <span id="name-slot">0</span>)</label>
            <input type="text" id="name-input" placeholder="Ex: Sala, Cozinha, Portão..." maxlength="32">
        </div>
        <div class="form-group">
            <label>Tipo</label>
            <input type="text" id="name-type" readonly>
        </div>
        <div class="btn-group">
            <button class="btn btn-primary" onclick="confirmName()">Salvar</button>
            <button class="btn btn-secondary" onclick="closeNameModal()">Cancelar</button>
        </div></div>
</div>

<div class="toast" id="toast"></div>

<script>
let pairingTimer = null;
let namingSlot = -1;
let s_pairingWindowSec = 180;

async function api(path, opts={}) {
    const res = await fetch(path, {headers:{'Content-Type':'application/json'}, ...opts});
    if (!res.ok) throw new Error(await res.text());
    return res.json();
}

function showToast(msg, err=false) {
    const t = document.getElementById('toast');
    t.textContent = msg;
    t.style.background = err ? '#dc2626' : '#16a34a';
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 3000);
}

function fmtUptime(ms) {
    const s = Math.floor(ms/1000);
    if (s < 60) return s+'s';
    if (s < 3600) return Math.floor(s/60)+'m '+s%60+'s';
    if (s < 86400) return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';
    return Math.floor(s/86400)+'d '+Math.floor((s%86400)/3600)+'h';
}

function typeName(type) {
    const names = {1:'Temp+Hum', 2:'Contato', 3:'Movimento', 4:'Gás', 5:'Chuva', 6:'Tanque', 7:'DHT+Gas'};
    return names[type] || 'Desconhecido';
}

function renderSensors(sensors) {
    const grid = document.getElementById('sensors-grid');
    if (!sensors.length) {
        grid.innerHTML = '<div class="empty">Nenhum sensor pareado. Clique em "Adicionar Sensor".</div>';
        return;
    }
    grid.innerHTML = sensors.map(s => `
        <div class="sensor">
            <div class="sensor-header">
                <div>
                    <div class="sensor-name">${s.name || 'Sem nome'}</div>
                    <span class="sensor-type">${typeName(s.type)} • Slot ${s.slot}</span>
                </div>
                <span class="badge ${s.online ? 'badge-online' : 'badge-offline'}">${s.online ? 'Online' : 'Offline'}</span>
            </div>
            <div class="sensor-meta">
                <div><span class="label">MAC</span><span class="value">${s.mac_str}</span></div>
                <div><span class="label">Bateria</span><span class="value state-battery">${s.battery_pct}%</span></div>
                <div><span class="label">RSSI</span><span class="value">${s.last_rssi} dBm</span></div>
                <div><span class="label">Último</span><span class="value">${s.last_seen >= 0 ? fmtUptime(s.last_seen) : '—'}</span></div>
                <div><span class="label">Seq</span><span class="value">${s.sequence}</span></div>
                <div><span class="label">Bridge ID</span><span class="value" style="font-size:0.65rem">${s.bridge_device_id || '—'}</span></div>
                ${s.ip ? `<div><span class="label">IP</span><span class="value"><a href="http://${s.ip}">${s.ip}</a></span></div>` : ''}
            </div>
            <div class="sensor-state" id="state-${s.slot}">${renderState(s)}</div>
            <div class="btn-group">
                <button class="btn btn-secondary" onclick="renameSensor(${s.slot})">Renomear</button>
                <button class="btn btn-danger" onclick="removeSensor(${s.slot})">Remover</button>
            </div>
        </div>
    `).join('');
}

function renderState(s) {
    const st = s.state || {};
    let html = '';
    if (s.type === 1) {
        html += `<span class="state-item state-temp">🌡 ${(st.temperature||0).toFixed(1)}°C</span>`;
        html += `<span class="state-item state-hum">💧 ${(st.humidity||0).toFixed(0)}%</span>`;
    } else if (s.type === 2) {
        html += `<span class="state-item state-contact">${st.contact ? '🚪 ABERTO' : '🔒 FECHADO'}</span>`;
        if (st.tamper) html += `<span class="state-item state-contact">⚠️ VIOLAÇÃO</span>`;
    } else if (s.type === 3) {
        html += `<span class="state-item state-motion">${st.occupancy ? '🏃 MOVIMENTO' : '😴 LIVRE'}</span>`;
    } else if (s.type === 4) {
        html += `<span class="state-item state-gas">⛽ ${st.gas_level||0} ppm</span>`;
        if (st.alarm) html += `<span class="state-item state-gas">🚨 ALARME</span>`;
    } else if (s.type === 5) {
        html += `<span class="state-item state-rain">🌧 ${st.rain_level||0}%</span>`;
        html += `<span class="state-item state-rain">${st.rain_digital ? '☔ Chuva' : '☀️ Seco'}</span>`;
    } else if (s.type === 6) {
        html += `<span class="state-item state-tank">🛢 ${st.level_pct||0}%</span>`;
        html += `<span class="state-item state-tank">${st.distance_cm||0} cm</span>`;
    } else if (s.type === 7) {
        html += `<span class="state-item state-temp">🌡 ${(st.temperature||0).toFixed(1)}°C</span>`;
        html += `<span class="state-item state-hum">💧 ${(st.humidity||0).toFixed(0)}%</span>`;
        html += `<span class="state-item state-gas">⛽ ${st.gas_level||0}%</span>`;
        if (st.alarm) html += `<span class="state-item state-gas">🚨 ALARME</span>`;
    }
    if (s.battery_pct !== undefined) {
        html += `<span class="state-item state-battery">🔋 ${s.battery_pct}%</span>`;
    }
    return html || '<span class="state-item" style="color:var(--muted)">Aguardando dados...</span>';
}

async function loadData() {
    try {
        const [info, sensors] = await Promise.all([api('/api/info'), api('/api/sensors')]);
        document.getElementById('stat-paired').textContent = info.paired_count;
        document.getElementById('stat-online').textContent = info.online_count;
        document.getElementById('stat-rx').textContent = info.rx_total;
        document.getElementById('stat-uptime').textContent = fmtUptime(info.uptime_ms);
        if (info.pairing_window_sec) s_pairingWindowSec = info.pairing_window_sec;
        renderSensors(sensors.map(s => ({
            ...s,
            mac_str: s.mac
        })));
    } catch (e) {
        showToast('Erro ao carregar: '+e.message, true);
    }
}

function startPairingCountdown() {
    const btn = document.getElementById('btn-pair');
    btn.classList.add('btn-pairing');
    let remaining = s_pairingWindowSec;
    btn.textContent = '✕ Pareando (' + remaining + 's)';
    pairingTimer = setInterval(() => {
        remaining--;
        if (remaining > 0) {
            btn.textContent = '✕ Pareando (' + remaining + 's)';
        } else {
            exitPairingMode();
        }
    }, 1000);
}

async function enterPairingMode() {
    if (pairingTimer) { exitPairingMode(); return; }
    try {
        await api('/api/pair/start', {method:'POST'});
        startPairingCountdown();
    } catch (e) {
        if (e.message.includes('already pairing')) {
            startPairingCountdown();
        } else {
            showToast('Erro: '+e.message, true);
        }
    }
}

async function exitPairingMode() {
    if (pairingTimer) clearInterval(pairingTimer);
    pairingTimer = null;
    try { await api('/api/pair/stop', {method:'POST'}); } catch(e) {}
    const btn = document.getElementById('btn-pair');
    btn.classList.remove('btn-pairing');
    btn.textContent = '+ Adicionar Sensor';
}

async function broadcastReregister() {
    try {
        await api('/api/broadcast', {method:'POST'});
        showToast('Re-registro enviado para Bridge');
    } catch (e) { showToast('Erro: '+e.message, true); }
}

async function renameSensor(slot) {
    try {
        const sensors = await api('/api/sensors');
        const s = sensors.find(x => x.slot === slot);
        if (!s) return;
        namingSlot = slot;
        document.getElementById('name-slot').textContent = slot;
        document.getElementById('name-input').value = s.name || '';
        document.getElementById('name-type').value = typeName(s.type);
        document.getElementById('name-modal').classList.add('show');
    } catch (e) { showToast('Erro: '+e.message, true); }
}

async function confirmName() {
    const name = document.getElementById('name-input').value.trim();
    if (!name) { showToast('Nome obrigatório', true); return; }
    try {
        await api('/api/sensor/'+namingSlot+'/name', {method:'POST', body:JSON.stringify({name})});
        showToast('Nome atualizado');
        closeNameModal();
        loadData();
    } catch (e) { showToast('Erro: '+e.message, true); }
}

function closeNameModal() {
    document.getElementById('name-modal').classList.remove('show');
    namingSlot = -1;
}

async function removeSensor(slot) {
    if (!confirm('Remover sensor do slot ' + slot + '?')) return;
    try {
        await api('/api/sensor/'+slot+'/remove', {method:'POST'});
        showToast('Sensor removido');
        loadData();
    } catch (e) { showToast('Erro: '+e.message, true); }
}

loadData();
setInterval(loadData, 10000);


document.getElementById('name-modal').addEventListener('click', e => { if(e.target.id==='name-modal') closeNameModal(); });
</script>
</body>
</html>
)rawliteral";

#endif