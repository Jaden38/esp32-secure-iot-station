#include "runtime_config.h"
#include "config.h"
#include "secrets.h"
#include <Preferences.h>

// Cache RAM protégé par mutex ; persistance NVS (namespace "cfg").
static Preferences       prefs;
static SemaphoreHandle_t s_mutex = nullptr;
static ControlConfig     s_ctrl = {};
static MqttConfig        s_mqtt = {};
static volatile bool     s_mqttDirty = false;

static void lock()   { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
static void unlock() { if (s_mutex) xSemaphoreGive(s_mutex); }

void runtimeConfigInit() {
    s_mutex = xSemaphoreCreateMutex();

    prefs.begin("cfg", true);                       // lecture seule
    s_ctrl.acqPeriodMs    = prefs.getUShort("ap", ACQ_PERIOD_MS_DEFAULT);
    s_ctrl.ctrlPeriodMs   = prefs.getUShort("cp", CTRL_PERIOD_MS_DEFAULT);
    s_ctrl.pubPeriodMs    = prefs.getUShort("pp", PUB_PERIOD_MS_DEFAULT);
    s_ctrl.tempOn         = prefs.getFloat("ton", TEMP_ON_DEFAULT);
    s_ctrl.hysteresis     = prefs.getFloat("hys", HYSTERESIS_DEFAULT);
    s_ctrl.humAlert       = prefs.getFloat("hal", HUM_ALERT_DEFAULT);
    s_ctrl.mode           = prefs.getUChar("md", MODE_AUTO);
    s_ctrl.relayManual    = prefs.getBool("rm", false);
    s_ctrl.estopAutoReset = prefs.getBool("ear", ESTOP_AUTO_RESET_DEFAULT);
    if (s_ctrl.acqPeriodMs < ACQ_PERIOD_MS_MIN) s_ctrl.acqPeriodMs = ACQ_PERIOD_MS_MIN;

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

ControlConfig runtimeGetControl() {
    lock(); ControlConfig c = s_ctrl; unlock();
    return c;
}

void runtimeSetControl(const ControlConfig& c) {
    ControlConfig v = c;
    if (v.acqPeriodMs < ACQ_PERIOD_MS_MIN) v.acqPeriodMs = ACQ_PERIOD_MS_MIN;
    if (v.ctrlPeriodMs < 100)  v.ctrlPeriodMs = 100;
    if (v.pubPeriodMs  < 1000) v.pubPeriodMs  = 1000;
    if (v.mode > MODE_MANUEL)  v.mode = MODE_AUTO;

    lock();
    s_ctrl = v;
    prefs.begin("cfg", false);
    prefs.putUShort("ap", v.acqPeriodMs);
    prefs.putUShort("cp", v.ctrlPeriodMs);
    prefs.putUShort("pp", v.pubPeriodMs);
    prefs.putFloat("ton", v.tempOn);
    prefs.putFloat("hys", v.hysteresis);
    prefs.putFloat("hal", v.humAlert);
    prefs.putUChar("md", v.mode);
    prefs.putBool("rm", v.relayManual);
    prefs.putBool("ear", v.estopAutoReset);
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
