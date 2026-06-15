#include "supervision.h"
#include "config.h"
#include "rtos_shared.h"
#include <string.h>

// TODO(tâche #8) : composer les lignes (heap/uptime/latence/état réseau),
// rendre via U8g2 si HAS_OLED, mettre à jour s_frame pour l'OLED virtuel web,
// high-water-mark des piles.
#if HAS_OLED
  // #include <U8g2lib.h>   // pilote SSD1306 réel (à activer en tâche #8)
#endif

static volatile uint32_t s_lastPublishLatencyMs = 0;

// Trame partagée (snapshot texte de l'écran), protégée par mutex dédié.
static char s_frame[SUP_LINES * (SUP_COLS + 1)] = {0};
static SemaphoreHandle_t s_frameMutex = nullptr;

bool supervisionInit() {
    s_frameMutex = xSemaphoreCreateMutex();
#if HAS_OLED
    // TODO: u8g2.begin() sous i2cMutex.
#endif
    return s_frameMutex != nullptr;
}

void supervisionReportPublishLatency(uint32_t ms) {
    s_lastPublishLatencyMs = ms;
}

size_t supervisionCopyFrame(char* out, size_t maxLen) {
    if (!out || maxLen == 0) return 0;
    size_t n = 0;
    if (s_frameMutex && xSemaphoreTake(s_frameMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        n = strlcpy(out, s_frame, maxLen);
        xSemaphoreGive(s_frameMutex);
    }
    return (n < maxLen) ? n : maxLen - 1;
}

void supervisionTask(void* pv) {
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        // TODO: composer la trame :
        //   heap = ESP.getFreeHeap(), uptime = millis()/1000,
        //   latence = s_lastPublishLatencyMs, état via netState.
        //   -> Serial.print(...) ; si HAS_OLED -> U8g2 (i2cMutex) ;
        //   -> remplir s_frame sous s_frameMutex (OLED virtuel web).
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SUPERVISION_PERIOD_MS));
    }
}
