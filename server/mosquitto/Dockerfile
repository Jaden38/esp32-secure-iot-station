# Image Mosquitto avec config + identifiants intégrés au build.
# Évite le bind-mount Windows du dossier /mosquitto/config (chown/permissions
# capricieux) et le wrapper sh -c : Mosquitto tourne via son entrypoint normal.
# Pour changer les identifiants : modifier les ARG (ou build args) puis rebuild.
FROM eclipse-mosquitto:2

ARG MQTT_USER=iot
ARG MQTT_PASS=change-me

COPY config/mosquitto.conf /mosquitto/config/mosquitto.conf
# Le pwfile doit appartenir à l'utilisateur mosquitto (UID 1883) qui exécute le
# broker, sinon "Unable to open pwfile" (exit 13). chmod 0700 = lisible par le
# propriétaire uniquement (pas d'avertissement "file too open").
RUN mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASS" \
 && chown mosquitto:mosquitto /mosquitto/config/passwd \
 && chmod 0700 /mosquitto/config/passwd
