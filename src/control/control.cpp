#include "control.h"
#include "config.h"
#include "rtos_shared.h"
#include "runtime_config.h"
#include "sensors/sensors.h"
#include "actuators/actuators.h"

// Couleurs LED d'état (R,G,B 0..255)
static void led(uint8_t r, uint8_t g, uint8_t b) {
    ActuatorCmd c{};
    c.type = ActuatorType::LED_RGB;
    c.r = r; c.g = g; c.b = b;
    actuatorsApply(c);
}
static void relay(bool on) {
    ActuatorCmd c{};
    c.type = ActuatorType::RELAY;
    c.relayOn = on;
    actuatorsApply(c);
}

bool controlInit() {
    return true;
}

void controlResetEstop() {
    xEventGroupClearBits(appState, BIT_ESTOP);
}

// --- Régulation : relais (hystérésis) + LED d'état + commandes manuelles -----
void controlTask(void* pv) {
    (void)pv;
    bool relayState = false;       // mémoire pour l'hystérésis
    bool blink = false;

    for (;;) {
        ControlConfig cfg = runtimeGetControl();
        SensorSample  s   = sensorsGetLatest();
        const bool estop  = xEventGroupGetBits(appState) & BIT_ESTOP;

        // 1) Commandes manuelles (web/MQTT) : un ordre relais bascule en MANUEL
        ActuatorCmd cmd;
        while (xQueueReceive(actuatorCmdQueue, &cmd, 0) == pdTRUE) {
            if (cmd.type == ActuatorType::RELAY && !estop) {
                cfg.mode = MODE_MANUEL;
                cfg.relayManual = cmd.relayOn;
                runtimeSetControl(cfg);     // persiste + reflète dans l'UI
            }
            // Les commandes LED sont ignorées : la LED est un indicateur d'état.
        }

        // 2) Décision relais + couleur LED (priorité : estop > manuel > auto)
        if (estop) {
            relayState = false;
            relay(false);
            blink = !blink;
            led(blink ? 255 : 0, 0, 0);          // rouge clignotant
        } else if (cfg.mode == MODE_MANUEL) {
            relayState = cfg.relayManual;
            relay(relayState);
            led(0, 0, 255);                       // bleu = mode manuel
        } else {                                  // MODE_AUTO
            if (!s.valid) {
                led(255, 0, 0);                   // rouge = capteur en défaut
            } else {
                if (s.temp >= cfg.tempOn)                  relayState = true;
                else if (s.temp <= cfg.tempOn - cfg.hysteresis) relayState = false;
                // sinon : on garde l'état (bande d'hystérésis)
                relay(relayState);

                if (s.humidity > cfg.humAlert) led(255, 60, 0);   // orange humidité
                else if (relayState)           led(255, 100, 0);  // orange ventilation
                else                           led(0, 255, 0);    // vert nominal
            }
        }

        vTaskDelay(pdMS_TO_TICKS(cfg.ctrlPeriodMs));
    }
}

// --- Arrêt d'urgence : réveil par l'ISR du contact --------------------------
void safetyTask(void* pv) {
    (void)pv;
    for (;;) {
        if (xSemaphoreTake(estopSem, portMAX_DELAY) != pdTRUE) continue;

        // État sûr immédiat (priorité max) : coupe le relais + signale l'estop.
        xEventGroupSetBits(appState, BIT_ESTOP);
        relay(false);
        log_w("[safety] ARRET D'URGENCE -> relais coupé");

        // Réarmement : automatique (contact relâché) ou manuel (via l'UI web).
        if (runtimeGetControl().estopAutoReset) {
            while (sensorsContactClosed()) vTaskDelay(pdMS_TO_TICKS(100));
            controlResetEstop();
            log_i("[safety] contact relâché -> arrêt d'urgence réarmé");
        }
    }
}
