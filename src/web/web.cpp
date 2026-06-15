#include "web.h"
#include "config.h"
#include "rtos_shared.h"
#include "runtime_config.h"
#include "sensors/sensors.h"
#include "actuators/actuators.h"
#include "control/control.h"
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
    doc["ts"]        = s.ts;

    EventBits_t bits = xEventGroupGetBits(netState);
    doc["wifi"] = (bool)(bits & BIT_WIFI_OK);
    doc["mqtt"] = (bool)(bits & BIT_MQTT_OK);

    ActuatorState a = actuatorsGetState();
    doc["relay"] = a.relayOn;
    JsonObject rgb = doc["rgb"].to<JsonObject>();
    rgb["r"] = a.r; rgb["g"] = a.g; rgb["b"] = a.b;

    // État métier (régulation / arrêt d'urgence)
    ControlConfig cfg = runtimeGetControl();
    doc["mode"]   = (cfg.mode == MODE_MANUEL) ? "manuel" : "auto";
    doc["tempOn"] = cfg.tempOn;
    doc["estop"]  = (bool)(xEventGroupGetBits(appState) & BIT_ESTOP);

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

// GET /api/control : paramètres de régulation (seuils, périodes, mode).
static void handleControlGet(AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    ControlConfig c = runtimeGetControl();
    JsonDocument doc;
    doc["acqPeriodMs"]    = c.acqPeriodMs;
    doc["ctrlPeriodMs"]   = c.ctrlPeriodMs;
    doc["pubPeriodMs"]    = c.pubPeriodMs;
    doc["tempOn"]         = c.tempOn;
    doc["hysteresis"]     = c.hysteresis;
    doc["humAlert"]       = c.humAlert;
    doc["mode"]           = (c.mode == MODE_MANUEL) ? "manuel" : "auto";
    doc["relayManual"]    = c.relayManual;
    doc["estopAutoReset"] = c.estopAutoReset;
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
    server.on("/api/control",     HTTP_GET, handleControlGet);

    // POST /api/estop/reset : réarme l'arrêt d'urgence
    server.on("/api/estop/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!authed(req)) return;
        controlResetEstop();
        req->send(200, "application/json", "{\"ok\":true}");
    });

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

    // POST /api/control : paramètres de régulation (champs optionnels)
    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/api/control",
        [](AsyncWebServerRequest* req, JsonVariant& json) {
            if (!authed(req)) return;
            ControlConfig c = runtimeGetControl();
            if (json["acqPeriodMs"].is<int>())  c.acqPeriodMs  = json["acqPeriodMs"];
            if (json["ctrlPeriodMs"].is<int>()) c.ctrlPeriodMs = json["ctrlPeriodMs"];
            if (json["pubPeriodMs"].is<int>())  c.pubPeriodMs  = json["pubPeriodMs"];
            if (json["tempOn"].is<float>())     c.tempOn       = json["tempOn"];
            if (json["hysteresis"].is<float>()) c.hysteresis   = json["hysteresis"];
            if (json["humAlert"].is<float>())   c.humAlert     = json["humAlert"];
            if (json["relayManual"].is<bool>()) c.relayManual  = json["relayManual"];
            if (json["estopAutoReset"].is<bool>()) c.estopAutoReset = json["estopAutoReset"];
            if (json["mode"].is<const char*>()) {
                const char* m = json["mode"].as<const char*>();
                c.mode = (strcmp(m, "manuel") == 0) ? MODE_MANUEL : MODE_AUTO;
            }
            runtimeSetControl(c);
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
