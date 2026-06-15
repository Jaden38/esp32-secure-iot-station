#include "network.h"
#include "config.h"
#include "rtos_shared.h"
#include "actuators/actuators.h"
#include "storage/storage.h"
#include "security/security.h"
#include "supervision/supervision.h"
#include "secrets.h"
#include <WiFi.h>
#include <MQTT.h>          // 256dpi/arduino-mqtt (lwmqtt)
#include <time.h>

// TODO(tâche #6) : surcharger host/port/user/pass depuis runtime_config (NVS).

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
    if (WiFi.status() == WL_CONNECTED) {
        xEventGroupSetBits(netState, BIT_WIFI_OK);
        return;
    }
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
    if (mqtt.connected()) {
        xEventGroupSetBits(netState, BIT_MQTT_OK);
        return;
    }
    xEventGroupClearBits(netState, BIT_MQTT_OK);

    static uint32_t last = 0;
    uint32_t now = millis();
    if (last != 0 && now - last < 3000) return;   // une tentative / 3 s
    last = now;

    mqtt.setWill(MQTT_TOPIC_LWT, "offline", true, 1);    // LWT retained QoS1
    log_i("[net] connexion MQTT %s:%d...", MQTT_HOST, MQTT_PORT);
    if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS)) {
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
    mqtt.begin(MQTT_HOST, MQTT_PORT, net);
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
        pumpOutbound();
        actuatorsDrainQueue(0);          // commandes web + MQTT-in
        vTaskDelay(pdMS_TO_TICKS(20));   // jamais delay()
    }
}
