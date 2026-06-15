#include "runtime_config.h"
#include "config.h"
#include "secrets.h"
#include <Preferences.h>

// Cache RAM protégé par mutex ; persistance NVS (namespace "cfg").
static Preferences       prefs;
static SemaphoreHandle_t s_mutex = nullptr;
static uint16_t          s_threshold = THRESHOLD_DEFAULT;
static MqttConfig        s_mqtt = {};
static volatile bool     s_mqttDirty = false;

static void lock()   { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
static void unlock() { if (s_mutex) xSemaphoreGive(s_mutex); }

void runtimeConfigInit() {
    s_mutex = xSemaphoreCreateMutex();

    prefs.begin("cfg", true);                       // lecture seule
    s_threshold = prefs.getUShort("thr", THRESHOLD_DEFAULT);
    String h  = prefs.getString("mh",  MQTT_HOST);
    uint16_t p = prefs.getUShort("mp", (uint16_t)MQTT_PORT);
    String u  = prefs.getString("mu",  MQTT_USER);
    String pw = prefs.getString("mpw", MQTT_PASS);
    prefs.end();

    strlcpy(s_mqtt.host, h.c_str(),  sizeof(s_mqtt.host));
    strlcpy(s_mqtt.user, u.c_str(),  sizeof(s_mqtt.user));
    strlcpy(s_mqtt.pass, pw.c_str(), sizeof(s_mqtt.pass));
    s_mqtt.port = p;
}

uint16_t runtimeGetThreshold() {
    lock(); uint16_t v = s_threshold; unlock();
    return v;
}

void runtimeSetThreshold(uint16_t v) {
    if (v > THRESHOLD_MAX) v = THRESHOLD_MAX;
    lock();
    s_threshold = v;
    prefs.begin("cfg", false);
    prefs.putUShort("thr", v);
    prefs.end();
    unlock();
}

MqttConfig runtimeGetMqtt() {
    lock(); MqttConfig c = s_mqtt; unlock();
    return c;
}

void runtimeSetMqtt(const MqttConfig& c) {
    lock();
    strlcpy(s_mqtt.host, c.host, sizeof(s_mqtt.host));
    strlcpy(s_mqtt.user, c.user, sizeof(s_mqtt.user));
    strlcpy(s_mqtt.pass, c.pass, sizeof(s_mqtt.pass));
    s_mqtt.port = c.port;

    prefs.begin("cfg", false);
    prefs.putString("mh",  s_mqtt.host);
    prefs.putUShort("mp",  s_mqtt.port);
    prefs.putString("mu",  s_mqtt.user);
    prefs.putString("mpw", s_mqtt.pass);
    prefs.end();

    s_mqttDirty = true;
    unlock();
}

bool runtimeMqttDirty()      { return s_mqttDirty; }
void runtimeClearMqttDirty() { s_mqttDirty = false; }
