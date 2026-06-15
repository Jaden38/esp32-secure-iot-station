#include "supervision.h"
#include "config.h"
#include "rtos_shared.h"
#include "runtime_config.h"
#include "sensors/sensors.h"
#include <string.h>
#include <stdio.h>

#if HAS_OLED
  #include <Wire.h>
  #include <U8g2lib.h>
  static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

static volatile uint32_t s_lastPublishLatencyMs = 0;

// Trame partagée (snapshot texte de l'écran), protégée par mutex dédié.
static char s_frame[SUP_LINES * (SUP_COLS + 1)] = {0};
static SemaphoreHandle_t s_frameMutex = nullptr;

bool supervisionInit() {
    s_frameMutex = xSemaphoreCreateMutex();
#if HAS_OLED
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
        u8g2.setI2CAddress(OLED_ADDR << 1);
        u8g2.begin();
        xSemaphoreGive(i2cMutex);
    }
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
        SensorSample s   = sensorsGetLatest();
        EventBits_t bits = xEventGroupGetBits(netState);
        const bool wifi  = bits & BIT_WIFI_OK;
        const bool mqtt  = bits & BIT_MQTT_OK;
        const unsigned long up     = millis() / 1000;
        const unsigned long heapKB = ESP.getFreeHeap() / 1024;

        // Composition de la trame (≤ SUP_COLS colonnes par ligne)
        char lines[SUP_LINES][SUP_COLS + 1];
        snprintf(lines[0], sizeof(lines[0]), "%-9s up:%lus", DEVICE_ID, up);
        snprintf(lines[1], sizeof(lines[1]), "Heap: %lu KB", heapKB);
        snprintf(lines[2], sizeof(lines[2]), "WiFi:%s  MQTT:%s",
                 wifi ? "OK" : "KO", mqtt ? "OK" : "KO");
        snprintf(lines[3], sizeof(lines[3]), "Pub lat: %lu ms",
                 (unsigned long)s_lastPublishLatencyMs);
        snprintf(lines[4], sizeof(lines[4]), "T:%.1f H:%.1f", s.temp, s.humidity);
        const bool estop = xEventGroupGetBits(appState) & BIT_ESTOP;
        snprintf(lines[5], sizeof(lines[5]), "Set:%.0fC %s",
                 runtimeGetControl().tempOn,
                 estop ? "*ESTOP*" : (s.contact ? "contact" : "ok"));

        // 1) Sortie série
        Serial.printf("[sup] up=%lus heap=%luKB %s lat=%lums T=%.1f H=%.1f\n",
                      up, heapKB,
                      mqtt ? "MQTT:OK" : (wifi ? "MQTT:KO" : "WiFi:KO"),
                      (unsigned long)s_lastPublishLatencyMs, s.temp, s.humidity);

        // 2) Trame pour l'OLED virtuel (web /api/supervision)
        char frame[sizeof(s_frame)];
        size_t pos = 0;
        for (int i = 0; i < SUP_LINES && pos < sizeof(frame) - 1; i++) {
            pos += snprintf(frame + pos, sizeof(frame) - pos, "%s\n", lines[i]);
        }
        if (s_frameMutex && xSemaphoreTake(s_frameMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            strlcpy(s_frame, frame, sizeof(s_frame));
            xSemaphoreGive(s_frameMutex);
        }

        // 3) OLED physique (si présent) — bus I²C protégé
#if HAS_OLED
        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tf);
            for (int i = 0; i < SUP_LINES; i++) u8g2.drawStr(0, 10 + i * 10, lines[i]);
            u8g2.sendBuffer();
            xSemaphoreGive(i2cMutex);
        }
#endif

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SUPERVISION_PERIOD_MS));
    }
}
