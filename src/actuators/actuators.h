// ============================================================================
//  actuators/ — LEDs d'état (PWM LEDC) + relais 5V (GPIO).
//  Couche SYNCHRONE sans tâche dédiée : actuatorsApply() écrit directement le
//  matériel. Les voyants sont pilotés par control/ ; le relais par control/
//  (régulation) ou via une commande validée (web/MQTT -> actuatorCmdQueue).
//  Choix : LEDC (ledcAttach core 3.x) pour les LEDs.
// ============================================================================
#pragma once

#include "app_types.h"

// État courant des actionneurs (pour web/supervision).
struct ActuatorState {
    bool    relayOn;
    uint8_t r, g, b;
};

// Init GPIO relais (OFF forcé dès le boot) + canaux LEDC. À appeler dans setup().
bool actuatorsInit();

// Applique immédiatement une commande (LED ou relais).
void actuatorsApply(const ActuatorCmd& cmd);

// Snapshot de l'état courant (thread-safe : champs scalaires).
ActuatorState actuatorsGetState();
