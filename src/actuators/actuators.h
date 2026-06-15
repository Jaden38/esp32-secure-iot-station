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

// Init GPIO relais + canaux LEDC. À appeler dans setup().
bool actuatorsInit();

// Applique immédiatement une commande (LED RGB ou relais).
void actuatorsApply(const ActuatorCmd& cmd);

// Vide actuatorCmdQueue (non bloquant si wait==0) et applique chaque commande.
void actuatorsDrainQueue(TickType_t wait);
