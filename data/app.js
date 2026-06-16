// app.js — Client de l'UI embarquée. Dialogue avec l'API /api/* (token Bearer).
"use strict";

let API_TOKEN = localStorage.getItem("apiToken") || "";

function ensureToken() {
  if (!API_TOKEN) {
    const t = prompt("Token API (Bearer) :");
    if (t) { API_TOKEN = t; localStorage.setItem("apiToken", t); }
  }
}

function authHeaders() { return { "Authorization": "Bearer " + API_TOKEN }; }
function jsonHeaders() { return { ...authHeaders(), "Content-Type": "application/json" }; }

function setText(id, v) {
  const el = document.getElementById(id);
  if (el) el.textContent = (v ?? "—");
}
function setVal(name, v) {
  const el = document.querySelector(`#ctrl-form [name=${name}], #mqtt-form [name=${name}]`);
  if (el && document.activeElement !== el) el.value = v;
}
function setBadge(id, label, ok) {
  const el = document.getElementById(id);
  if (!el) return;
  el.innerHTML = '<span class="dot"></span>' + label + " " + (ok ? "OK" : "KO");
  el.classList.toggle("ok", !!ok);
  el.classList.toggle("ko", !ok);
}

// Toast de confirmation (succès / erreur)
let toastTimer = null;
function toast(msg, ok = true) {
  const t = document.getElementById("toast");
  if (!t) return;
  t.textContent = msg;
  t.classList.toggle("ok", ok);
  t.classList.toggle("ko", !ok);
  t.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove("show"), 2200);
}

// --- Live + état régulation/sécurité ---
async function refresh() {
  try {
    const r = await fetch("/api/live", { headers: authHeaders() });
    if (!r.ok) return;
    const d = await r.json();
    setText("temp", d.data?.temp);
    setText("humidity", d.data?.humidity);
    setText("contact", d.contact ? "fermé" : "ouvert");
    setText("mode", d.mode);
    setText("relay", d.relay ? "ON" : "OFF");
    document.getElementById("relay")?.classList.toggle("on", !!d.relay);
    document.getElementById("contact")?.classList.toggle("on", !!d.contact);
    setBadge("wifi", "Wi-Fi", d.wifi);
    setBadge("mqtt", "MQTT", d.mqtt);
    // Badge + état arrêt d'urgence
    const estop = !!d.estop;
    const e = document.getElementById("estop");
    if (e) {
      e.innerHTML = '<span class="dot"></span>' + (estop ? "ARRÊT URGENCE" : "Sécurité OK");
      e.classList.toggle("ko", estop);
      e.classList.toggle("ok", !estop);
    }
    setText("estop-state", estop ? "🔴 DÉCLENCHÉ" : "🟢 normal");
  } catch (e) { /* hors-ligne : on garde le dernier état */ }
}

// --- OLED virtuel ---
async function refreshOled() {
  try {
    const r = await fetch("/api/supervision", { headers: authHeaders() });
    if (r.ok) document.getElementById("oled").textContent = await r.text();
  } catch (e) { /* hors-ligne */ }
}

// --- Régulation : charge la config dans le formulaire ---
async function loadControl() {
  try {
    const r = await fetch("/api/control", { headers: authHeaders() });
    if (!r.ok) return;
    const c = await r.json();
    setVal("mode", c.mode);
    setVal("tempOn", c.tempOn);
    setVal("hysteresis", c.hysteresis);
    setVal("humAlert", c.humAlert);
    setVal("acqPeriodMs", c.acqPeriodMs);
    setVal("pubPeriodMs", c.pubPeriodMs);
  } catch (e) { /* ignore */ }
}

document.getElementById("ctrl-form")?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  const body = {
    mode: fd.get("mode"),
    tempOn: Number(fd.get("tempOn")),
    hysteresis: Number(fd.get("hysteresis")),
    humAlert: Number(fd.get("humAlert")),
    acqPeriodMs: Number(fd.get("acqPeriodMs")),
    pubPeriodMs: Number(fd.get("pubPeriodMs")),
  };
  const r = await fetch("/api/control", { method: "POST", headers: jsonHeaders(), body: JSON.stringify(body) });
  toast(r.ok ? "Régulation enregistrée" : "Échec enregistrement", r.ok);
  loadControl();
});

// --- Arrêt d'urgence : réarmement ---
document.getElementById("estop-reset")?.addEventListener("click", async () => {
  const r = await fetch("/api/estop/reset", { method: "POST", headers: authHeaders() });
  toast(r.ok ? "Arrêt d'urgence réarmé" : "Échec réarmement", r.ok);
  refresh();
});

// --- Commande manuelle ventilation (bascule en mode manuel) ---
document.getElementById("relay-on")?.addEventListener("click", () => sendRelay(true));
document.getElementById("relay-off")?.addEventListener("click", () => sendRelay(false));
async function sendRelay(on) {
  const r = await fetch("/api/actuator", { method: "POST", headers: jsonHeaders(),
    body: JSON.stringify({ type: "relay", on }) });
  toast(r.ok ? ("Ventilation " + (on ? "ON" : "OFF") + " (mode manuel)") : "Échec commande", r.ok);
  setTimeout(() => { refresh(); loadControl(); }, 300);
}

// --- Config MQTT ---
async function loadConfig() {
  try {
    const r = await fetch("/api/config", { headers: authHeaders() });
    if (!r.ok) return;
    const c = await r.json();
    setVal("host", c.host);
    setVal("port", c.port);
    setVal("user", c.user);
    const topic = document.querySelector("#mqtt-form [name=topic]");
    if (topic) { topic.value = c.topic; topic.readOnly = true; }
    const pass = document.querySelector("#mqtt-form [name=pass]");
    if (pass) pass.placeholder = c.passSet ? "•••• (défini)" : "(vide)";
  } catch (e) { /* ignore */ }
}

document.getElementById("mqtt-form")?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  const body = { host: fd.get("host"), port: Number(fd.get("port")),
    user: fd.get("user"), pass: fd.get("pass") };
  const r = await fetch("/api/config", { method: "POST", headers: jsonHeaders(), body: JSON.stringify(body) });
  toast(r.ok ? "Config MQTT enregistrée" : "Échec enregistrement", r.ok);
  loadConfig();
});

ensureToken();
loadControl();
loadConfig();
setInterval(refresh, 1000);
setInterval(refreshOled, 2000);
refresh();
refreshOled();
