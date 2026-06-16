#include "security.h"
#include "config.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <string.h>

// Comparaison en temps quasi constant (évite les timing attacks sur le token).
static bool constantTimeEquals(const char* a, size_t la, const char* b, size_t lb) {
    uint8_t diff = (uint8_t)(la ^ lb);
    for (size_t i = 0; i < la; i++) diff |= (uint8_t)(a[i] ^ b[i % (lb ? lb : 1)]);
    return diff == 0 && la == lb;
}

bool securityCheckToken(const String& authHeader) {
    const String prefix = "Bearer ";
    if (!authHeader.startsWith(prefix)) return false;
    String tok = authHeader.substring(prefix.length());
    return constantTimeEquals(tok.c_str(), tok.length(),
                              API_TOKEN, strlen(API_TOKEN));
}

bool securityParseActuatorCmd(const char* json, size_t len, ActuatorCmd& out) {
    if (!json || len == 0 || len > 200) return false;     // rejette payloads longs

    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;    // JSON invalide

    const char* type = doc["type"].is<const char*>() ? doc["type"].as<const char*>()
                                                      : nullptr;
    if (!type) return false;

    // Seule commande externe acceptée : {"type":"relay","on":true|false}.
    // (Les LEDs sont des voyants d'état pilotés par control/, non commandables.)
    if (strcmp(type, "relay") == 0) {
        if (!doc["on"].is<bool>()) return false;
        out.type    = ActuatorType::RELAY;
        out.relayOn = doc["on"].as<bool>();
        return true;
    }

    return false;   // type inconnu
}
