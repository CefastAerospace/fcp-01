// Interface da estação terrena: estado por /api/state, eventos ao vivo por /events (SSE).
"use strict";

const COMMANDS = ["ping", "set_mode", "start_mission", "stop_mission", "get_status", "enter_safe",
  "exit_safe", "request_tm", "set_time", "adcs_start", "adcs_stop", "deploy_antenna", "request_log",
  "test_tm", "reset_obc"];
const MODES = ["bench", "safe", "detumb", "sun", "mission", "lowpwr"];
// Exemplos de args do catálogo (common/protocol/ttc-json.md); o OBC valida o conteúdo.
const COMMAND_ARGS = {
  set_mode: () => ({ mode: $("tc-mode")?.value || "safe" }),
  set_time: () => ({ unix: Math.floor(Date.now() / 1000) }),
  test_tm: () => ({ count: 1000 }),
};
const MAX_LOG = 300;
const MAX_POINTS = 300;

const $ = (id) => document.getElementById(id);
const fmt = (value, digits = 1) => (typeof value === "number" ? value.toFixed(digits) : "—");
const cssVar = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();

let lastPacketAt = null;

// ─── Gráfico de linhas em canvas, sem dependências (uso offline) ─────

class LineChart {
  constructor(canvas, seriesCount) {
    this.canvas = canvas;
    this.series = Array.from({ length: seriesCount }, () => []);
    new ResizeObserver(() => this.draw()).observe(canvas);
  }

  push(values) {
    values.forEach((value, index) => {
      const points = this.series[index];
      points.push(typeof value === "number" ? value : null);
      if (points.length > MAX_POINTS) points.shift();
    });
    this.draw();
  }

  draw() {
    const ratio = window.devicePixelRatio || 1;
    const width = this.canvas.clientWidth;
    const height = this.canvas.clientHeight;
    this.canvas.width = width * ratio;
    this.canvas.height = height * ratio;
    const ctx = this.canvas.getContext("2d");
    ctx.scale(ratio, ratio);
    ctx.clearRect(0, 0, width, height);

    const values = this.series.flat().filter((v) => v !== null);
    ctx.font = "11px system-ui, sans-serif";
    ctx.fillStyle = cssVar("--muted");
    if (values.length === 0) {
      ctx.fillText("sem dados", 8, height / 2);
      return;
    }

    let min = Math.min(...values);
    let max = Math.max(...values);
    if (min === max) { min -= 1; max += 1; }
    const left = 44, right = 6, top = 6, bottom = 6;
    const plotWidth = width - left - right;
    const plotHeight = height - top - bottom;
    const y = (v) => top + plotHeight * (1 - (v - min) / (max - min));

    // Grade e escala.
    ctx.strokeStyle = cssVar("--border");
    ctx.lineWidth = 1;
    [min, (min + max) / 2, max].forEach((v) => {
      ctx.beginPath();
      ctx.moveTo(left, y(v));
      ctx.lineTo(width - right, y(v));
      ctx.stroke();
      ctx.fillText(v.toFixed(Math.abs(max - min) < 10 ? 2 : 0), 2, y(v) + 4);
    });

    // Séries.
    this.series.forEach((points, index) => {
      ctx.strokeStyle = cssVar(`--series-${(index % 5) + 1}`);
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      let started = false;
      points.forEach((v, i) => {
        if (v === null) { started = false; return; }
        const x = left + (plotWidth * i) / Math.max(MAX_POINTS - 1, 1);
        if (started) ctx.lineTo(x, y(v)); else ctx.moveTo(x, y(v));
        started = true;
      });
      ctx.stroke();
    });
  }
}

const charts = {
  vb: new LineChart($("ch-vb"), 1),
  tp: new LineChart($("ch-tp"), 5),
  rssi: new LineChart($("ch-rssi"), 1),
};

// ─── Mapa das aeronaves ──────────────────────────────────────────────
// Fundo em duas camadas: contornos offline (static/vendor/borders, sempre
// disponível) e folhas OSM via /tiles, servidas do cache local do servidor.

const MAP_CENTER = [-15.79, -47.88];  // Padrão; o servidor manda o de --map-center.
const MAP_ZOOM = 4;
const AC_TRAIL_POINTS = 200;          // Igual ao TRACK_POINTS do servidor.
const AC_FADE_MS = 30 * 1000;         // Sem notícia há 30 s: rastro esmaecido.
const AC_STALE_MS = 15 * 60 * 1000;   // Some do mapa após 15 min sem notícia.

class AircraftMap {
  constructor(container) {
    this.map = L.map(container, { worldCopyJump: true }).setView(MAP_CENTER, MAP_ZOOM);
    this.map.attributionControl.setPrefix("");
    this.entries = new Map(); // ICAO -> { trail, trailLine, marker, lastSeen }

    const osm = L.tileLayer("/tiles/{z}/{x}/{y}.png", {
      maxZoom: 17,
      attribution: "&copy; OpenStreetMap",
    }).addTo(this.map);
    this.borders = L.layerGroup().addTo(this.map);
    L.control.layers(null, { "OSM (cache local)": osm, "Contornos offline": this.borders },
      { position: "topright" }).addTo(this.map);
    this.loadBorders();
  }

  async loadBorders() {
    try {
      const data = await (await fetch("/static/vendor/borders/borders.geojson")).json();
      L.geoJSON(data, {
        interactive: false,
        style: (feature) => ({
          color: cssVar("--muted"),
          weight: feature.properties.k === "state" ? 0.6 : 1.2,
          opacity: feature.properties.k === "state" ? 0.5 : 0.9,
          dashArray: feature.properties.k === "state" ? "3 3" : null,
          fill: false,
        }),
      }).addTo(this.borders);
    } catch (error) {
      appendLog(`contornos do mapa indisponíveis: ${error}`, "err");
    }
  }

  setView(center, zoom) {
    this.map.setView(center, zoom);
  }

  static color(alt) {
    const bands = [1000, 5000, 10000, 20000];
    const index = bands.findIndex((band) => (alt ?? 0) < band);
    return ["--series-1", "--series-3", "--series-2", "--series-4", "--series-5"][index === -1 ? 4 : index];
  }

  static valid(lat, lon) {
    return Number.isFinite(lat) && Number.isFinite(lon) && Math.abs(lat) <= 90 && Math.abs(lon) <= 180;
  }

  entry(icao) {
    let entry = this.entries.get(icao);
    if (!entry) {
      entry = { trail: [], trailLine: null, marker: null, lastSeen: 0 };
      this.entries.set(icao, entry);
    }
    return entry;
  }

  // Rastros guardados pelo servidor: a página recarregada não perde o histórico.
  loadTracks(tracks) {
    Object.entries(tracks).forEach(([icao, points]) => {
      const entry = this.entry(icao);
      entry.trail = points.filter(([lat, lon]) => AircraftMap.valid(lat, lon))
        .map(([lat, lon]) => [lat, lon]).slice(-AC_TRAIL_POINTS);
      this.drawTrail(entry, points.length ? points[points.length - 1][2] : null);
    });
  }

  upsert(list) {
    const now = Date.now();
    list.forEach((ac) => {
      if (AircraftMap.valid(ac.lat, ac.lon)) this.reach(String(ac.icao), ac, now);
    });
    [...this.entries].forEach(([icao, entry]) => {
      const age = now - entry.lastSeen;
      if (entry.lastSeen && age > AC_STALE_MS) { this.remove(icao); return; }
      const faded = !entry.lastSeen || age > AC_FADE_MS;
      entry.trailLine?.setStyle({ opacity: faded ? 0.25 : 0.75 });
      entry.marker?.setOpacity(faded ? 0.35 : 1);
    });
  }

  drawTrail(entry, alt) {
    const color = cssVar(AircraftMap.color(alt));
    if (!entry.trailLine) {
      entry.trailLine = L.polyline(entry.trail, { color, weight: 2, opacity: 0.75 }).addTo(this.map);
    } else {
      entry.trailLine.setStyle({ color });
      entry.trailLine.setLatLngs(entry.trail);
    }
  }

  reach(icao, ac, now) {
    const color = cssVar(AircraftMap.color(ac.alt));
    const entry = this.entry(icao);
    entry.lastSeen = now;
    const point = [ac.lat, ac.lon];
    const last = entry.trail[entry.trail.length - 1];
    // O polling de /api/state repete a última posição: só pontos novos entram no rastro.
    if (!last || last[0] !== point[0] || last[1] !== point[1]) {
      entry.trail.push(point);
      if (entry.trail.length > AC_TRAIL_POINTS) entry.trail.shift();
    }
    this.drawTrail(entry, ac.alt);

    const heading = (((ac.hdg ?? 0) % 360) + 360) % 360;
    // Silhueta apontando para o norte, girada pelo rumo (o glifo ✈ varia com a fonte).
    const html = `<span class="ac-icon" style="color:${color}"><svg viewBox="0 0 24 24" width="20" height="20" style="transform:rotate(${heading}deg)"><path fill="currentColor" stroke="var(--bg)" stroke-width="0.8" d="M12 2c.8 0 1.4.8 1.4 1.8V9l7.6 4.6v2l-7.6-2.3v4.5l2.2 1.7V21L12 20l-3.6 1v-1.5l2.2-1.7v-4.5L3 15.6v-2L10.6 9V3.8C10.6 2.8 11.2 2 12 2z"/></svg></span>`;
    const icon = L.divIcon({ html, className: "", iconSize: [22, 22], iconAnchor: [11, 11] });
    const course = typeof ac.hdg === "number" ? `${fmt(heading, 0)}°` : "rumo —";
    const tooltip = `<b class="ac-tip">${icao}</b> ${course}<br>alt ${fmt(ac.alt, 0)} ft · vel ${fmt(ac.vel, 0)} kt · ${ac.cnt ?? "—"} msgs`;
    if (!entry.marker) {
      entry.marker = L.marker(point, { icon, zIndexOffset: 100 }).addTo(this.map)
        .bindTooltip(tooltip, { direction: "top", offset: [0, -12] });
    } else {
      entry.marker.setLatLng(point);
      entry.marker.setIcon(icon);
      entry.marker.setTooltipContent(tooltip);
    }
  }

  remove(icao) {
    const entry = this.entries.get(icao);
    if (!entry) return;
    if (entry.trailLine) this.map.removeLayer(entry.trailLine);
    if (entry.marker) this.map.removeLayer(entry.marker);
    this.entries.delete(icao);
  }

  fit() {
    const points = [...this.entries.values()]
      .flatMap((entry) => entry.trail)
      .map((point) => L.latLng(point[0], point[1]));
    if (points.length === 0) return;
    if (points.length === 1) this.map.setView(points[0], Math.max(this.map.getZoom(), 9));
    else this.map.fitBounds(L.latLngBounds(points), { padding: [30, 30], maxZoom: 11 });
  }

  clearTrails() {
    this.entries.forEach((entry) => {
      const last = entry.trail[entry.trail.length - 1];
      entry.trail = last ? [last] : [];
      if (entry.trailLine) entry.trailLine.setLatLngs(entry.trail);
    });
  }
}

const map = (window.L !== undefined && $("map")) ? new AircraftMap("map") : null;

// ─── Renderização ────────────────────────────────────────────────────

function renderTelemetry(tm) {
  $("tm-n").textContent = `n=${tm.n}`;
  $("tm-md").textContent = tm.md ?? "—";
  $("tm-ts").textContent = typeof tm.ts === "number" ? new Date(tm.ts * 1000).toISOString().replace("T", " ").slice(0, 19) + " UTC" : "—";
  $("tm-bat").textContent = `${fmt(tm.vb, 2)} V · ${fmt(tm.ib, 0)} mA`;
  $("tm-sol").textContent = `${fmt(tm.vs, 2)} V · ${fmt(tm.is, 0)} mA`;
  $("tm-w").textContent = `${fmt(tm.w)} °/s`;
  $("tm-az").textContent = `${fmt(tm.az)}°`;
  $("tm-mis").textContent = `${tm.ac ?? "—"} / ${tm.msg ?? "—"}`;
  $("tm-lnk").textContent = `${tm.rssi ?? "—"} dBm · ${fmt(tm.snr)} dB · rx ${tm.rx ?? "—"} · err ${tm.err ?? "—"} · rst ${tm.rst ?? "—"}`;
  $("tm-tp").textContent = (tm.tp || []).map((v) => fmt(v)).join(" · ") || "—";
}

function renderAircraft(list) {
  const body = $("ac-body");
  if (!list.length) return;
  body.replaceChildren(...list
    .sort((a, b) => String(a.icao).localeCompare(String(b.icao)))
    .map((ac) => {
      const row = document.createElement("tr");
      const time = typeof ac.ts === "number" ? new Date(ac.ts * 1000).toISOString().slice(11, 19) : "—";
      [ac.icao, fmt(ac.lat, 5), fmt(ac.lon, 5), fmt(ac.alt, 0), fmt(ac.vel), fmt(ac.hdg), ac.cnt ?? "—", time]
        .forEach((value) => {
          const cell = document.createElement("td");
          cell.textContent = value;
          row.append(cell);
        });
      return row;
    }));
}

function renderState(state) {
  $("source").textContent = state.source === "wired" ? "umbilical" : "RF";
  const loss = state.loss;
  $("lk-received").textContent = loss.received;
  $("lk-lost").textContent = loss.lost;
  $("lk-rate").textContent = `${(loss.loss_rate * 100).toFixed(2)}%`;
  $("lk-crc").textContent = state.counts.crc ?? 0;
  $("lk-restarts").textContent = loss.restarts;
  const meta = state.last_meta || {};
  $("lk-signal").textContent = meta.rssi !== undefined ? `${meta.rssi} dBm · ${fmt(meta.snr)} dB` : "—";
  $("lk-log").textContent = state.log_file ?? "—";
  if (state.latest_tm) renderTelemetry(state.latest_tm);
  renderAircraft(state.aircraft || []);
  map?.upsert(state.aircraft || []);
}

function appendLog(text, cls = "") {
  const log = $("log");
  const item = document.createElement("li");
  item.className = cls;
  item.textContent = `${new Date().toLocaleTimeString()}  ${text}`;
  log.prepend(item);
  while (log.children.length > MAX_LOG) log.lastChild.remove();
}

function describe(event) {
  if (event.dir === "tx") return [`→ ${JSON.stringify(event.pkt)}${event.attempt > 1 ? ` (tentativa ${event.attempt})` : ""}`, "tx"];
  if (event.dir === "log") return [event.text, event.kind === "error" ? "err" : "sys"];
  if (event.kind === "crc") return ["quadro com CRC inválido descartado pelo rádio", "err"];
  if (event.kind === "raw") return [`quadro não-JSON: ${event.text}`, "err"];
  const signal = event.gs ? `  [${event.gs.rssi} dBm, ${fmt(event.gs.snr)} dB]` : "";
  const repeated = event.delivery?.repeated ? "  (entrega repetida, só reconfirmada)" : "";
  return [`← ${JSON.stringify(event.pkt)}${signal}${repeated}`, ""];
}

// ─── Eventos ao vivo ─────────────────────────────────────────────────

function handleEvent(event) {
  if (event.dir === "report") {
    $("test-report").textContent = event.summary + (event.path ? `\nRelatório: ${event.path}` : "");
    document.querySelectorAll("form.test button").forEach((b) => { b.disabled = false; });
    return;
  }

  const [text, cls] = describe(event);
  appendLog(text, cls);
  if (event.dir !== "rx") return;

  lastPacketAt = Date.now();
  if (event.gs && typeof event.gs.rssi === "number") charts.rssi.push([event.gs.rssi]);
  const pkt = event.pkt;
  if (pkt && pkt.t === "tm") {
    renderTelemetry(pkt);
    charts.vb.push([pkt.vb]);
    charts.tp.push(pkt.tp || []);
  }
  // Entrega repetida já foi processada: não move o marcador de novo.
  if (pkt && pkt.t === "ac" && !event.delivery?.repeated) map?.upsert([pkt]);
}

function connect() {
  const source = new EventSource("/events");
  source.onopen = () => { $("sse").textContent = "conectado"; $("sse").className = "pill ok"; };
  source.onerror = () => { $("sse").textContent = "desconectado"; $("sse").className = "pill bad"; };
  source.onmessage = (message) => handleEvent(JSON.parse(message.data));
}

async function refreshState() {
  try {
    const response = await fetch("/api/state");
    renderState(await response.json());
  } catch (error) {
    // Servidor fora do ar: o indicador do SSE já mostra.
  }
  $("lk-age").textContent = lastPacketAt ? `${Math.round((Date.now() - lastPacketAt) / 1000)} s atrás` : "—";
}

// ─── Telecomandos e testes ───────────────────────────────────────────

async function sendTc(cmd, args, button) {
  if (button) button.disabled = true;
  const item = document.createElement("li");
  item.textContent = `${cmd}: aguardando ACK…`;
  $("tc-results").prepend(item);
  try {
    const response = await fetch("/api/tc", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ cmd, args }),
    });
    const result = await response.json();
    if (result.error) {
      item.textContent = `${cmd}: ${result.error}`;
      item.className = "bad";
    } else {
      const q = result.q !== null && result.q !== undefined ? ` · q ${result.q} ms` : "";
      const rtt = result.rtt_ms !== null ? ` · ida e volta ${result.rtt_ms} ms` : "";
      item.textContent = `${cmd} seq=${result.seq}: ${result.status}${q}${rtt} · ${result.attempts} tentativa(s)`;
      item.className = result.ok ? "ok" : "bad";
    }
  } catch (error) {
    item.textContent = `${cmd}: ${error}`;
    item.className = "bad";
  } finally {
    if (button) button.disabled = false;
  }
}

document.querySelectorAll("button[data-cmd]").forEach((button) => {
  button.addEventListener("click", () => {
    if (button.dataset.confirm && !window.confirm(button.dataset.confirm)) return;
    sendTc(button.dataset.cmd, null, button);
  });
});

$("btn-time").addEventListener("click", (e) => sendTc("set_time", { unix: Math.floor(Date.now() / 1000) }, e.currentTarget));

$("map-fit").addEventListener("click", () => map?.fit());
$("map-clear-trails").addEventListener("click", () => map?.clearTrails());

$("btn-mode").addEventListener("click", (e) => {
  const mode = $("tc-mode").value;
  if (mode === "lowpwr" && !window.confirm("Forçar LOW-POWER? A saída exige exit_safe.")) return;
  sendTc("set_mode", { mode }, e.currentTarget);
});
MODES.forEach((mode) => $("tc-mode").append(new Option(mode, mode)));

const select = $("tc-cmd");
COMMANDS.forEach((cmd) => select.append(new Option(cmd, cmd)));

// Ao escolher o comando, preenche o exemplo de args (ou limpa, se não houver).
function fillArgs() {
  const example = COMMAND_ARGS[select.value];
  $("tc-args").value = example ? JSON.stringify(example()) : "";
}
select.addEventListener("change", fillArgs);

$("tc-form").addEventListener("submit", (e) => {
  e.preventDefault();
  const text = $("tc-args").value.trim();
  let args = null;
  if (text) {
    try {
      args = JSON.parse(text);
    } catch (error) {
      appendLog(`args inválido: ${error.message}`, "err");
      return;
    }
  }
  sendTc(select.value, args, e.submitter);
});

document.querySelectorAll("form.test").forEach((form) => {
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const button = form.querySelector("button");
    const count = Number(form.querySelector("input").value);
    button.disabled = true;
    const response = await fetch("/api/test", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ test: form.dataset.test, count }),
    });
    const result = await response.json();
    if (response.status !== 202) {
      $("test-report").textContent = result.error;
      button.disabled = false;
      return;
    }
    $("test-report").textContent = `${result.started}: em andamento (${result.count})…`;
  });
});

// Configuração do servidor (centro do mapa, catálogo) e rastros já recebidos.
async function init() {
  try {
    const config = await (await fetch("/api/config")).json();
    if (Array.isArray(config.commands) && config.commands.length) {
      const current = select.value;
      select.replaceChildren(...config.commands.map((cmd) => new Option(cmd, cmd)));
      if (config.commands.includes(current)) select.value = current;
    }
    map?.setView(config.map_center, config.map_zoom);
  } catch (error) {
    // Sem /api/config: mantém o catálogo e o centro padrão.
  }
  try {
    map?.loadTracks(await (await fetch("/api/tracks")).json());
  } catch (error) {
    // Sem rastros anteriores.
  }
  connect();
  refreshState();
  setInterval(refreshState, 2000);
}

init();
