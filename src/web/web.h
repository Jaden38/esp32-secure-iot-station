// ============================================================================
//  web/ — Interface embarquée (ESPAsyncWebServer, esp32async fork).
//  Sert /data (index.html, app.js, style.css) depuis LittleFS — AUCUN HTML/JS
//  inliné (cf. cahier des charges §5). API /api/* protégée par token (cf. §8).
//  Fonctions : live capteurs, état Wi-Fi/MQTT, config MQTT (NVS), cmd actionneurs.
// ============================================================================
#pragma once

bool webInit();

// Tâche FreeRTOS serveur web (CORE_WEB, PRIO_WEB).
// NB: AsyncWebServer est piloté par callbacks ; la tâche ne fait que surveiller
// l'état (les mesures live sont lues à la demande via sensorsGetLatest()).
void webTask(void* pv);
