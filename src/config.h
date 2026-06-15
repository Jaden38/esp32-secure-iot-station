// ============================================================================
//  config.h — Constantes matérielles et applicatives (compile-time)
//  Centralise pins, adresses I²C, identités MQTT, priorités/cœurs des tâches.
//  Les SECRETS (Wi-Fi/MQTT credentials) vivent dans secrets.h (gitignoré).
// ============================================================================
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Identité du device / contrat MQTT (cf. CLAUDE.md §6)
//  Topic publish  : campus/<groupe>/<deviceID>/data
//  Topic subscribe: campus/<groupe>/<deviceID>/cmd
// ---------------------------------------------------------------------------
#define DEVICE_ID   "ESP32-1"
#define MQTT_GROUP  "g1"
#define MQTT_TOPIC_DATA  "campus/" MQTT_GROUP "/" DEVICE_ID "/data"
#define MQTT_TOPIC_CMD   "campus/" MQTT_GROUP "/" DEVICE_ID "/cmd"
#define MQTT_TOPIC_LWT   "campus/" MQTT_GROUP "/" DEVICE_ID "/status"

// ---------------------------------------------------------------------------
//  Bus I²C — ACTIF uniquement si HAS_OLED=1 (écran SSD1306 physique).
//  Par défaut (HAS_OLED=0) aucun périphérique I²C : DHT22 est en 1-wire,
//  l'OLED est virtuel (Serial + panneau web).
// ---------------------------------------------------------------------------
static constexpr int  I2C_SDA       = 21;
static constexpr int  I2C_SCL       = 22;
static constexpr uint32_t I2C_FREQ  = 400000;        // 400 kHz fast-mode
static constexpr uint8_t OLED_ADDR   = 0x3C;

// ---------------------------------------------------------------------------
//  GPIO capteurs / actionneurs
// ---------------------------------------------------------------------------
static constexpr int PIN_DHT      = 4;    // DHT22 1-wire (data), pull-up 10k
// NB: potentiomètre absent -> le "seuil" est réglé via l'UI web (pas d'ADC).
// "Contact" = deux fils à toucher ensemble (remplace le bouton). Même montage
// électrique qu'un bouton : entrée numérique + pull-up interne + IRQ.
static constexpr int PIN_CONTACT  = 27;
static constexpr int PIN_LED_R    = 25;   // LEDC (PWM)
static constexpr int PIN_LED_G    = 26;
static constexpr int PIN_LED_B    = 33;
static constexpr int PIN_RELAY    = 32;   // sortie GPIO -> relais 5V

// ---------------------------------------------------------------------------
//  Acquisition (cf. CLAUDE.md §3)
// ---------------------------------------------------------------------------
// DHT22 : période mini 2 s entre deux lectures fiables -> 2000 ms.
static constexpr uint32_t SENSOR_PERIOD_MS = 2000;   // 0.5 Hz (limite DHT22)
static constexpr size_t   FILTER_WINDOW     = 8;     // moyenne glissante
static constexpr uint32_t CONTACT_DEBOUNCE_MS = 30;  // anti-rebond contact 2 fils
// Bornes de plausibilité (détection valeurs aberrantes) — plage DHT22.
static constexpr float TEMP_MIN = -40.0f, TEMP_MAX = 80.0f;
static constexpr float HUM_MIN  =   0.0f, HUM_MAX  = 100.0f;

// Seuil réglable (remplace le potentiomètre) — défini via l'UI web, plage ADC
// 0..4095 par cohérence avec un vrai pot. Sert à déclencher le relais.
static constexpr uint16_t THRESHOLD_MIN     = 0;
static constexpr uint16_t THRESHOLD_MAX     = 4095;
static constexpr uint16_t THRESHOLD_DEFAULT = 2048;

// ---------------------------------------------------------------------------
//  Supervision (cf. CLAUDE.md §9)
// ---------------------------------------------------------------------------
static constexpr uint32_t SUPERVISION_PERIOD_MS = 10000;  // 0.1 Hz

// ---------------------------------------------------------------------------
//  FreeRTOS — répartition cœurs / priorités (cf. README archi logicielle)
//  Cœur 0 : capteurs + supervision   |   Cœur 1 : réseau + web + storage
// ---------------------------------------------------------------------------
static constexpr BaseType_t CORE_SENSORS    = 0;
static constexpr BaseType_t CORE_SUPERVISION = 0;
static constexpr BaseType_t CORE_NETWORK    = 1;
static constexpr BaseType_t CORE_WEB        = 1;
static constexpr BaseType_t CORE_STORAGE    = 1;

static constexpr UBaseType_t PRIO_NETWORK    = 4;
static constexpr UBaseType_t PRIO_SENSORS    = 3;
static constexpr UBaseType_t PRIO_WEB        = 2;
static constexpr UBaseType_t PRIO_STORAGE    = 2;
static constexpr UBaseType_t PRIO_SUPERVISION = 1;

// Tailles de pile (octets) — à ajuster via uxTaskGetStackHighWaterMark
static constexpr uint32_t STACK_SENSORS    = 4096;
static constexpr uint32_t STACK_NETWORK    = 8192;   // TLS/MQTT gourmand
static constexpr uint32_t STACK_WEB        = 8192;
static constexpr uint32_t STACK_STORAGE    = 4096;
static constexpr uint32_t STACK_SUPERVISION = 3072;

// ---------------------------------------------------------------------------
//  Tailles de queues (cf. README)
// ---------------------------------------------------------------------------
static constexpr size_t Q_SENSOR_LEN   = 16;
static constexpr size_t Q_CMD_LEN      = 8;
static constexpr size_t Q_OUTBOUND_LEN = 32;
