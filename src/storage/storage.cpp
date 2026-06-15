#include "storage.h"
#include "config.h"
#include "rtos_shared.h"
#include <LittleFS.h>

// Buffer offline (1 ligne JSON = 1 message). En cours de replay, on renomme
// d'abord en .replay : les nouveaux messages vont dans un buffer neuf, ce qui
// évite de perdre des données et limite les doublons en cas de coupure.
static const char* BUFFER_PATH = "/buffer.jsonl";
static const char* REPLAY_PATH = "/buffer.replay";

// --- Compaction : ne garde que les STORAGE_KEEP_BYTES les plus récents -------
// Appelée sous fsMutex quand le buffer dépasse STORAGE_MAX_BYTES.
static void compactLocked() {
    File in = LittleFS.open(BUFFER_PATH, "r");
    if (!in) return;
    size_t size = in.size();
    if (size <= STORAGE_MAX_BYTES) { in.close(); return; }

    in.seek(size - STORAGE_KEEP_BYTES);
    in.readStringUntil('\n');                 // jette la 1re ligne partielle

    File out = LittleFS.open("/buffer.tmp", "w");
    if (!out) { in.close(); return; }
    uint8_t buf[512];
    size_t n;
    while ((n = in.read(buf, sizeof(buf))) > 0) out.write(buf, n);
    in.close();
    out.close();

    LittleFS.remove(BUFFER_PATH);
    LittleFS.rename("/buffer.tmp", BUFFER_PATH);
    log_w("[storage] compaction buffer offline (%u -> ~%u o)",
          (unsigned)size, (unsigned)STORAGE_KEEP_BYTES);
}

bool storageInit() {
    if (!LittleFS.begin(true)) {               // formate si le montage échoue
        log_e("[storage] LittleFS.begin a échoué");
        return false;
    }
    log_i("[storage] LittleFS monté (%u/%u o)",
          (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    return true;
}

bool storageAppend(const OutboundPayload& p) {
    if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(200)) != pdTRUE)
        return false;

    bool ok = false;
    File f = LittleFS.open(BUFFER_PATH, "a");
    if (f) {
        f.println(p.json);                     // ligne JSON + '\n'
        size_t size = f.size();
        f.close();
        if (size > STORAGE_MAX_BYTES) compactLocked();
        ok = true;
        log_w("[storage] MQTT hors-ligne -> mesure bufferisée (buffer ~%u o)",
              (unsigned)size);
    }
    xSemaphoreGive(fsMutex);
    return ok;
}

// --- Replay : re-pousse le buffer vers outboundJsonQueue (publié par network) -
static void replayBuffer() {
    if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(500)) != pdTRUE)
        return;

    // Bascule le buffer courant en .replay (sauf si un replay est déjà en cours).
    if (!LittleFS.exists(REPLAY_PATH)) {
        if (!LittleFS.exists(BUFFER_PATH)) { xSemaphoreGive(fsMutex); return; }
        LittleFS.rename(BUFFER_PATH, REPLAY_PATH);
    }

    File f = LittleFS.open(REPLAY_PATH, "r");
    if (!f) { xSemaphoreGive(fsMutex); return; }

    bool reachedEnd = true;
    int  n = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        OutboundPayload p{};
        strlcpy(p.json, line.c_str(), sizeof(p.json));
        // Backpressure : attend un peu si la queue est pleine ; sinon on garde
        // le reste pour le prochain passage (réenvoi piloté par MQTT_OK).
        if (xQueueSend(outboundJsonQueue, &p, pdMS_TO_TICKS(1000)) != pdTRUE) {
            reachedEnd = false;
            break;
        }
        n++;
    }
    f.close();
    if (reachedEnd) LittleFS.remove(REPLAY_PATH);   // tout réinjecté

    if (n > 0) {
        log_i("[storage] reconnecté -> replay de %d mesure(s)%s", n,
              reachedEnd ? "" : " (partiel, reste à rejouer)");
    }
    xSemaphoreGive(fsMutex);
}

void storageTask(void* pv) {
    (void)pv;
    for (;;) {
        // Bloque tant que MQTT n'est pas disponible (sans consommer le bit).
        xEventGroupWaitBits(netState, BIT_MQTT_OK, pdFALSE, pdTRUE, portMAX_DELAY);
        replayBuffer();
        vTaskDelay(pdMS_TO_TICKS(2000));     // cadence de replay
    }
}
