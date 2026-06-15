# Image Mosquitto avec config + identifiants intégrés au build.
# Évite le bind-mount Windows du dossier /mosquitto/config (chown/permissions
# capricieux) et le wrapper sh -c : Mosquitto tourne via son entrypoint normal.
# Pour changer les identifiants : modifier les ARG (ou build args) puis rebuild.
FROM eclipse-mosquitto:2

ARG MQTT_USER=iot
ARG MQTT_PASS=change-me

COPY config/mosquitto.conf /mosquitto/config/mosquitto.conf
RUN mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASS"
