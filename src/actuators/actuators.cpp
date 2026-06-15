#include "actuators.h"
#include "config.h"
#include "rtos_shared.h"

// LED RGB pilotée par LEDC (API core 3.x : ledcAttach / ledcWrite).
// Relais sur GPIO simple, polarité configurable (RELAY_ACTIVE_LOW).

static ActuatorState s_state = {false, 0, 0, 0};

// Niveau GPIO à écrire pour obtenir l'état relais voulu, selon la polarité.
static inline void writeRelay(bool on) {
    digitalWrite(PIN_RELAY, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// Duty LEDC pour une LED (inversé si anode commune).
static inline void writeLed(int pin, uint8_t v) {
    ledcWrite(pin, LED_COMMON_ANODE ? (255 - v) : v);
}

bool actuatorsInit() {
    // Relais : forcer OFF AVANT tout (sécurité — évite le clic au boot).
    pinMode(PIN_RELAY, OUTPUT);
    writeRelay(false);

    // LED RGB : un canal LEDC auto-assigné par broche (core 3.x).
    bool ok = ledcAttach(PIN_LED_R, LED_PWM_FREQ, LED_PWM_RES_BITS) &&
              ledcAttach(PIN_LED_G, LED_PWM_FREQ, LED_PWM_RES_BITS) &&
              ledcAttach(PIN_LED_B, LED_PWM_FREQ, LED_PWM_RES_BITS);
    writeLed(PIN_LED_R, 0);
    writeLed(PIN_LED_G, 0);
    writeLed(PIN_LED_B, 0);

    s_state = {false, 0, 0, 0};
    return ok;
}

void actuatorsApply(const ActuatorCmd& cmd) {
    switch (cmd.type) {
        case ActuatorType::RELAY:
            writeRelay(cmd.relayOn);
            s_state.relayOn = cmd.relayOn;
            break;
        case ActuatorType::LED_RGB:
            writeLed(PIN_LED_R, cmd.r);
            writeLed(PIN_LED_G, cmd.g);
            writeLed(PIN_LED_B, cmd.b);
            s_state.r = cmd.r;
            s_state.g = cmd.g;
            s_state.b = cmd.b;
            break;
    }
}

ActuatorState actuatorsGetState() {
    return s_state;
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
