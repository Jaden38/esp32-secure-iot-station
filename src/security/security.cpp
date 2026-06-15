#include "security.h"
#include "config.h"
#include "secrets.h"

// TODO(tâche #7) : validation stricte via ArduinoJson (types + bornes),
// comparaison de token en temps constant, rejet payloads trop longs.

bool securityCheckToken(const String& authHeader) {
    // TODO: extraire "Bearer <token>" et comparer à API_TOKEN.
    const String prefix = "Bearer ";
    if (!authHeader.startsWith(prefix)) return false;
    return authHeader.substring(prefix.length()) == API_TOKEN;
}

bool securityParseActuatorCmd(const char* json, size_t len, ActuatorCmd& out) {
    (void)json; (void)len; (void)out;
    // TODO: JsonDocument doc; deserializeJson; vérifier champs/types; remplir out.
    return false;
}
