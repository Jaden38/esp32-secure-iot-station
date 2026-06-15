// ============================================================================
//  control/ — Logique métier (supervision armoire technique).
//  Deux opérations FreeRTOS isolées :
//    - controlTask  : régulation ventilation (relais) par hystérésis + LED état,
//                     drain des commandes manuelles (web/MQTT), mode AUTO/MANUEL.
//    - safetyTask   : arrêt d'urgence (contact) — réveil par sémaphore (IRQ),
//                     coupe le relais (état sûr) + LED rouge + bit BIT_ESTOP.
//  Toute la config (seuils, périodes, mode) vient de runtime_config (NVS).
// ============================================================================
#pragma once

bool controlInit();

// Tâche régulation (CORE_SENSORS, priorité moyenne).
void controlTask(void* pv);

// Tâche arrêt d'urgence (priorité max, événementielle).
void safetyTask(void* pv);

// Réarme l'arrêt d'urgence (appelé par l'UI web).
void controlResetEstop();
