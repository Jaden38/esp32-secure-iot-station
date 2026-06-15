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

// --- Configuration de régulation métier (NVS), lue par les tâches métier ---
struct ControlConfig {
    uint16_t acqPeriodMs;    // période acquisition capteurs
    uint16_t ctrlPeriodMs;   // période régulation relais/LED
    uint16_t pubPeriodMs;    // période publication MQTT
    float    tempOn;         // °C : ventilation ON au-dessus
    float    hysteresis;     // °C : OFF sous (tempOn - hysteresis)
    float    humAlert;       // % : alerte humidité au-dessus
    uint8_t  mode;           // CtrlMode : MODE_AUTO / MODE_MANUEL
    bool     relayManual;    // état relais désiré en mode MANUEL
    bool     estopAutoReset; // true = estop se réarme à l'ouverture du contact
};
ControlConfig runtimeGetControl();
void          runtimeSetControl(const ControlConfig& c);   // borné + persisté

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
