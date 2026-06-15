// ============================================================================
//  actuators/ — LED RGB (PWM LEDC) + relais 5V (GPIO).
//  Handler SYNCHRONE : pas de tâche dédiée (cf. README, 5 tâches figées).
//  Les commandes sont produites par web/ et network/ dans actuatorCmdQueue,
//  puis drainées via actuatorsDrainQueue() (appelé depuis networkTask).
//  Choix : LEDC (ledcAttach core 3.x) pour la LED RGB.
// ============================================================================
#pragma once

#include "app_types.h"
#include "freertos/FreeRTOS.h"

// État courant des actionneurs (pour web/supervision).
struct ActuatorState {
    bool    relayOn;
    uint8_t r, g, b;
};

// Init GPIO relais (OFF forcé dès le boot) + canaux LEDC. À appeler dans setup().
bool actuatorsInit();

// Applique immédiatement une commande (LED RGB ou relais).
void actuatorsApply(const ActuatorCmd& cmd);

// Snapshot de l'état courant (thread-safe : champs scalaires).
ActuatorState actuatorsGetState();

// Vide actuatorCmdQueue (non bloquant si wait==0) et applique chaque commande.
void actuatorsDrainQueue(TickType_t wait);
