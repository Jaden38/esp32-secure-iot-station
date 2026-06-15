// app.js — Client de l'UI embarquée. Dialogue avec l'API /api/* (token Bearer).
// TODO(tâche #6) : récupérer le token (prompt/localStorage), brancher les POST.

"use strict";

const API_TOKEN = localStorage.getItem("apiToken") || "";

function authHeaders() {
  return { "Authorization": "Bearer " + API_TOKEN };
}

// Rafraîchit les mesures live + état connexion.
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
  } catch (e) {
    // hors-ligne : on laisse l'UI dans son dernier état connu
  }
}

function setText(id, v) {
  const el = document.getElementById(id);
  if (el) el.textContent = (v ?? "—");
}

function setBadge(id, label, ok) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = label + " " + (ok ? "OK" : "KO");
  el.classList.toggle("ok", !!ok);
  el.classList.toggle("ko", !ok);
}

// --- Commandes actionneurs (TODO: POST /api/actuator) ---
document.getElementById("relay-on")?.addEventListener("click", () => sendRelay(true));
document.getElementById("relay-off")?.addEventListener("click", () => sendRelay(false));

async function sendRelay(on) {
  await fetch("/api/actuator", {
    method: "POST",
    headers: { ...authHeaders(), "Content-Type": "application/json" },
    body: JSON.stringify({ type: "relay", on }),
  });
}

// --- Seuil (remplace le potentiomètre) -> POST /api/threshold ---
const thr = document.getElementById("threshold");
thr?.addEventListener("input", () => setText("threshold-val", thr.value));
thr?.addEventListener("change", async () => {
  await fetch("/api/threshold", {
    method: "POST",
    headers: { ...authHeaders(), "Content-Type": "application/json" },
    body: JSON.stringify({ value: Number(thr.value) }),
  });
});

// --- OLED virtuel : rafraîchit le panneau de supervision ---
async function refreshOled() {
  try {
    const r = await fetch("/api/supervision", { headers: authHeaders() });
    if (r.ok) document.getElementById("oled").textContent = await r.text();
  } catch (e) { /* hors-ligne */ }
}

setInterval(refresh, 1000);
setInterval(refreshOled, 2000);
refresh();
refreshOled();
