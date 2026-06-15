// ============================================================================
//  secrets.example.h — MODÈLE de configuration sensible
//  -> Copier en `include/secrets.h` et remplir. secrets.h est GITIGNORÉ.
//     cp include/secrets.example.h include/secrets.h
// ============================================================================
#pragma once

// --- Wi-Fi ---
#define WIFI_SSID       "your-ssid"
#define WIFI_PASSWORD   "your-wifi-password"

// --- MQTT (auth obligatoire, cf. cahier des charges §8) ---
#define MQTT_HOST       "192.168.1.10"
#define MQTT_PORT       1883
#define MQTT_USER       "iot"
#define MQTT_PASS       "change-me"

// --- Protection de l'API web locale (token Bearer, cf. cahier des charges §8) ---
#define API_TOKEN       "change-me-long-random-token"
