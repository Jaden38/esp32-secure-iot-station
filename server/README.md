# Serveur central — stack Docker

Broker MQTT + Node-RED + MongoDB, plus le bonus InfluxDB + Grafana, le tout en
un `docker compose up`.

## Démarrage

```bash
cd server
docker compose up -d        # build Node-RED (palettes) + lance toute la stack
docker compose logs -f      # suivre le démarrage
```

## Services & accès

| Service | URL / port | Identifiants |
|---|---|---|
| Mosquitto (MQTT) | `mqtt://<IP_PC>:1883` | `iot` / `change-me` |
| Node-RED | http://localhost:1880 | — |
| Dashboard Node-RED | http://localhost:1880/ui | — |
| MongoDB | `mongodb://localhost:27017` (db `iot`) | — |
| InfluxDB | http://localhost:8086 | `admin` / `esp32admin` |
| Grafana | http://localhost:3000 | `admin` / `esp32admin` |

> **InfluxDB** : org `esi`, bucket `iot`, token `esp32-iot-token`.

## Connecter l'ESP32

1. `MQTT_HOST` doit pointer vers l'**IP LAN du PC** qui héberge la stack (sur le
   même réseau Wi-Fi que l'ESP32 — ici le partage de connexion). Régler dans
   `include/secrets.h` **ou** via l'UI web embarquée (page de config MQTT).
2. Identifiants MQTT : `iot` / `change-me` (alignés sur les défauts `secrets.h`).
   Pour changer, modifier `MQTT_USER`/`MQTT_PASS` dans `docker-compose.yml` et
   côté ESP32.
3. Topics (imposés) : données `campus/g1/ESP32-1/data`, commandes
   `campus/g1/ESP32-1/cmd`, statut/LWT `campus/g1/ESP32-1/status`.

## Étapes manuelles après le 1er import (Node-RED)

Node-RED n'importe pas les secrets : à faire une fois dans l'éditeur (puis
*Deploy*) :

1. **Broker MQTT** : ouvrir un nœud MQTT → onglet *Security* → user `iot`,
   password `change-me`.
2. **InfluxDB** : ouvrir le nœud `influxdb out` → config `influx-iot` → token
   `esp32-iot-token` (org `esi`).
3. **MongoDB** : aucun identifiant par défaut (laisser vide).

Les palettes requises (`node-red-dashboard`, `node-red-node-mongodb`,
`node-red-contrib-influxdb`) sont déjà installées par le `Dockerfile`.

## Flux Node-RED (`nodered/flows.json`)

- `mqtt in campus/+/+/data` → **validation de schéma** (device/ts/data typés) →
  - **MongoDB** (`measurements`, NoSQL)
  - **InfluxDB** (`environment`, bonus historisation)
  - **Dashboard** : jauges temp/humidité + graphe + état (LWT)
- **Commandes** : interrupteur relais + sliders R/G/B → `mqtt out .../cmd`
  (JSON validé côté ESP32 par le module security).

## Grafana (bonus)

Datasource InfluxDB, dashboard *ESP32 IoT Station* et **2 alertes** provisionnés
automatiquement :

- **Absence de données** : aucun message depuis 10 min.
- **Anomalie température** : dernière valeur > 45 °C.

Métrique de robustesse : panneau *Messages reçus (10 min)*.

> Pour une notification (mail/Telegram…), ajouter un *contact point* dans
> Grafana > Alerting. Les règles changent d'état sans cela (visible dans
> *Alert rules*).

## Vérifier la chaîne sans ESP32

```bash
docker exec -it iot-mosquitto \
  mosquitto_pub -u iot -P change-me -t campus/g1/ESP32-1/data \
  -m '{"device":"ESP32-1","ts":0,"data":{"temp":23.4,"humidity":56.7}}'
```
La mesure doit apparaître dans le dashboard Node-RED, MongoDB et Grafana.
