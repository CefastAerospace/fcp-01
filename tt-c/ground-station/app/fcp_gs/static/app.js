// Interface da estação terrena: estado por /api/state, eventos ao vivo por /events (SSE).
"use strict";

const COMMANDS = ["ping", "set_mode", "start_mission", "stop_mission", "get_status", "enter_safe",
  "exit_safe", "request_tm", "set_time", "adcs_start", "adcs_stop", "deploy_antenna", "request_log",
  "test_tm", "reset_obc"];
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
  return [`← ${JSON.stringify(event.pkt)}${signal}`, ""];
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

const select = $("tc-cmd");
COMMANDS.forEach((cmd) => select.append(new Option(cmd, cmd)));

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

connect();
refreshState();
setInterval(refreshState, 2000);
