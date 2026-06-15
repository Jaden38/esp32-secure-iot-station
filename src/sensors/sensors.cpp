#include "sensors.h"
#include "config.h"
#include "rtos_shared.h"

// TODO(tâche #2) : #include <DHTesp.h>, IRQ contact, buffers de moyenne
// glissante, sanity-check, sérialisation JSON imposée.
#include "runtime_config.h"

bool sensorsInit() {
    // TODO: dht.setup(PIN_DHT, DHTesp::DHT22);
    //       pinMode(PIN_CONTACT, INPUT_PULLUP) + attachInterrupt.
    return true;
}

void sensorTask(void* pv) {
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        // TODO: lire DHT22 (temp/hum), contact, recopier runtimeGetThreshold()
        //       dans sample.threshold, filtrer, timestamp, sanity-check,
        //       xQueueSend(sensorDataQueue, ...) puis formatter le JSON imposé
        //       et xQueueSend(outboundJsonQueue, ...).
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}
