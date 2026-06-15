// ============================================================================
//  runtime_config.h — Configuration MODIFIABLE à chaud (vs config.h compile-time).
//  Écrite par web/ (UI), lue par sensors/ & network/. Persistance NVS (tâche #6).
//  Remplace notamment le potentiomètre : le "seuil" est réglé via l'UI web.
// ============================================================================
#pragma once

#include <Arduino.h>

// Charge la config depuis NVS (ou valeurs par défaut). À appeler dans setup().
void runtimeConfigInit();

// --- Seuil (ex-potentiomètre), accès atomique thread-safe ---
uint16_t runtimeGetThreshold();
void     runtimeSetThreshold(uint16_t v);   // borné [THRESHOLD_MIN..MAX] + persist

// TODO(tâche #6) : getters/setters config MQTT (host/port/user/pass/topic) NVS.
