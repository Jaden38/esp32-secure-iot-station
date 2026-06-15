#include "web.h"
#include "config.h"
#include "rtos_shared.h"

// TODO(tâche #6) : AsyncWebServer server(80); routes statiques LittleFS,
// /api/live, /api/state, /api/config (GET/POST NVS), /api/actuator (POST ->
// actuatorCmdQueue), middleware token (security::checkToken).
// Substituts matériels :
//   /api/threshold (GET/POST) -> runtimeGet/SetThreshold (ex-potentiomètre)
//   /api/supervision (GET)    -> supervisionCopyFrame (OLED virtuel)

bool webInit() {
    // TODO: server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    //       déclarer les routes /api/* ; server.begin();
    return true;
}

void webTask(void* pv) {
    (void)pv;
    SensorSample latest{};
    for (;;) {
        // Met à jour le cache "live" lu par /api/live (peek non destructif).
        if (xQueuePeek(sensorDataQueue, &latest, 0) == pdTRUE) {
            // TODO: stocker latest dans un cache protégé pour les handlers /api.
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
