// ============================================================================
//  app_types.h — Types partagés transitant par les queues FreeRTOS.
//  POD only : copiés par valeur dans les queues (pas de pointeurs propriétaires).
// ============================================================================
#pragma once

#include <Arduino.h>

// --- Échantillon capteur (push par SensorAcquisition) -----------------------
struct SensorSample {
    uint32_t ts;        // epoch (s) ou millis() si NTP indispo
    float    temp;      // °C   (filtré, DHT22)
    float    humidity;  // %    (filtré, DHT22)
    bool     contact;   // état des deux fils (true = en contact)
    bool     valid;     // false si valeur aberrante détectée
};

// --- Commande actionneur (web/MQTT -> Actuators) ----------------------------
enum class ActuatorType : uint8_t {
    RELAY = 0,
    LED_RGB,
};

struct ActuatorCmd {
    ActuatorType type;
    bool    relayOn;        // si type == RELAY
    uint8_t r, g, b;        // si type == LED_RGB (0..255)
};

// --- Payload sortant prêt à publier (SensorAcquisition -> Network/Storage) ---
// Format JSON imposé sérialisé dans un buffer fixe (cf. CLAUDE.md §4).
static constexpr size_t OUTBOUND_JSON_MAX = 256;
struct OutboundPayload {
    char     json[OUTBOUND_JSON_MAX];
    uint32_t ts;            // pour mesurer la latence de publication
};
