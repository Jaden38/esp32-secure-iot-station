// ============================================================================
//  network/ — Wi-Fi + MQTT (PubSubClient).
//  Responsabilités (cf. CLAUDE.md §6) :
//    - connexion Wi-Fi + reconnexion auto -> BIT_WIFI_OK
//    - MQTT auth user/pass, LWT, QoS>=1, reconnexion -> BIT_MQTT_OK
//    - publish outboundJsonQueue sur MQTT_TOPIC_DATA
//    - subscribe MQTT_TOPIC_CMD -> validation -> actuatorCmdQueue
//    - draine actuatorCmdQueue (web + mqtt) via actuatorsDrainQueue()
//    - synchronisation NTP (epoch pour les timestamps capteurs)
//  Choix (figé tâche #4) : 256dpi/arduino-mqtt (MQTTClient, lwmqtt) — publish
//  QoS 1 réel. PubSubClient écarté car limité au QoS 0 (cf. CLAUDE.md §6).
// ============================================================================
#pragma once

bool networkInit();

// Tâche FreeRTOS réseau (CORE_NETWORK, PRIO_NETWORK).
void networkTask(void* pv);
