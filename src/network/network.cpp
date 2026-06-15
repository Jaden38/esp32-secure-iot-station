#include "network.h"
#include "config.h"
#include "rtos_shared.h"
#include "actuators/actuators.h"
#include "storage/storage.h"
#include "security/security.h"
#include "supervision/supervision.h"
#include "runtime_config.h"
#include "sensors/sensors.h"
#include "secrets.h"        // WIFI_SSID / WIFI_PASSWORD (le MQTT vient du NVS)
#include <WiFi.h>
#include <MQTT.h>          // 256dpi/arduino-mqtt (lwmqtt)
#include <ArduinoJson.h>
#include <time.h>

static WiFiClient  net;
static MQTTClient  mqtt(512);     // buffers R/W 512 o (payload imposé < 256)
static bool        s_ntpStarted = false;

// --- Callback commande MQTT entrante : validation -> queue actionneurs --------
static void onMqttMessage(String& topic, String& payload) {
    (void)topic;
    ActuatorCmd cmd;
    if (securityParseActuatorCmd(payload.c_str(), payload.length(), cmd)) {
        xQueueSend(actuatorCmdQueue, &cmd, 0);
    } else {
        log_w("[net] commande MQTT rejetée: %s", payload.c_str());
    }
}

// --- Wi-Fi : connexion + reconnexion auto (non bloquant) ---------------------
static void ensureWifi() {
    static bool wasUp = false;
    if (WiFi.status() == WL_CONNECTED) {
        if (!wasUp) {        // transition -> affiche l'IP (utile pour l'UI web)
            wasUp = true;
            log_i("[net] Wi-Fi OK — IP: %s (UI web: http://%s/)",
                  WiFi.localIP().toString().c_str(),
                  WiFi.localIP().toString().c_str());
        }
        xEventGroupSetBits(netState, BIT_WIFI_OK);
        return;
    }
    wasUp = false;
    xEventGroupClearBits(netState, BIT_WIFI_OK | BIT_MQTT_OK);

    static uint32_t last = 0;
    uint32_t now = millis();
    if (last != 0 && now - last < 5000) return;   // une tentative / 5 s
    last = now;
    log_i("[net] connexion Wi-Fi à %s...", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void startNtpOnce() {
    if (s_ntpStarted) return;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // epoch UTC
    s_ntpStarted = true;
}

// --- MQTT : connexion auth + LWT + souscription commandes --------------------
static void ensureMqtt() {
    // Config MQTT modifiée via l'UI -> on se déconnecte pour repartir dessus.
    if (runtimeMqttDirty()) {
        MqttConfig cfg = runtimeGetMqtt();
        mqtt.disconnect();
        mqtt.begin(cfg.host, cfg.port, net);
        runtimeClearMqttDirty();
        xEventGroupClearBits(netState, BIT_MQTT_OK);
        log_i("[net] config MQTT mise à jour -> %s:%u", cfg.host, cfg.port);
    }

    if (mqtt.connected()) {
        xEventGroupSetBits(netState, BIT_MQTT_OK);
        return;
    }
    xEventGroupClearBits(netState, BIT_MQTT_OK);

    static uint32_t last = 0;
    uint32_t now = millis();
    if (last != 0 && now - last < 3000) return;   // une tentative / 3 s
    last = now;

    MqttConfig cfg = runtimeGetMqtt();
    mqtt.setWill(MQTT_TOPIC_LWT, "offline", true, 1);    // LWT retained QoS1
    log_i("[net] connexion MQTT %s:%u...", cfg.host, cfg.port);
    if (mqtt.connect(DEVICE_ID, cfg.user, cfg.pass)) {
        mqtt.subscribe(MQTT_TOPIC_CMD, 1);               // commandes QoS1
        mqtt.publish(MQTT_TOPIC_LWT, "online", true, 1);
        xEventGroupSetBits(netState, BIT_MQTT_OK);
        log_i("[net] MQTT connecté");
    } else {
        log_w("[net] échec MQTT (err=%d rc=%d)", mqtt.lastError(), mqtt.returnCode());
    }
}

// --- Publication : vide outboundJsonQueue, bascule offline si échec ----------
static void pumpOutbound() {
    OutboundPayload p;
    while (xQueueReceive(outboundJsonQueue, &p, 0) == pdTRUE) {
        bool sent = false;
        if (mqtt.connected()) {
            uint32_t t0 = millis();
            sent = mqtt.publish(MQTT_TOPIC_DATA, p.json, false, 1);   // QoS1
            supervisionReportPublishLatency(millis() - t0);
        }
        if (!sent) {
            storageAppend(p);     // mode offline -> LittleFS (replay tâche #5)
        }
    }
}

bool networkInit() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    MqttConfig cfg = runtimeGetMqtt();
    mqtt.begin(cfg.host, cfg.port, net);
    mqtt.onMessage(onMqttMessage);
    mqtt.setKeepAlive(30);
    return true;
}

void networkTask(void* pv) {
    (void)pv;
    for (;;) {
        ensureWifi();
        if (WiFi.status() == WL_CONNECTED) {
            startNtpOnce();
            ensureMqtt();
        }
        mqtt.loop();
        pumpOutbound();                  // publie ce que produit la télémétrie
        vTaskDelay(pdMS_TO_TICKS(20));   // jamais delay()
    }
}

// --- Télémétrie : publie un instantané toutes les pubPeriodMs (découplé de
//     l'acquisition). Inclut l'état métier (estop/relais/mode) en plus du
//     format imposé {device, ts, data:{temp, humidity}}. -----------------------
void telemetryTask(void* pv) {
    (void)pv;
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        ControlConfig cfg = runtimeGetControl();
        SensorSample  s   = sensorsGetLatest();
        if (s.valid) {
            JsonDocument doc;
            doc["device"] = DEVICE_ID;
            doc["ts"]     = s.ts;
            JsonObject data = doc["data"].to<JsonObject>();
            data["temp"]     = s.temp;
            data["humidity"] = s.humidity;
            // Champs additionnels (n'altèrent pas les champs imposés)
            doc["estop"] = (bool)(xEventGroupGetBits(appState) & BIT_ESTOP);
            doc["relay"] = actuatorsGetState().relayOn;
            doc["mode"]  = (cfg.mode == MODE_MANUEL) ? "manuel" : "auto";

            OutboundPayload p{};
            p.ts = s.ts;
            serializeJson(doc, p.json, sizeof(p.json));
            if (xQueueSend(outboundJsonQueue, &p, 0) != pdTRUE) {
                OutboundPayload drop;
                if (xQueueReceive(outboundJsonQueue, &drop, 0) == pdTRUE)
                    xQueueSend(outboundJsonQueue, &p, 0);
            }
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(cfg.pubPeriodMs));
    }
}
