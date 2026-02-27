// data/app.js - Makita BMS Tool UI Logic

const MAKITA_MODELS = {
  "BL1815": { cap: "1.5Ah", cells: "5x 18650", config: "5S1P", v_nom: "18V" },
  "BL1815N": { cap: "1.5Ah", cells: "5x 18650", config: "5S1P", v_nom: "18V" },
  "BL1820": { cap: "2.0Ah", cells: "5x 18650", config: "5S1P", v_nom: "18V" },
  "BL1830": { cap: "3.0Ah", cells: "10x 18650", config: "5S2P", v_nom: "18V" },
  "BL1840": { cap: "4.0Ah", cells: "10x 18650", config: "5S2P", v_nom: "18V" },
  "BL1850": { cap: "5.0Ah", cells: "10x 18650", config: "5S2P", v_nom: "18V" },
  "BL1850B": { cap: "5.0Ah", cells: "10x 18650", config: "5S2P", v_nom: "18V" },
  "BL1860B": { cap: "6.0Ah", cells: "10x 18650", config: "5S2P", v_nom: "18V" },
  "BL1415": { cap: "1.5Ah", cells: "4x 18650", config: "4S1P", v_nom: "14.4V" },
  "BL1430": { cap: "3.0Ah", cells: "8x 18650", config: "4S2P", v_nom: "14.4V" },
  "BL1440": { cap: "4.0Ah", cells: "8x 18650", config: "4S2P", v_nom: "14.4V" },
  "BL1450": { cap: "5.0Ah", cells: "8x 18650", config: "4S2P", v_nom: "14.4V" },
  "BL1460": { cap: "6.0Ah", cells: "8x 18650", config: "4S2P", v_nom: "14.4V" }
};

const TRANSLATIONS = {
  es: {
    subtitle: "Diagnostico de Bateria",
    sectionTitle: "Operaciones",
    section_overview: "Estado del Paquete",
    rawTitle: "Consola de Sistema",
    footerText: "v1.2",
    btn_read: "Leer Info",
    btn_dynamic: "Leer Voltajes",
    btn_clear_err: "Resetear Errores",
    btn_led_test: "Test LED",
    msg_wait: "Por favor, espere...",
    cell: "Celda",
    status_connecting: "Conectando...",
    status_online: "En linea",
    status_offline: "Desconectado",
    locked: "BLOQUEADA",
    unlocked: "DESBLOQUEADA",
    model: "Modelo",
    cycles: "Ciclos de carga",
    state: "Estado",
    mfg_date: "Fecha fabricacion",
    capacity: "Capacidad",
    rom_id: "ID ROM",
    sum_total: "Voltaje Total",
    sum_diff: "Diferencia",
    log_waiting: "Esperando conexion...",
    log_req_static: "Solicitando datos maestros...",
    log_req_dynamic: "Actualizando voltajes...",
    log_clear_confirm: "Borrar errores del BMS?",
    log_ws_error: "Error WebSocket.",
    log_evt_config: "Configurando eventos...",
    lbl_auto: "Lectura Real-time",
    lbl_temp: "Temperaturas",
    lbl_imbalance: "Desbalanceo (HUD)",
    lbl_report: "Generar informe",
    health_good: "Excelente",
    health_fair: "Regular",
    health_poor: "Degradada",
    health_dead: "Agotada",
    fatigue_low: "Baja",
    fatigue_med: "Media",
    fatigue_high: "Alta",
    report_title: "INFORME TECNICO DE BATERIA",
    lbl_balance_title: "Asistente de Balanceo",
    lbl_model_compare: "Comparativa de Modelo",
    lbl_nominal_cap: "Cap. Nominal:",
    lbl_config: "Config:",
    lbl_cells: "Celdas:",
    lbl_current_state: "Estado actual:",
    lbl_imbalance_detected: "Desviacion detectada:",
    lbl_action_charge: "Cargar",
    lbl_action_discharge: "Descargar",
    lbl_soh: "SOH (Salud)",
    lbl_history: "Historial de Voltaje",
    lbl_system_title: "Sistema & FW Update",
    lbl_wifi_config: "WiFi (Modo Estacion)",
    lbl_wifi_ssid: "Nombre de red (SSID)",
    lbl_wifi_pass: "Contrasena",
    btn_save_wifi: "Conectar",
    lbl_wifi_hint: "El ESP32 se reiniciara. El AP Makita se mantiene.",
    msg_presence_detected: " (Bateria detectada)",
    msg_presence_empty: " (Bus vacio)",
    lbl_total_voltage: "Voltaje Pack",
    bal_ok: "Equilibrado",
    bal_warn: "Desviacion",
    bal_crit: "Critico",
    ota_title: "Actualizacion de Firmware",
    ota_desc: "Sube un archivo .bin para actualizar.",
    btn_ota_select: "Seleccionar Archivo",
    ota_msg_uploading: "Subiendo..."
  },
  en: {
    subtitle: "Battery Diagnostics",
    sectionTitle: "Operations",
    section_overview: "Pack Overview",
    rawTitle: "System Console",
    footerText: "v1.2",
    btn_read: "Read Info",
    btn_dynamic: "Read Voltages",
    btn_clear_err: "Reset Errors",
    btn_led_test: "Test LED",
    msg_wait: "Please wait...",
    cell: "Cell",
    status_connecting: "Connecting...",
    status_online: "Online",
    status_offline: "Offline",
    locked: "LOCKED",
    unlocked: "UNLOCKED",
    model: "Model",
    cycles: "Charge cycles",
    state: "Status",
    mfg_date: "Mfg Date",
    capacity: "Capacity",
    rom_id: "ROM ID",
    sum_total: "Total Voltage",
    sum_diff: "Difference",
    log_waiting: "Waiting for connection...",
    log_req_static: "Requesting master data...",
    log_req_dynamic: "Updating voltages...",
    log_clear_confirm: "Clear BMS errors?",
    log_ws_error: "WebSocket error.",
    log_evt_config: "Setting up events...",
    lbl_auto: "Real-time Updates",
    lbl_temp: "Temperature",
    lbl_imbalance: "Cell Imbalance (HUD)",
    lbl_report: "Export Report",
    health_good: "Excellent",
    health_fair: "Fair",
    health_poor: "Degraded",
    health_dead: "Worn Out",
    fatigue_low: "Low",
    fatigue_med: "Medium",
    fatigue_high: "High",
    report_title: "BATTERY TECHNICAL REPORT",
    lbl_balance_title: "Balancing Assistant",
    lbl_model_compare: "Model Comparison",
    lbl_nominal_cap: "Nominal Cap:",
    lbl_config: "Config:",
    lbl_cells: "Cells:",
    lbl_current_state: "Current state:",
    lbl_imbalance_detected: "Imbalance detected:",
    lbl_action_charge: "Charge",
    lbl_action_discharge: "Discharge",
    lbl_soh: "SOH (Health)",
    lbl_history: "Voltage History",
    lbl_system_title: "System & FW Update",
    lbl_wifi_config: "WiFi Station Mode",
    lbl_wifi_ssid: "Network Name (SSID)",
    lbl_wifi_pass: "Password",
    btn_save_wifi: "Connect",
    lbl_wifi_hint: "ESP32 will restart. AP stays as backup.",
    msg_presence_detected: " (Battery detected)",
    msg_presence_empty: " (Empty Bus)",
    lbl_total_voltage: "Pack Voltage",
    bal_ok: "Balanced",
    bal_warn: "Imbalanced",
    bal_crit: "Critical",
    ota_title: "Firmware Update",
    ota_desc: "Upload a .bin file to update.",
    btn_ota_select: "Select File",
    ota_msg_uploading: "Uploading..."
  }
};

// Globals
let currentLang = localStorage.getItem('makita_lang') || 'en';
let socket;
let isConnected = false;
let lastData = null;
let lastPresence = false;
let pollInterval = null;
let historyChart = null;
const MAX_HISTORY = 40;
let historyData = {
  labels: [],
  datasets: [1, 2, 3, 4, 5].map(i => ({
    label: `Cell ${i}`,
    data: [],
    borderColor: `hsl(${i * 60}, 70%, 50%)`,
    backgroundColor: `hsla(${i * 60}, 70%, 50%, 0.1)`,
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.3,
    fill: false
  }))
};

function updateHistoryDatasets(cellCount) {
  if (historyData.datasets.length === cellCount) return;
  if (cellCount < historyData.datasets.length) {
    historyData.datasets = historyData.datasets.slice(0, cellCount);
  } else {
    for (let i = historyData.datasets.length; i < cellCount; i++) {
      historyData.datasets.push({
        label: `${t('cell')} ${i + 1}`,
        data: [],
        borderColor: `hsl(${i * 60}, 70%, 50%)`,
        backgroundColor: `hsla(${i * 60}, 70%, 50%, 0.1)`,
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.3,
        fill: false
      });
    }
  }
  if (historyChart) {
    historyChart.data.datasets = historyData.datasets;
    historyChart.update();
  }
}

const el = id => document.getElementById(id);
const t = key => (TRANSLATIONS[currentLang] && TRANSLATIONS[currentLang][key]) || key;
const savePref = (k, v) => localStorage.setItem(`makita_${k}`, v);

// ── Init ──
document.addEventListener('DOMContentLoaded', () => {
  initTheme();
  initLang();
  setupEventListeners();
  setupMobileMenu();
  connect();

  if (localStorage.getItem('makita_auto') === 'true') {
    const bAuto = el('checkAuto');
    if (bAuto) bAuto.checked = true;
  }

  initChart();
});

// ── Theme ──
function initTheme() {
  const isDark = localStorage.getItem('makita_theme') === 'dark';
  setTheme(isDark ? 'dark' : 'light');
}

function setTheme(theme, skipSync = false) {
  const isDark = (theme === 'dark');
  document.body.classList.toggle('dark-mode', isDark);
  savePref('theme', theme);

  const bLight = el('themeLight');
  const bDark = el('themeDark');
  if (bLight) bLight.classList.toggle('active', !isDark);
  if (bDark) bDark.classList.toggle('active', isDark);

  if (!skipSync && isConnected) {
    sendCommand('save_config', { lang: currentLang, theme: theme });
  }
}

// ── Language ──
function initLang() {
  if (!localStorage.getItem('makita_lang')) {
    const navLang = navigator.language.split('-')[0];
    currentLang = (navLang === 'es') ? 'es' : 'en';
  }
  applyTranslations();
}

function applyTranslations() {
  const data = TRANSLATIONS[currentLang];
  savePref('lang', currentLang);

  document.querySelectorAll('[data-i18n]').forEach(item => {
    const key = item.getAttribute('data-i18n');
    if (data[key]) item.textContent = data[key];
  });

  document.querySelectorAll('[data-i18n-hold]').forEach(item => {
    const key = item.getAttribute('data-i18n-hold');
    if (data[key]) item.placeholder = data[key];
  });

  // Update lang button active states
  const bEN = el('btnEN');
  const bES = el('btnES');
  if (bEN) bEN.classList.toggle('active', currentLang === 'en');
  if (bES) bES.classList.toggle('active', currentLang === 'es');

  refreshStatus();

  if (historyChart) {
    historyChart.data.datasets.forEach((ds, i) => {
      ds.label = `${t('cell')} ${i + 1}`;
    });
    historyChart.update('none');
  }

  if (lastData) {
    renderStaticTable(lastData);
    renderCells(lastData);
  }
}

// ── Status Display ──
function refreshStatus() {
  const statusText = el('statusText');
  const statusDot = el('statusDot');
  const mobileDot = el('mobileStatusDot');
  if (!statusText) return;

  const setDot = (cls) => {
    if (statusDot) { statusDot.className = 'status-dot ' + cls; }
    if (mobileDot) { mobileDot.className = 'mobile-status ' + cls; }
  };

  if (!isConnected) {
    statusText.textContent = t('status_offline');
    setDot('offline');
    return;
  }

  if (lastPresence) {
    statusText.textContent = t('status_online') + t('msg_presence_detected');
    setDot('online');
  } else {
    statusText.textContent = t('status_online') + t('msg_presence_empty');
    setDot('sim');
  }
}

// ── Mobile Menu ──
function setupMobileMenu() {
  const toggle = el('menuToggle');
  const sidebar = el('sidebar');
  if (!toggle || !sidebar) return;

  // Create overlay
  const overlay = document.createElement('div');
  overlay.className = 'sidebar-overlay';
  document.body.appendChild(overlay);

  toggle.addEventListener('click', () => {
    sidebar.classList.toggle('open');
    overlay.classList.toggle('active');
  });

  overlay.addEventListener('click', () => {
    sidebar.classList.remove('open');
    overlay.classList.remove('active');
  });

  // Close sidebar on nav button click (mobile)
  sidebar.querySelectorAll('.nav-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      if (window.innerWidth <= 900) {
        sidebar.classList.remove('open');
        overlay.classList.remove('active');
      }
    });
  });
}

// ── Chart ──
function initChart() {
  try {
    if (typeof Chart === 'undefined') return;
    const canvas = el('historyChart');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    historyChart = new Chart(ctx, {
      type: 'line',
      data: historyData,
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        scales: {
          x: { display: false },
          y: {
            min: 2.5, max: 4.3,
            ticks: { color: 'rgba(128,128,128,0.8)' },
            grid: { color: 'rgba(128,128,128,0.1)' }
          }
        },
        plugins: {
          legend: {
            display: true,
            labels: { boxWidth: 10, font: { size: 10 }, color: 'rgba(128,128,128,0.8)' }
          }
        }
      }
    });
  } catch (e) {
    log("Chart init error: " + e.message);
  }
}

function updateChart(cellVoltages) {
  if (!historyChart) return;
  const now = new Date().toLocaleTimeString();
  if (historyData.labels.length > MAX_HISTORY) {
    historyData.labels.shift();
    historyData.datasets.forEach(ds => ds.data.shift());
  }
  updateHistoryDatasets(cellVoltages.length);
  historyData.labels.push(now);
  cellVoltages.forEach((v, i) => {
    if (historyData.datasets[i]) historyData.datasets[i].data.push(v);
  });
  historyChart.update('none');
}

// ── WebSocket ──
let reconnectAttempts = 0;
const MAX_RECONNECT = 50;

function connect() {
  const statusText = el('statusText');
  if (statusText) statusText.textContent = t('status_connecting');

  try {
    socket = new WebSocket(`ws://${window.location.host}/ws`);

    socket.onopen = () => {
      isConnected = true;
      reconnectAttempts = 0;
      log("WebSocket connected.");
      refreshStatus();
      sendCommand('presence');
      sendCommand('get_config');
    };

    socket.onclose = () => {
      isConnected = false;
      refreshStatus();
      if (reconnectAttempts < MAX_RECONNECT) {
        const delay = Math.min(2000 + reconnectAttempts * 1000, 10000);
        reconnectAttempts++;
        log(`Connection lost. Reconnecting in ${delay/1000}s... (${reconnectAttempts}/${MAX_RECONNECT})`);
        setTimeout(() => { if (!isConnected) connect(); }, delay);
      } else {
        log("Max reconnect attempts reached.");
      }
    };

    socket.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      handleMessage(msg);
    };

    socket.onerror = () => { log(t('log_ws_error')); };
  } catch (e) {
    log("Connection error: " + e.message);
    setTimeout(() => connect(), 3000);
  }
}

// No simulation mode - always connect to real hardware

function handleMessage(msg) {
  if (msg.type === 'static_data') {
    lastData = msg.data;
    renderStaticTable(msg.data);
    renderCells(msg.data);
    if (msg.features) updateButtonStates(msg.features);
    el('overviewCard').classList.remove('hidden');
    if (msg.data.cell_voltages) updateChart(msg.data.cell_voltages);
  } else if (msg.type === 'dynamic_data') {
    if (lastData && msg.data) {
      // Reject obviously bad data before rendering
      if (msg.data.pack_voltage > 25 || (msg.data.cell_voltages && msg.data.cell_voltages.some(v => v > 5))) {
        log("[WARN] Rejected bad dynamic data (out of range)");
        return;
      }
      Object.assign(lastData, msg.data);
      renderCells(lastData);
      renderStaticTable(lastData);
      if (msg.data.cell_voltages) updateChart(msg.data.cell_voltages);
    }
  } else if (msg.type === 'presence') {
    updatePresence(msg.present);
  } else if (msg.type === 'success' || msg.type === 'error') {
    showNotification(msg.message, msg.type === 'success' ? 'success' : 'danger');
  } else if (msg.type === 'debug') {
    log(msg.message);
  } else if (msg.type === 'config') {
    if (msg.lang && msg.lang !== currentLang) {
      currentLang = msg.lang;
      applyTranslations();
    }
    if (msg.theme) setTheme(msg.theme, true);
  }
}

function sendCommand(cmd, params = {}) {
  if (!isConnected) return;
  socket.send(JSON.stringify({ command: cmd, ...params }));
}

// ── Render ──
function renderStaticTable(d) {
  const table = el('data-table');
  if (!table) return;

  const rows = [
    [t('model'), d.model],
    [t('cycles'), d.charge_cycles],
    [t('state'), d.lock_status === "LOCKED" ? `<span class="badge badge-danger">${t('locked')}</span>` : `<span class="badge badge-success">${t('unlocked')}</span>`],
    [t('capacity'), d.capacity],
    [t('mfg_date'), d.mfg_date],
    [t('lbl_total_voltage'), d.pack_voltage.toFixed(2) + " V"],
    [t('rom_id'), `<span style="font-family:monospace;font-size:11px">${d.rom_id}</span>`]
  ];

  table.innerHTML = rows.map(r => `
    <div class="kv-row">
      <span class="k">${r[0]}</span>
      <span class="v">${r[1]}</span>
    </div>`).join('');
}

function renderCells(d) {
  const area = el('cellsArea');
  if (!area) return;
  area.innerHTML = d.cell_voltages.map((v, i) => {
    const pct = Math.min(100, Math.max(0, (v - 2.5) / (4.2 - 2.5) * 100));
    let colorClass = 'bg-ok';
    if (v < 0.5) colorClass = 'dead-cell';
    else if (v < 3.0) colorClass = 'crit-low-animated';
    else if (v < 3.3) colorClass = 'bg-error';
    else if (v < 3.6) colorClass = 'bg-warning';

    return `
      <div class="cell-container">
        <div class="cell-gfx">
          <div class="cell-gfx-content">
            <span class="cell-pole">+</span>
            <span class="cell-gfx-vol">${v.toFixed(2)}V</span>
            <span class="cell-pole">-</span>
          </div>
          <div class="cell-level-bar ${colorClass}" style="height:${pct}%;"></div>
        </div>
        <div class="cell-number">${t('cell')} ${i + 1}</div>
      </div>`;
  }).join('');

  if (el('packSummary')) {
    el('packSummary').innerHTML = `${t('sum_total')}: <strong>${d.pack_voltage.toFixed(2)}V</strong> | ${t('sum_diff')}: <span style="color:${d.cell_diff > 0.1 ? 'red' : 'inherit'}">${d.cell_diff.toFixed(3)}V</span>`;
  }

  renderAdvancedDiagnostics(d);
}

function renderAdvancedDiagnostics(d) {
  let fatigueLevel = 'fatigue_low';
  if (d.charge_cycles > 150 || d.cell_diff > 0.15) fatigueLevel = 'fatigue_med';
  if (d.charge_cycles > 300 || d.cell_diff > 0.25) fatigueLevel = 'fatigue_high';

  let health = 100 - (d.charge_cycles / 10);
  health -= (d.cell_diff * 40);
  health = Math.max(0, Math.round(health));

  const ring = el('sohRing');
  const label = el('sohLabel');
  if (ring && label) {
    ring.textContent = health + '%';
    ring.className = 'soh-ring ' + (health > 80 ? 'good' : (health > 50 ? 'fair' : 'poor'));
    let hLabel = 'health_good';
    if (health <= 80) hLabel = 'health_fair';
    if (health <= 50) hLabel = 'health_poor';
    if (health <= 20) hLabel = 'health_dead';
    label.innerHTML = `${t(hLabel)}<br><small style="font-size:10px;opacity:0.6">${t(fatigueLevel)}</small>`;
  }

  renderModelComparison(d);
  renderBalancingAssistant(d);

  if (d.temp1 !== undefined && d.temp2 !== undefined) {
    const tValues = el('tempValues');
    if (tValues) tValues.textContent = `${d.temp1.toFixed(1)}\u00B0C | ${d.temp2.toFixed(1)}\u00B0C`;
    if (el('tempBar1')) el('tempBar1').style.height = Math.min(100, (d.temp1 / 60) * 100) + '%';
    if (el('tempBar2')) el('tempBar2').style.height = Math.min(100, (d.temp2 / 60) * 100) + '%';
  }

  renderImbalanceHUD(d);
  updateImbalanceBadge(d.cell_diff);
}

function renderModelComparison(d) {
  const container = el('modelComparison');
  const section = el('modelSection');
  if (!container || !d.model) return;

  const modelKey = d.model.split('/')[0].split(' ')[0].trim();
  const nominal = MAKITA_MODELS[modelKey];

  if (nominal) {
    section.classList.remove('hidden');
    container.innerHTML = `
      <div class="kv-row"><span>${t('lbl_nominal_cap')}</span><strong>${nominal.cap}</strong></div>
      <div class="kv-row"><span>${t('lbl_config')}</span><strong>${nominal.config}</strong></div>
      <div class="kv-row"><span>${t('lbl_cells')}</span><strong>${nominal.cells}</strong></div>
      <div class="kv-row"><span>${t('lbl_current_state')}</span><strong>${d.capacity}</strong></div>`;
  } else {
    section.classList.add('hidden');
  }
}

function renderBalancingAssistant(d) {
  const container = el('balanceAssistant');
  const section = el('balanceSection');
  if (!container) return;

  if (d.cell_diff > 0.05) {
    section.classList.remove('hidden');
    const avgV = d.cell_voltages.reduce((a, b) => a + b, 0) / d.cell_voltages.length;
    let html = `<p>${t('lbl_imbalance_detected')} <strong>${d.cell_diff.toFixed(3)}V</strong></p>`;
    d.cell_voltages.forEach((v, i) => {
      const diff = v - avgV;
      if (Math.abs(diff) > 0.02) {
        const action = diff > 0 ? t('lbl_action_discharge') : t('lbl_action_charge');
        const color = diff > 0 ? 'var(--error)' : 'var(--accent)';
        html += `<div class="balance-step"><span style="color:${color}">&#9679;</span><span>${t('cell')} ${i + 1}: <strong>${action}</strong> (~${(Math.abs(diff) * 500).toFixed(0)} mAh)</span></div>`;
      }
    });
    container.innerHTML = html;
  } else {
    section.classList.add('hidden');
  }
}

function renderImbalanceHUD(d) {
  const container = el('imbalanceHUD');
  if (!container) return;
  const avg = d.cell_voltages.reduce((a, b) => a + b, 0) / d.cell_voltages.length;
  container.innerHTML = d.cell_voltages.map((v, i) => {
    const diff = v - avg;
    const absDiff = Math.abs(diff);
    const width = Math.min(100, (absDiff / 0.2) * 100);
    const color = absDiff > 0.1 ? 'var(--error)' : (absDiff > 0.05 ? 'var(--warning)' : 'var(--success)');
    return `
      <div class="hud-row">
        <span class="hud-label">${t('cell')} ${i + 1}</span>
        <div class="hud-bar-bg">
          <div class="hud-bar-fill" style="width:${width}%;background:${color};${diff < 0 ? 'right:0' : 'left:0'}"></div>
        </div>
        <span class="hud-val" style="color:${color}">${diff > 0 ? '+' : ''}${diff.toFixed(3)}</span>
      </div>`;
  }).join('');
}

function updateImbalanceBadge(diff) {
  const badge = el('imbalanceBadge');
  if (!badge) return;
  badge.classList.remove('hidden', 'bal-ok', 'bal-warn', 'bal-crit');
  if (diff < 0.05) { badge.classList.add('bal-ok'); badge.textContent = t('bal_ok'); }
  else if (diff < 0.15) { badge.classList.add('bal-warn'); badge.textContent = t('bal_warn'); }
  else { badge.classList.add('bal-crit'); badge.textContent = t('bal_crit'); }
}

function generateReport() {
  if (!lastData) return;
  const d = lastData;
  const date = new Date().toLocaleString();
  let report = `==========================================\n`;
  report += `   ${t('report_title')}\n`;
  report += `==========================================\n\n`;
  report += `DATE: ${date}\nMODEL: ${d.model}\nROM ID: ${d.rom_id}\nSTATUS: ${d.lock_status}\n\n`;
  report += `------------------------------------------\nCELL DIAGNOSTICS\n------------------------------------------\n`;
  d.cell_voltages.forEach((v, i) => { report += `${t('cell')} ${i + 1}: ${v.toFixed(3)}V\n`; });
  report += `\nPACK VOLTAGE: ${d.pack_voltage.toFixed(2)}V\nIMBALANCE: ${d.cell_diff.toFixed(3)}V\nCYCLES: ${d.charge_cycles}\n\n`;
  report += `------------------------------------------\n`;
  report += `SOH: ${el('sohRing').textContent}\nTEMPS: ${el('tempValues').textContent}\n`;
  report += `==========================================\nGenerated by Makita BMS Tool\n`;

  const blob = new Blob([report], { type: 'text/plain' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `Report_${d.model.replace(/\s/g, '_')}.txt`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
  log("Report generated.");
}

// ── UI State ──
function updateButtonStates(f) {
  el('btnReadDynamic').disabled = !f.read_dynamic;
  el('btnClearErrors').disabled = !f.clear_errors;
  el('btnLed').disabled = !f.led_test;
  el('serviceActions').classList.toggle('hidden', !(f.clear_errors || f.led_test));
  toggleAutoPolling(el('checkAuto').checked);
}

function toggleAutoPolling(active) {
  clearInterval(pollInterval);
  pollInterval = null;
  savePref('auto', active);
  if (active && isConnected && !el('btnReadDynamic').disabled) {
    pollInterval = setInterval(() => { sendCommand('read_dynamic'); }, 3000);
  }
}

function updatePresence(present) {
  lastPresence = present;
  refreshStatus();
  if (!present && isConnected) {
    el('overviewCard').classList.add('hidden');
    el('serviceActions').classList.add('hidden');
    lastData = null;
  }
}

function showNotification(msg, type) {
  const n = el('notification');
  if (!n) return;
  n.textContent = msg;
  n.className = type;
  n.classList.remove('hidden');
  setTimeout(() => n.classList.add('hidden'), 4000);
}

function log(msg) {
  const logEl = el('log');
  if (logEl) {
    const d = new Date();
    const ts = d.getHours().toString().padStart(2, '0') + ":" + d.getMinutes().toString().padStart(2, '0') + ":" + d.getSeconds().toString().padStart(2, '0');
    logEl.textContent += `\n[${ts}] ${msg}`;
    logEl.scrollTop = logEl.scrollHeight;
  }
}

// ── Event Listeners ──
function setupEventListeners() {
  log(t('log_evt_config'));

  const bStatic = el('btnReadStatic');
  const bDynamic = el('btnReadDynamic');
  const bClear = el('btnClearErrors');
  const bLed = el('btnLed');
  const bEN = el('btnEN');
  const bES = el('btnES');

  if (bStatic) bStatic.addEventListener('click', () => { log(t('log_req_static')); sendCommand('read_static'); });
  if (bDynamic) bDynamic.addEventListener('click', () => { log(t('log_req_dynamic')); sendCommand('read_dynamic'); });
  if (bClear) bClear.addEventListener('click', () => { if (confirm(t('log_clear_confirm'))) sendCommand('clear_errors'); });

  let ledOn = false;
  if (bLed) bLed.addEventListener('click', () => { ledOn = !ledOn; sendCommand(ledOn ? 'led_on' : 'led_off'); });

  const setLang = (lang) => {
    currentLang = lang;
    applyTranslations();
    if (isConnected) {
      const isDark = document.body.classList.contains('dark-mode');
      sendCommand('save_config', { lang: lang, theme: isDark ? 'dark' : 'light' });
    }
  };

  if (bEN) bEN.addEventListener('click', () => setLang('en'));
  if (bES) bES.addEventListener('click', () => setLang('es'));

  const bAuto = el('checkAuto');
  if (bAuto) bAuto.addEventListener('change', (e) => toggleAutoPolling(e.target.checked));

  const bLight = el('themeLight');
  const bDark = el('themeDark');
  if (bLight) bLight.addEventListener('click', () => setTheme('light'));
  if (bDark) bDark.addEventListener('click', () => setTheme('dark'));

  const bExp = el('btnExport');
  if (bExp) bExp.addEventListener('click', generateReport);

  const bSys = el('btnSystem');
  if (bSys) bSys.addEventListener('click', () => el('systemSection').classList.toggle('hidden'));

  const bOta = el('btnOta');
  const fOta = el('otaFile');
  if (bOta && fOta) {
    bOta.addEventListener('click', () => fOta.click());
    fOta.addEventListener('change', () => uploadFirmware(fOta.files[0]));
  }

  const bWifi = el('btnSaveWifi');
  if (bWifi) {
    bWifi.addEventListener('click', () => {
      const ssid = el('wifiSSID').value;
      const pass = el('wifiPass').value;
      if (!ssid) return alert("Enter a network name (SSID)");
      if (confirm(`Configure WiFi and restart?\nNetwork: ${ssid}`)) {
        sendCommand('set_wifi', { ssid: ssid, pass: pass });
      }
    });
  }
}

function uploadFirmware(file) {
  if (!file) return;
  if (!confirm('Update ESP32 firmware?')) return;

  const formData = new FormData();
  formData.append('update', file);

  const xhr = new XMLHttpRequest();
  const bar = el('otaBar');
  const container = el('otaProgress');
  const percentText = el('otaPercent');

  container.classList.remove('hidden');
  xhr.open('POST', '/update', true);

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      bar.style.width = pct + '%';
      if (percentText) percentText.textContent = pct + '%';
    }
  };

  xhr.onload = () => {
    if (xhr.status === 200) {
      log("Update successful. Rebooting...");
      showNotification("Success! Rebooting...", "success");
      setTimeout(() => window.location.reload(), 5000);
    } else {
      log("Update failed.");
      showNotification("Update Failed", "danger");
      container.classList.add('hidden');
    }
  };

  xhr.send(formData);
}
