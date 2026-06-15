// ============================================================================
//  main.cpp — Point d'entrée. AUCUNE logique métier ici (cf. CLAUDE.md §1).
//  Rôle : init matériel + primitives FreeRTOS + création des 5 tâches épinglées.
//  loop() reste vide (le travail vit dans les tâches).
//
//  Répartition (cf. README) :
//    Cœur 0 : SensorAcquisition (prio 3), Supervision (prio 1)
//    Cœur 1 : NetworkMQTT (prio 4), WebServer (prio 2), StorageReplay (prio 2)
// ============================================================================
#include <Arduino.h>

#include "config.h"
#include "rtos_shared.h"
#include "runtime_config.h"
#include "sensors/sensors.h"
#include "actuators/actuators.h"
#include "control/control.h"
#include "network/network.h"
#include "storage/storage.h"
#include "web/web.h"
#include "supervision/supervision.h"

static void halt(const char* why) {
    Serial.printf("[FATAL] %s — système arrêté.\n", why);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void setup() {
    Serial.begin(115200);
    delay(50);  // unique tolérance : stabilisation UART avant scheduler (setup,
                // hors logique métier). Tout le reste utilise vTaskDelay().
    Serial.println("\n=== esp32-secure-iot-station — boot ===");

    if (!rtosInit())        halt("rtosInit (heap insuffisant ?)");

    runtimeConfigInit();    // seuil (ex-pot) + config MQTT depuis NVS

    // Init matériel / modules (avant de lancer les tâches qui les utilisent).
    if (!sensorsInit())     halt("sensorsInit");
    if (!actuatorsInit())   halt("actuatorsInit");
    if (!controlInit())     halt("controlInit");
    if (!storageInit())     halt("storageInit");
    if (!networkInit())     halt("networkInit");
    if (!webInit())         halt("webInit");
    if (!supervisionInit()) halt("supervisionInit");

    // --- Création des tâches FreeRTOS épinglées (cf. CLAUDE.md §2) ---
    xTaskCreatePinnedToCore(sensorTask,      "SensorAcq",   STACK_SENSORS,
                            nullptr, PRIO_SENSORS,     nullptr, CORE_SENSORS);
    xTaskCreatePinnedToCore(supervisionTask, "Supervision", STACK_SUPERVISION,
                            nullptr, PRIO_SUPERVISION, nullptr, CORE_SUPERVISION);
    xTaskCreatePinnedToCore(networkTask,     "NetworkMQTT", STACK_NETWORK,
                            nullptr, PRIO_NETWORK,     nullptr, CORE_NETWORK);
    xTaskCreatePinnedToCore(webTask,         "WebServer",   STACK_WEB,
                            nullptr, PRIO_WEB,         nullptr, CORE_WEB);
    xTaskCreatePinnedToCore(storageTask,     "StorageRepl", STACK_STORAGE,
                            nullptr, PRIO_STORAGE,     nullptr, CORE_STORAGE);

    // --- Opérations métier (régulation, arrêt d'urgence, télémétrie) ---
    xTaskCreatePinnedToCore(controlTask,     "Control",     STACK_CONTROL,
                            nullptr, PRIO_CONTROL,     nullptr, CORE_CONTROL);
    xTaskCreatePinnedToCore(safetyTask,      "Safety",      STACK_SAFETY,
                            nullptr, PRIO_SAFETY,      nullptr, CORE_SAFETY);
    xTaskCreatePinnedToCore(telemetryTask,   "Telemetry",   STACK_TELEMETRY,
                            nullptr, PRIO_TELEMETRY,   nullptr, CORE_TELEMETRY);

    Serial.println("=== tâches lancées ===");
}

void loop() {
    // Vide par conception : toute la logique vit dans les tâches FreeRTOS.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
