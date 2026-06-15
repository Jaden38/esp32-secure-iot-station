#include "web.h"
#include "config.h"
#include "rtos_shared.h"
#include "runtime_config.h"
#include "sensors/sensors.h"
#include "actuators/actuators.h"
#include "security/security.h"
#include "supervision/supervision.h"
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// AsyncWebServer : piloté par callbacks (tâche AsyncTCP). Les fichiers de l'UI
// sont servis depuis LittleFS (/data uploadé via `pio run -t uploadfs`).
// Les routes /api/* sont protégées par token Bearer (cf. security/).
static AsyncWebServer server(80);

// Vérifie le token ; émet 401 et renvoie false si absent/invalide.
static bool authed(AsyncWebServerRequest* req) {
    if (req->hasHeader("Authorization") &&
        securityCheckToken(req->header("Authorization"))) {
        return true;
    }
    req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
}

// GET /api/live : mesures + état réseau + actionneurs + métriques.
static void handleLive(AsyncWebServerRequest* req) {
    if (!authed(req)) return;

    JsonDocument doc;
    SensorSample s = sensorsGetLatest();
    JsonObject data = doc["data"].to<JsonObject>();
    data["temp"]     = s.temp;
    data["humidity"] = s.humidity;
    doc["valid"]     = s.valid;
    doc["contact"]   = s.contact;
    doc["threshold"] = s.threshold;
    doc["ts"]        = s.ts;

    EventBits_t bits = xEventGroupGetBits(netState);
    doc["wifi"] = (bool)(bits & BIT_WIFI_OK);
    doc["mqtt"] = (bool)(bits & BIT_MQTT_OK);

    ActuatorState a = actuatorsGetState();
    doc["relay"] = a.relayOn;
    JsonObject rgb = doc["rgb"].to<JsonObject>();
    rgb["r"] = a.r; rgb["g"] = a.g; rgb["b"] = a.b;

    doc["heap"]   = (uint32_t)ESP.getFreeHeap();
    doc["uptime"] = (uint32_t)(millis() / 1000);

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

// GET /api/supervision : trame texte de l'OLED virtuel.
static void handleSupervision(AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    char frame[SUP_LINES * (SUP_COLS + 1)];
    supervisionCopyFrame(frame, sizeof(frame));
    req->send(200, "text/plain", frame);
}

// GET /api/config : config MQTT (le mot de passe n'est jamais renvoyé en clair).
static void handleConfigGet(AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    MqttConfig c = runtimeGetMqtt();
    JsonDocument doc;
    doc["host"]    = c.host;
    doc["port"]    = c.port;
    doc["user"]    = c.user;
    doc["passSet"] = (strlen(c.pass) > 0);
    doc["topic"]   = MQTT_TOPIC_DATA;     // imposé (lecture seule)
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

bool webInit() {
    // Fichiers statiques (UI) — non protégés, sinon la page ne se charge pas.
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.on("/api/live",        HTTP_GET, handleLive);
    server.on("/api/supervision", HTTP_GET, handleSupervision);
    server.on("/api/config",      HTTP_GET, handleConfigGet);

    // POST /api/threshold : {"value":0..4095}
    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/api/threshold",
        [](AsyncWebServerRequest* req, JsonVariant& json) {
            if (!authed(req)) return;
            if (!json["value"].is<int>()) {
                req->send(400, "application/json", "{\"error\":\"bad value\"}");
                return;
            }
            int v = json["value"].as<int>();
            if (v < 0) v = 0;
            if (v > THRESHOLD_MAX) v = THRESHOLD_MAX;
            runtimeSetThreshold((uint16_t)v);
            req->send(200, "application/json", "{\"ok\":true}");
        }));

    // POST /api/actuator : {"type":"relay","on":true} | {"type":"led","r","g","b"}
    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/api/actuator",
        [](AsyncWebServerRequest* req, JsonVariant& json) {
            if (!authed(req)) return;
            char buf[201];
            size_t n = serializeJson(json, buf, sizeof(buf));
            ActuatorCmd cmd;
            if (securityParseActuatorCmd(buf, n, cmd)) {
                xQueueSend(actuatorCmdQueue, &cmd, 0);
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(400, "application/json", "{\"error\":\"invalid command\"}");
            }
        }));

    // POST /api/config : {host,port,user,pass} (pass conservé si omis/vide)
    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/api/config",
        [](AsyncWebServerRequest* req, JsonVariant& json) {
            if (!authed(req)) return;
            MqttConfig c = runtimeGetMqtt();
            if (json["host"].is<const char*>())
                strlcpy(c.host, json["host"].as<const char*>(), sizeof(c.host));
            if (json["port"].is<int>())
                c.port = (uint16_t)json["port"].as<int>();
            if (json["user"].is<const char*>())
                strlcpy(c.user, json["user"].as<const char*>(), sizeof(c.user));
            if (json["pass"].is<const char*>()) {
                const char* pw = json["pass"].as<const char*>();
                if (strlen(pw) > 0) strlcpy(c.pass, pw, sizeof(c.pass));
            }
            runtimeSetMqtt(c);     // persiste + déclenche la reconnexion MQTT
            req->send(200, "application/json", "{\"ok\":true}");
        }));

    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "application/json", "{\"error\":\"not found\"}");
    });

    server.begin();
    return true;
}

void webTask(void* pv) {
    (void)pv;
    // Le serveur est callback-driven (tâche AsyncTCP) : cette tâche réserve le
    // slot d'architecture et pourrait porter du SSE/WebSocket périodique.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
