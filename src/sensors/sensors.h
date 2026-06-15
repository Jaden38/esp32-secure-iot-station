// ============================================================================
//  sensors/ — Acquisition DHT22 + contact (2 fils, IRQ) + seuil web.
//  Responsabilités (cf. CLAUDE.md §3) :
//    - lecture DHT22 (1-wire sur PIN_DHT — pas d'I²C, donc pas d'i2cMutex)
//    - état contact (GPIO + IRQ) + seuil courant via runtimeGetThreshold()
//      (potentiomètre absent -> seuil réglé depuis l'UI web)
//    - filtrage (moyenne glissante, FILTER_WINDOW)
//    - timestamp + détection valeurs aberrantes (bornes config.h)
//    - push SensorSample -> sensorDataQueue
//    - format JSON imposé -> outboundJsonQueue
//
//  Choix lib (figé tâche #2) : DHTesp (beegee-tokyo). Comparée à l'approche RMT
//  non-bloquante (dhtESP32-rmt / esp32DHT) : DHTesp est éprouvée et son unique
//  défaut — une section critique ~5 ms (portENTER_CRITICAL) — est neutralisé ici
//  car l'acquisition est épinglée sur le Cœur 0, isolée de la stack WiFi (Cœur 1).
// ============================================================================
#pragma once

#include "app_types.h"

// Init matériel capteurs (DHT22 + contact IRQ). À appeler dans setup().
bool sensorsInit();

// Tâche FreeRTOS d'acquisition périodique (CORE_SENSORS, PRIO_SENSORS).
void sensorTask(void* pv);

// Dernier échantillon (cache thread-safe) — pour web/ & supervision/ (fan-out).
SensorSample sensorsGetLatest();
