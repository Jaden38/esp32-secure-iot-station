#include "sensors.h"
#include "config.h"
#include "rtos_shared.h"
#include "runtime_config.h"
#include <DHTesp.h>
#include <math.h>
#include <time.h>

static DHTesp dht;

// Cache du dernier échantillon (fan-out vers web/ & supervision/).
static SensorSample      s_latest = {};
static SemaphoreHandle_t s_latestMutex = nullptr;

SensorSample sensorsGetLatest() {
    SensorSample copy = {};
    if (s_latestMutex && xSemaphoreTake(s_latestMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        copy = s_latest;
        xSemaphoreGive(s_latestMutex);
    }
    return copy;
}

// ===========================================================================
//  Contact 2 fils (ex-bouton) : interruption + anti-rebond logiciel.
//  Montage INPUT_PULLUP -> fils en contact (vers GND) = niveau BAS.
// ===========================================================================
static volatile bool     s_contactClosed = false;
static volatile uint32_t s_lastEdgeMs    = 0;
static volatile uint32_t s_contactEvents = 0;   // compteur d'événements (debug)

static void IRAM_ATTR contactIsr() {
    uint32_t now = millis();
    if (now - s_lastEdgeMs < CONTACT_DEBOUNCE_MS) return;   // rebond ignoré
    s_lastEdgeMs    = now;
    s_contactClosed = (digitalRead(PIN_CONTACT) == LOW);
    s_contactEvents++;
    // Contact fermé = appui sur l'arrêt d'urgence -> réveille la tâche Safety.
    if (s_contactClosed && estopSem) {
        BaseType_t hpw = pdFALSE;
        xSemaphoreGiveFromISR(estopSem, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}

bool sensorsContactClosed() {
    return s_contactClosed;
}

// ===========================================================================
//  Filtrage : moyenne glissante sur FILTER_WINDOW. N'absorbe que les lectures
//  valides (les valeurs aberrantes ne polluent pas le filtre).
// ===========================================================================
static float  s_tBuf[FILTER_WINDOW] = {0};
static float  s_hBuf[FILTER_WINDOW] = {0};
static size_t s_fCount = 0;
static size_t s_fIdx   = 0;

static void filterPush(float t, float h) {
    s_tBuf[s_fIdx] = t;
    s_hBuf[s_fIdx] = h;
    s_fIdx = (s_fIdx + 1) % FILTER_WINDOW;
    if (s_fCount < FILTER_WINDOW) s_fCount++;
}

static float filterAvg(const float* buf) {
    if (s_fCount == 0) return NAN;
    float sum = 0;
    for (size_t i = 0; i < s_fCount; i++) sum += buf[i];
    return sum / s_fCount;
}

static inline float round1(float v) { return roundf(v * 10.0f) / 10.0f; }

// Timestamp : epoch si l'horloge est synchronisée (NTP via module network),
// sinon repli sur l'uptime en secondes.
static uint32_t nowTs() {
    time_t now = time(nullptr);
    return (now > 1700000000) ? (uint32_t)now : (uint32_t)(millis() / 1000);
}

// Envoi non bloquant avec éviction du plus ancien si la queue est pleine
// (on privilégie la donnée fraîche pour le "live").
template <typename T>
static void queueSendFresh(QueueHandle_t q, const T* item) {
    if (xQueueSend(q, item, 0) != pdTRUE) {
        T dropped;
        if (xQueueReceive(q, &dropped, 0) == pdTRUE) xQueueSend(q, item, 0);
    }
}

bool sensorsInit() {
    s_latestMutex = xSemaphoreCreateMutex();
    dht.setup(PIN_DHT, DHTesp::DHT22);
    pinMode(PIN_CONTACT, INPUT_PULLUP);
    s_contactClosed = (digitalRead(PIN_CONTACT) == LOW);
    attachInterrupt(digitalPinToInterrupt(PIN_CONTACT), contactIsr, CHANGE);
    return true;
}

void sensorTask(void* pv) {
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        // 1) Lecture DHT22
        TempAndHumidity th = dht.getTempAndHumidity();
        const bool dhtOk = (dht.getStatus() == DHTesp::ERROR_NONE) &&
                           !isnan(th.temperature) && !isnan(th.humidity);

        // 2) Détection des valeurs aberrantes (bornes config.h)
        const bool inRange = dhtOk &&
            th.temperature >= TEMP_MIN && th.temperature <= TEMP_MAX &&
            th.humidity    >= HUM_MIN  && th.humidity    <= HUM_MAX;

        // 3) Filtrage (seulement si valide)
        if (inRange) filterPush(th.temperature, th.humidity);

        // 4) Composition de l'échantillon (timestamp + seuil + contact)
        SensorSample s{};
        s.ts        = nowTs();
        s.temp      = round1(filterAvg(s_tBuf));
        s.humidity  = round1(filterAvg(s_hBuf));
        s.contact   = s_contactClosed;
        s.valid     = inRange && (s_fCount > 0);

        // Met à jour le cache "live" (lu par web/ & supervision/)
        if (s_latestMutex && xSemaphoreTake(s_latestMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            s_latest = s;
            xSemaphoreGive(s_latestMutex);
        }

        queueSendFresh(sensorDataQueue, &s);

        // La publication MQTT est désormais assurée par telemetryTask (réseau),
        // découplée du rythme d'acquisition (cf. pubPeriodMs).
        if (!s.valid) {
            log_w("[sensors] lecture invalide (status=%s)", dht.getStatusString());
        }

        // Période d'acquisition configurable à chaud (>= 2 s pour le DHT22)
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(runtimeGetControl().acqPeriodMs));
    }
}
