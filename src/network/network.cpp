#include "network.h"
#include "config.h"
#include "rtos_shared.h"
#include "actuators/actuators.h"

// TODO(tâche #4) : WiFi.h, PubSubClient, callback MQTT-in -> security::validate
// -> actuatorCmdQueue, mesure latence de publication (OutboundPayload.ts).

bool networkInit() {
    // TODO: WiFi.mode(WIFI_STA); config PubSubClient (server/port/callback).
    return true;
}

void networkTask(void* pv) {
    (void)pv;
    for (;;) {
        // TODO: assurer Wi-Fi + MQTT (reconnexion), set/clear BIT_WIFI_OK/MQTT_OK.
        //       if MQTT_OK : drainer outboundJsonQueue -> publish QoS1
        //                    sinon -> storage (buffer offline).
        //       mqttClient.loop();
        actuatorsDrainQueue(0);          // commandes web + MQTT-in
        vTaskDelay(pdMS_TO_TICKS(50));   // jamais delay() — vTaskDelay only
    }
}
