// app.js — Client de l'UI embarquée. Dialogue avec l'API /api/* (token Bearer).
"use strict";

let API_TOKEN = localStorage.getItem("apiToken") || "";

function ensureToken() {
  if (!API_TOKEN) {
    const t = prompt("Token API (Bearer) :");
    if (t) { API_TOKEN = t; localStorage.setItem("apiToken", t); }
  }
}

function authHeaders() {
  return { "Authorization": "Bearer " + API_TOKEN };
}

function jsonHeaders() {
  return { ...authHeaders(), "Content-Type": "application/json" };
}

function setText(id, v) {
  const el = document.getElementById(id);
  if (el) el.textContent = (v ?? "—");
}

function setVal(name, v) {
  const el = document.querySelector(`[name=${name}]`);
  if (el && document.activeElement !== el) el.value = v;
}

function setBadge(id, label, ok) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = label + " " + (ok ? "OK" : "KO");
  el.classList.toggle("ok", !!ok);
  el.classList.toggle("ko", !ok);
}

const thr = document.getElementById("threshold");

// --- Mesures live + état connexion ---
async function refresh() {
  try {
    const r = await fetch("/api/live", { headers: authHeaders() });
    if (!r.ok) return;
    const d = await r.json();
    setText("temp", d.data?.temp);
    setText("humidity", d.data?.humidity);
    setText("contact", d.contact ? "fermé" : "ouvert");
    setBadge("wifi", "Wi-Fi", d.wifi);
    setBadge("mqtt", "MQTT", d.mqtt);
    // Synchronise le slider sur la valeur persistée (sauf pendant un réglage)
    if (typeof d.threshold === "number" && thr && document.activeElement !== thr) {
      thr.value = d.threshold;
      setText("threshold-val", d.threshold);
    }
  } catch (e) { /* hors-ligne : on garde le dernier état */ }
}

// --- OLED virtuel ---
async function refreshOled() {
  try {
    const r = await fetch("/api/supervision", { headers: authHeaders() });
    if (r.ok) document.getElementById("oled").textContent = await r.text();
  } catch (e) { /* hors-ligne */ }
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
    const topic = document.querySelector("[name=topic]");
    if (topic) { topic.value = c.topic; topic.readOnly = true; }
    const pass = document.querySelector("[name=pass]");
    if (pass) pass.placeholder = c.passSet ? "•••• (défini)" : "(vide)";
  } catch (e) { /* ignore */ }
}

document.getElementById("mqtt-form")?.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  const body = {
    host: fd.get("host"),
    port: Number(fd.get("port")),
    user: fd.get("user"),
    pass: fd.get("pass"),
  };
  await fetch("/api/config", { method: "POST", headers: jsonHeaders(), body: JSON.stringify(body) });
  loadConfig();
});

// --- Commandes actionneurs ---
document.getElementById("relay-on")?.addEventListener("click", () => sendRelay(true));
document.getElementById("relay-off")?.addEventListener("click", () => sendRelay(false));

async function sendRelay(on) {
  await fetch("/api/actuator", { method: "POST", headers: jsonHeaders(),
    body: JSON.stringify({ type: "relay", on }) });
}

document.getElementById("led")?.addEventListener("change", async (e) => {
  const hex = e.target.value;                      // #rrggbb
  const r = parseInt(hex.substr(1, 2), 16);
  const g = parseInt(hex.substr(3, 2), 16);
  const b = parseInt(hex.substr(5, 2), 16);
  await fetch("/api/actuator", { method: "POST", headers: jsonHeaders(),
    body: JSON.stringify({ type: "led", r, g, b }) });
});

// --- Seuil (ex-potentiomètre) ---
thr?.addEventListener("input", () => setText("threshold-val", thr.value));
thr?.addEventListener("change", async () => {
  await fetch("/api/threshold", { method: "POST", headers: jsonHeaders(),
    body: JSON.stringify({ value: Number(thr.value) }) });
});

ensureToken();
loadConfig();
setInterval(refresh, 1000);
setInterval(refreshOled, 2000);
refresh();
refreshOled();
