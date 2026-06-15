// ============================================================================
//  rtos_shared.h — Primitives FreeRTOS partagées entre modules.
//  Déclarées ici, définies dans rtos_shared.cpp, créées par rtosInit().
//  Tout accès I²C passe par i2cMutex ; tout accès FS par fsMutex.
// ============================================================================
#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "app_types.h"

// --- Queues -----------------------------------------------------------------
extern QueueHandle_t sensorDataQueue;    // SensorSample   x Q_SENSOR_LEN
extern QueueHandle_t actuatorCmdQueue;   // ActuatorCmd    x Q_CMD_LEN
extern QueueHandle_t outboundJsonQueue;  // OutboundPayload x Q_OUTBOUND_LEN

// --- Mutex (bus partagés) ---------------------------------------------------
extern SemaphoreHandle_t i2cMutex;       // OLED (seul périph. I²C)
extern SemaphoreHandle_t fsMutex;        // LittleFS (storage + web)

// --- EventGroup état réseau -------------------------------------------------
extern EventGroupHandle_t netState;
static constexpr EventBits_t BIT_WIFI_OK = (1 << 0);
static constexpr EventBits_t BIT_MQTT_OK = (1 << 1);

// --- État applicatif + arrêt d'urgence --------------------------------------
extern EventGroupHandle_t appState;
static constexpr EventBits_t BIT_ESTOP = (1 << 0);   // arrêt d'urgence actif

// Sémaphore donné par l'ISR du contact -> réveille la tâche Safety (estop).
extern SemaphoreHandle_t estopSem;

// Crée toutes les primitives ci-dessus. À appeler dans setup() AVANT les tâches.
// Retourne false si une allocation échoue (heap insuffisant).
bool rtosInit();
