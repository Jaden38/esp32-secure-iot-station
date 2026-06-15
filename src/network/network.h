// ============================================================================
//  network/ — Wi-Fi + MQTT (PubSubClient).
//  Responsabilités (cf. CLAUDE.md §6) :
//    - connexion Wi-Fi + reconnexion auto -> BIT_WIFI_OK
//    - MQTT auth user/pass, LWT, QoS>=1, reconnexion -> BIT_MQTT_OK
//    - publish outboundJsonQueue sur MQTT_TOPIC_DATA
//    - subscribe MQTT_TOPIC_CMD -> validation -> actuatorCmdQueue
//    - draine actuatorCmdQueue (web + mqtt) via actuatorsDrainQueue()
//  Choix : PubSubClient (léger, QoS1 publish). Alternatives à comparer tâche #4.
// ============================================================================
#pragma once

bool networkInit();

// Tâche FreeRTOS réseau (CORE_NETWORK, PRIO_NETWORK).
void networkTask(void* pv);
