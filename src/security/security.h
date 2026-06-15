// ============================================================================
//  security/ — Sécurité minimale (cf. cahier des charges §8).
//    - auth MQTT : user/pass fournis par network/ (secrets.h)
//    - validation JSON entrants : schéma / champs obligatoires / types
//    - protection API locale : token Bearer
//  Pas de tâche dédiée : fonctions utilitaires appelées par network/ et web/.
// ============================================================================
#pragma once

#include <Arduino.h>
#include "app_types.h"

// Vérifie le token d'un header "Authorization: Bearer <token>".
bool securityCheckToken(const String& authHeader);

// Valide + parse un JSON de commande actionneur. Retourne false si invalide.
bool securityParseActuatorCmd(const char* json, size_t len, ActuatorCmd& out);
