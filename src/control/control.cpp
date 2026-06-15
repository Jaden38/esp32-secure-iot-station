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

// 3 LEDs DISCRÈTES (pas de mélange RGB), ordre physique rouge/orange/verte :
//   canal R = rouge (GPIO25), G = orange (GPIO26), B = verte (GPIO33).
// Une seule allumée à la fois.
enum Status : uint8_t { ST_OFF, ST_GREEN, ST_ORANGE, ST_RED };
static void statusLed(Status s) {
    led(s == ST_RED    ? 255 : 0,   // R -> GPIO25 (rouge)
        s == ST_ORANGE ? 255 : 0,   // G -> GPIO26 (orange)
        s == ST_GREEN  ? 255 : 0);  // B -> GPIO33 (verte)
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
            statusLed(blink ? ST_RED : ST_OFF);   // rouge clignotant
        } else if (cfg.mode == MODE_MANUEL) {
            relayState = cfg.relayManual;
            relay(relayState);
            statusLed(ST_ORANGE);                 // orange = mode manuel
        } else {                                  // MODE_AUTO
            if (!s.valid) {
                statusLed(ST_RED);                // rouge = capteur en défaut
            } else {
                if (s.temp >= cfg.tempOn)                  relayState = true;
                else if (s.temp <= cfg.tempOn - cfg.hysteresis) relayState = false;
                // sinon : on garde l'état (bande d'hystérésis)
                relay(relayState);

                // orange si ventilation active ou humidité haute, sinon vert
                if (relayState || s.humidity > cfg.humAlert) statusLed(ST_ORANGE);
                else                                         statusLed(ST_GREEN);
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
