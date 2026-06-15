#include "rtos_shared.h"
#include "config.h"

QueueHandle_t sensorDataQueue   = nullptr;
QueueHandle_t actuatorCmdQueue  = nullptr;
QueueHandle_t outboundJsonQueue = nullptr;

SemaphoreHandle_t i2cMutex = nullptr;
SemaphoreHandle_t fsMutex  = nullptr;

EventGroupHandle_t netState = nullptr;
EventGroupHandle_t appState = nullptr;
SemaphoreHandle_t  estopSem = nullptr;

bool rtosInit() {
    sensorDataQueue   = xQueueCreate(Q_SENSOR_LEN,   sizeof(SensorSample));
    actuatorCmdQueue  = xQueueCreate(Q_CMD_LEN,      sizeof(ActuatorCmd));
    outboundJsonQueue = xQueueCreate(Q_OUTBOUND_LEN, sizeof(OutboundPayload));

    i2cMutex = xSemaphoreCreateMutex();
    fsMutex  = xSemaphoreCreateMutex();

    netState = xEventGroupCreate();
    appState = xEventGroupCreate();
    estopSem = xSemaphoreCreateBinary();

    return sensorDataQueue && actuatorCmdQueue && outboundJsonQueue &&
           i2cMutex && fsMutex && netState && appState && estopSem;
}
