"use strict";

// Minimal Smart Home Brain web app: logs in to the broker over MQTT/WebSockets
// and shows the board status (home/system/status, see README "MQTT topics").

const STATUS_TOPIC = "home/system/status";
const PREFS_KEY = "shb.login";  // URL and username only; the password is never stored

const $ = (id) => document.getElementById(id);

let client = null;
let lastStatus = null;      // last parsed status payload
let lastStatusAt = 0;       // Date.now() when it arrived
let everConnected = false; // distinguishes "wrong URL" from a later network drop

function loadPrefs() {
  try {
    return JSON.parse(localStorage.getItem(PREFS_KEY)) || {};
  } catch {
    return {};
  }
}

function savePrefs(prefs) {
  try {
    localStorage.setItem(PREFS_KEY, JSON.stringify(prefs));
  } catch {
    // Storage blocked (private mode): the form just starts empty next time.
  }
}

function setConn(text, kind) {
  const el = $("conn");
  el.textContent = text;
  el.className = `pill pill-${kind}`;
}

function showLoginError(message) {
  const el = $("login-error");
  el.textContent = message;
  el.hidden = !message;
}

function formatUptime(totalSeconds) {
  const s = Math.max(0, Math.floor(totalSeconds));
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const parts = [];
  if (d) parts.push(`${d}d`);
  if (d || h) parts.push(`${h}h`);
  parts.push(`${m}m`, `${s % 60}s`);
  return parts.join(" ");
}

function renderStatus() {
  const state = $("board-state");
  if (!lastStatus) {
    state.textContent = "Waiting for status…";
    state.className = "board board-unknown";
    return;
  }

  const online = lastStatus.online === true;
  state.textContent = online ? "Board online" : "Board offline";
  state.className = `board board-${online ? "online" : "offline"}`;

  // The board publishes uptime only at the moment of sending (NFR5: no periodic
  // stream), so the page extrapolates it while the board is online.
  if (online && Number.isFinite(lastStatus.uptime_s)) {
    $("uptime").textContent = formatUptime(lastStatus.uptime_s + (Date.now() - lastStatusAt) / 1000);
  } else {
    $("uptime").textContent = "–";
  }
  $("errors").textContent = Number.isFinite(lastStatus.errors) ? String(lastStatus.errors) : "–";
  $("fw").textContent = typeof lastStatus.fw === "string" ? lastStatus.fw : "–";
  $("last-update").textContent = new Date(lastStatusAt).toLocaleTimeString();

  const lastError = $("last-error");
  lastError.textContent = lastStatus.last_error ? `Last error: ${lastStatus.last_error}` : "";
  lastError.hidden = !lastStatus.last_error;
}

function onStatusMessage(payload) {
  try {
    const parsed = JSON.parse(payload.toString());
    if (typeof parsed !== "object" || parsed === null) throw new Error("not an object");
    lastStatus = parsed;
    lastStatusAt = Date.now();
  } catch (err) {
    console.warn("Ignoring malformed status payload:", payload.toString(), err);
    return;
  }
  renderStatus();
}

function showDashboard(visible) {
  $("login").hidden = visible;
  $("dashboard").hidden = !visible;
}

function connect(url, username, password) {
  const button = $("login-form").querySelector("button");
  button.disabled = true;
  showLoginError("");
  setConn("Connecting…", "off");

  client = mqtt.connect(url, {
    username,
    password,
    clientId: `shb-web-${Math.random().toString(16).slice(2, 10)}`,
    reconnectPeriod: 3000,
    connectTimeout: 8000,
    clean: true,
  });

  client.on("connect", () => {
    everConnected = true;
    button.disabled = false;
    setConn("Connected", "on");
    showDashboard(true);
    client.subscribe(STATUS_TOPIC, { qos: 1 }, (err) => {
      if (err) console.error("Subscribe failed:", err);
    });
  });

  client.on("message", (topic, payload) => {
    if (topic === STATUS_TOPIC) onStatusMessage(payload);
  });

  client.on("reconnect", () => setConn("Reconnecting…", "off"));
  client.on("offline", () => setConn("Broker unreachable", "bad"));

  client.on("close", () => {
    // Never connected: the URL is wrong or the broker is down. Stop retrying.
    // After a successful connection, keep retrying (the page recovers by itself).
    if (!everConnected && client) {
      disconnect();
      showLoginError(`Cannot reach the broker at ${url}.`);
    }
  });

  client.on("error", (err) => {
    // Wrong credentials are final: stop retrying and go back to the login form.
    const authFailed = err && (err.code === 4 || err.code === 5 || /not authori[sz]ed|bad user/i.test(err.message));
    if (authFailed) {
      disconnect();
      showLoginError("Login refused: check username and password.");
    }
  });
}

function disconnect() {
  everConnected = false;
  if (client) {
    // Clear the reference first: end() can emit "close" synchronously.
    const old = client;
    client = null;
    old.end(true);
  }
  lastStatus = null;
  renderStatus();
  showDashboard(false);
  setConn("Disconnected", "off");
  $("login-form").querySelector("button").disabled = false;
}

function init() {
  const prefs = loadPrefs();
  // Default: broker on the same host that serves this page (see broker/mosquitto.conf).
  $("url").value = prefs.url || `ws://${location.hostname || "localhost"}:9001`;
  $("username").value = prefs.username || "";

  $("login-form").addEventListener("submit", (event) => {
    event.preventDefault();
    const url = $("url").value.trim();
    const username = $("username").value.trim();
    savePrefs({ url, username });
    connect(url, username, $("password").value);
    $("password").value = "";
  });

  $("disconnect").addEventListener("click", disconnect);

  setInterval(() => {
    if (lastStatus && lastStatus.online === true) renderStatus();
  }, 1000);
}

init();
