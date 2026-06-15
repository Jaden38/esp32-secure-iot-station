#include "storage.h"
#include "config.h"
#include "rtos_shared.h"

// TODO(tâche #5) : #include <LittleFS.h>, gestion buffer.jsonl, rotation/limite
// de taille, replay ligne-à-ligne sous fsMutex à la reconnexion MQTT.

static const char* BUFFER_PATH = "/buffer.jsonl";

bool storageInit() {
    // TODO: LittleFS.begin(true); créer buffer.jsonl si absent.
    return true;
}

bool storageAppend(const OutboundPayload& p) {
    (void)p;
    // TODO: take(fsMutex); append ligne JSON; give(fsMutex).
    return true;
}

void storageTask(void* pv) {
    (void)pv;
    for (;;) {
        // TODO: xEventGroupWaitBits(netState, BIT_MQTT_OK, ...) puis replay
        //       buffer.jsonl -> outboundJsonQueue (ou republish direct), tronquer.
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
