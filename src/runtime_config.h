// ============================================================================
//  runtime_config.h — Configuration MODIFIABLE à chaud (vs config.h compile-time).
//  Écrite par web/ (UI), lue par sensors/ & network/. Persistance NVS (Preferences).
//   - "seuil" : remplace le potentiomètre (slider web)
//   - config MQTT : broker/port/user/pass éditables depuis l'UI (CLAUDE.md §5)
// ============================================================================
#pragma once

#include <Arduino.h>

// Charge la config depuis NVS (défauts secrets.h si absente). À appeler dans setup().
void runtimeConfigInit();

// --- Seuil (ex-potentiomètre), accès thread-safe ---
uint16_t runtimeGetThreshold();
void     runtimeSetThreshold(uint16_t v);     // borné + persisté

// --- Configuration MQTT (NVS) ---
struct MqttConfig {
    char     host[64];
    uint16_t port;
    char     user[32];
    char     pass[64];
};
MqttConfig runtimeGetMqtt();
void       runtimeSetMqtt(const MqttConfig& c);   // persiste + marque "dirty"

// "dirty" : positionné quand la config MQTT change -> network/ force une reconnexion.
bool runtimeMqttDirty();
void runtimeClearMqttDirty();
