#include "actuators.h"
#include "config.h"
#include "rtos_shared.h"

// TODO(tâche #4 actionneurs) : ledcAttach(PIN_LED_*, freq, res), pinMode relais.

bool actuatorsInit() {
    // TODO: pinMode(PIN_RELAY, OUTPUT); ledcAttach(PIN_LED_R/G/B, 5000, 8);
    return true;
}

void actuatorsApply(const ActuatorCmd& cmd) {
    (void)cmd;
    // TODO: switch(cmd.type) { RELAY -> digitalWrite ; LED_RGB -> ledcWrite }
}

void actuatorsDrainQueue(TickType_t wait) {
    ActuatorCmd cmd;
    if (xQueueReceive(actuatorCmdQueue, &cmd, wait) == pdTRUE) {
        actuatorsApply(cmd);
        // draine le reste sans attendre
        while (xQueueReceive(actuatorCmdQueue, &cmd, 0) == pdTRUE) {
            actuatorsApply(cmd);
        }
    }
}
