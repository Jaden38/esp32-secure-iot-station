// ============================================================================
//  config.h — Constantes matérielles et applicatives (compile-time)
//  Centralise pins, adresses I²C, identités MQTT, priorités/cœurs des tâches.
//  Les SECRETS (Wi-Fi/MQTT credentials) vivent dans secrets.h (gitignoré).
// ============================================================================
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Identité du device / contrat MQTT
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
//  Acquisition
// ---------------------------------------------------------------------------
// DHT22 : période mini 2 s entre deux lectures fiables -> 2000 ms.
static constexpr uint32_t SENSOR_PERIOD_MS = 2000;   // 0.5 Hz (limite DHT22)
static constexpr size_t   FILTER_WINDOW     = 8;     // moyenne glissante
static constexpr uint32_t CONTACT_DEBOUNCE_MS = 30;  // anti-rebond contact 2 fils
// Bornes de plausibilité (détection valeurs aberrantes) — plage DHT22.
static constexpr float TEMP_MIN = -40.0f, TEMP_MAX = 80.0f;
static constexpr float HUM_MIN  =   0.0f, HUM_MAX  = 100.0f;

// ---------------------------------------------------------------------------
//  Régulation métier (supervision armoire technique) — valeurs par défaut.
//  Toutes modifiables à chaud via l'UI web (runtime_config, NVS).
// ---------------------------------------------------------------------------
// Périodes des opérations FreeRTOS (découplées)
static constexpr uint16_t ACQ_PERIOD_MS_DEFAULT  = 2000;  // acquisition (>=2s DHT)
static constexpr uint16_t CTRL_PERIOD_MS_DEFAULT = 1000;  // régulation relais/LED
static constexpr uint16_t PUB_PERIOD_MS_DEFAULT  = 10000; // publication MQTT
// Seuils de régulation
static constexpr float TEMP_ON_DEFAULT   = 28.0f;  // °C : ventilation ON au-dessus
static constexpr float HYSTERESIS_DEFAULT = 2.0f;  // °C : OFF sous (tempOn - hyst)
static constexpr float HUM_ALERT_DEFAULT = 70.0f;  // % : alerte humidité au-dessus
// Mode de fonctionnement
enum CtrlMode : uint8_t { MODE_AUTO = 0, MODE_MANUEL = 1 };
static constexpr bool ESTOP_AUTO_RESET_DEFAULT = false;  // reset manuel par défaut
static constexpr uint16_t ACQ_PERIOD_MS_MIN = 2000;      // borne basse DHT22

// ---------------------------------------------------------------------------
//  Actionneurs — LED RGB (LEDC) + relais
// ---------------------------------------------------------------------------
static constexpr uint32_t LED_PWM_FREQ     = 5000;  // Hz
static constexpr uint8_t  LED_PWM_RES_BITS = 8;     // duty 0..255
// LED RGB réalisée avec 3 LEDs discrètes à CATHODE commune (anode = GPIO+R).
// Passer à true si LED RGB à anode commune (logique PWM inversée).
static constexpr bool LED_COMMON_ANODE = false;
// ⚠️ VÉRIFIER le module relais : la plupart des modules bon marché sont
// ACTIF-BAS (IN=LOW -> relais fermé). Mettre false si actif-haut.
static constexpr bool RELAY_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
//  Storage offline (LittleFS)
// ---------------------------------------------------------------------------
// Buffer JSONL des messages non publiés ; compaction au-delà de MAX (on garde
// les plus récents jusqu'à KEEP) pour borner l'usure flash.
static constexpr size_t STORAGE_MAX_BYTES  = 256 * 1024;
static constexpr size_t STORAGE_KEEP_BYTES = 192 * 1024;

// ---------------------------------------------------------------------------
//  Supervision
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

static constexpr UBaseType_t PRIO_SAFETY     = 5;  // arrêt d'urgence (max)
static constexpr UBaseType_t PRIO_NETWORK    = 4;
static constexpr UBaseType_t PRIO_SENSORS    = 3;
static constexpr UBaseType_t PRIO_CONTROL    = 3;  // régulation
static constexpr UBaseType_t PRIO_WEB        = 2;
static constexpr UBaseType_t PRIO_STORAGE    = 2;
static constexpr UBaseType_t PRIO_TELEMETRY  = 2;
static constexpr UBaseType_t PRIO_SUPERVISION = 1;

// Cœurs / piles des opérations métier
static constexpr BaseType_t CORE_CONTROL   = 0;   // avec l'acquisition
static constexpr BaseType_t CORE_SAFETY    = 0;
static constexpr BaseType_t CORE_TELEMETRY = 1;   // côté réseau
static constexpr uint32_t STACK_CONTROL    = 4096;
static constexpr uint32_t STACK_SAFETY     = 3072;
static constexpr uint32_t STACK_TELEMETRY  = 4096;

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
