#include "runtime_config.h"
#include "config.h"

// Stockage atomique simple (uint16 tient dans un mot -> lecture/écriture atomiques
// sur Xtensa). NVS branché en tâche #6.
static volatile uint16_t s_threshold = THRESHOLD_DEFAULT;

void runtimeConfigInit() {
    // TODO(tâche #6) : Preferences.begin("cfg"); charger seuil + config MQTT.
    s_threshold = THRESHOLD_DEFAULT;
}

uint16_t runtimeGetThreshold() {
    return s_threshold;
}

void runtimeSetThreshold(uint16_t v) {
    if (v > THRESHOLD_MAX) v = THRESHOLD_MAX;
    s_threshold = v;
    // TODO(tâche #6) : persister dans NVS.
}
