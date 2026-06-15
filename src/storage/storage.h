// ============================================================================
//  storage/ — LittleFS : buffer offline + replay (cf. cahier des charges §4).
//  Quand MQTT KO : append du JSON dans buffer.jsonl (1 ligne = 1 message).
//  À la reconnexion (BIT_MQTT_OK) : relit et republie, puis tronque.
//  Tout accès FS sous fsMutex. Choix (figé tâche #5) : LittleFS — SPIFFS est
//  déprécié ; LittleFS offre un append "crash-proof" et un meilleur wear-leveling.
// ============================================================================
#pragma once

#include "app_types.h"

bool storageInit();

// Append d'un payload non publié (appelé par network/ quand MQTT KO).
bool storageAppend(const OutboundPayload& p);

// Tâche FreeRTOS de replay (CORE_STORAGE, PRIO_STORAGE).
void storageTask(void* pv);
