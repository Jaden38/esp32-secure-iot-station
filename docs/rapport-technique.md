# Rapport technique — Système IoT sécurisé et autonome

**Projet** : `esp32-secure-iot-station` · **Module** : Système IoT (Master II)
**Cible** : ESP32-WROOM-32 (Xtensa LX6 dual-core) · PlatformIO + Arduino-ESP32 3.x + FreeRTOS

> Diagrammes complets (architecture matérielle, logicielle, flux de données) et
> schéma de câblage : voir le [`README.md`](../README.md). Ce rapport synthétise
> les choix de conception et leur justification.

---

## 1. Objectif & vue d'ensemble

Station de supervision environnementale autonome et résiliente : acquisition
multi-capteurs filtrée, publication MQTT robuste (QoS 1), survie aux pannes
réseau (buffer + replay), interface web embarquée sécurisée, et chaîne serveur
Node-RED → MongoDB avec bonus historisation InfluxDB/Grafana.

La station mesure température/humidité, lit un **contact 2 fils** (acquittement)
et un **seuil réglable**, pilote une **LED RGB** (état) et un **relais**
(ventilation/éclairage), et s'auto-supervise (heap, uptime, latence).

## 2. Architecture logicielle (FreeRTOS)

Aucune logique dans `loop()` (vide) : tout vit dans **5 tâches** épinglées,
communiquant par **3 queues**, **mutex** et un **EventGroup** d'état réseau.

| Tâche | Cœur | Prio | Rôle |
|---|---|---|---|
| SensorAcquisition | 0 | 3 | DHT22 + contact + filtrage + JSON |
| Supervision | 0 | 1 | métriques sur série + OLED |
| NetworkMQTT | 1 | 4 | Wi-Fi/MQTT, publication, commandes |
| WebServer | 1 | 2 | AsyncWebServer + API |
| StorageReplay | 1 | 2 | buffer offline + retransmission |

**Répartition cœurs** : acquisition isolée sur le Cœur 0 (hors interruptions
Wi-Fi) ; réseau/web/stockage sur le Cœur 1 (à côté de la stack réseau Arduino).
Découplage par messages → pas de partage d'état mutable, pas d'interblocage.
Le code est strictement modulaire (`src/{sensors,actuators,network,storage,web,
security,supervision}/`) ; aucun `delay()` (uniquement `vTaskDelay`/timers).

## 3. Acquisition & filtrage (🟢 Sensor Engineer)

- **DHT22** (lib `DHTesp`) lu à 0,5 Hz (limite matérielle 2 s).
- **Filtrage** : moyenne glissante sur 8 échantillons, n'absorbant que les
  lectures valides (les aberrations ne polluent pas le filtre).
- **Timestamp** : epoch NTP dès synchronisation, sinon repli sur l'uptime.
- **Détection d'aberrations** : rejet `NaN`, contrôle du statut DHT, bornes
  min/max (température −40..80 °C, humidité 0..100 %) → flag `valid`.
- **Contact 2 fils** : entrée GPIO `INPUT_PULLUP` + **interruption** avec
  anti-rebond 30 ms en ISR (`IRAM_ATTR`).

> **Choix de lib** : `DHTesp` (bit-bang) retenue contre l'approche RMT
> non-bloquante car éprouvée ; son unique défaut (section critique ~5 ms) est
> neutralisé par l'épinglage de l'acquisition sur le Cœur 0, isolé du Wi-Fi.

## 4. Communication MQTT (🔵 Network Engineer)

- Lib **`256dpi/arduino-mqtt`** (lwmqtt) : `PubSubClient` a été **écarté** car
  il ne publie qu'en **QoS 0**, incompatible avec l'exigence QoS ≥ 1.
- **Publication QoS 1** sur le topic imposé `campus/<groupe>/<deviceID>/data`,
  payload au **format JSON imposé** `{device, ts, data:{temp, humidity}}`.
- **LWT** retained (`.../status` = `offline`/`online`), `keepalive` 30 s.
- **Reconnexion automatique** Wi-Fi et MQTT (tentatives espacées, non bloquant).
- **Souscription** aux commandes `campus/<groupe>/<deviceID>/cmd` →
  validation → file d'actionneurs.
- **Latence de publication** mesurée et transmise à la supervision.

## 5. Mode offline & fiabilité (⚫ Reliability Engineer)

- Système de fichiers **LittleFS** (SPIFFS déprécié ; LittleFS = append
  *crash-proof* + meilleur wear-leveling).
- MQTT indisponible → chaque message est **bufferisé** en JSONL
  (`buffer.jsonl`). **Compaction** au-delà de 256 Ko (conservation des plus
  récents) pour borner l'usure flash.
- À la reconnexion (bit `MQTT_OK`), la tâche StorageReplay **rejoue** le buffer
  vers la file de publication. Le buffer est d'abord renommé en `.replay` : les
  nouveaux messages vont dans un fichier neuf → **aucune perte**, doublons
  minimisés (sémantique *at-least-once*, renforcée par le QoS 1).

## 6. Sécurité (🔴 Security Engineer)

- **Authentification MQTT** user/password (broker Mosquitto, `allow_anonymous
  false`).
- **Validation des JSON entrants** (commandes) : parsing strict ArduinoJson,
  vérification des champs/types et **bornes** (rejet des payloads > 200 o,
  type inconnu, valeurs RGB hors 0..255).
- **Protection de l'API web locale** : token **Bearer** sur toutes les routes
  `/api/*`, comparé en **temps quasi constant** (anti timing-attack). Les
  fichiers statiques restent publics pour permettre le chargement de l'UI.
- Secrets isolés dans `include/secrets.h` (gitignoré ; modèle versionné).

## 7. Interface web embarquée (🟣 Full-Stack IoT)

- **AsyncWebServer** (fork maintenu `esp32async`), fichiers **séparés** servis
  depuis LittleFS (`/data/{index.html,app.js,style.css}`) — zéro HTML/JS inliné.
- Fonctions : **live** capteurs, **état** Wi-Fi/MQTT, **configuration MQTT**
  (broker/port/user/pass) persistée en **NVS** et appliquée à chaud (reconnexion
  automatique), **commande** des actionneurs, **OLED virtuel** (panneau de
  supervision).
- API : `GET /api/live`, `GET /api/supervision`, `GET|POST /api/config`,
  `POST /api/threshold`, `POST /api/actuator`.

## 8. Supervision & performance (🟡 Performance Engineer)

Tâche périodique affichant **heap libre**, **uptime** et **latence de
publication MQTT** (+ état réseau, dernières mesures) sur le **port série** et
un **OLED** — réel (U8g2) si présent, sinon **virtuel** dans l'UI web.
Empreinte mesurée : **RAM ~15 %**, **Flash ~38 %** (partition `huge_app`, 3 Mo).

## 9. Serveur central & bonus (🟣 / Bonus Grafana)

Stack **Docker Compose** (`server/`) en une commande :

- **Mosquitto** (broker authentifié).
- **Node-RED** : `mqtt in` → **validation de schéma** → **MongoDB** (NoSQL,
  collection `measurements`) + **InfluxDB** (historisation) + **dashboard**
  (jauges, graphe, état) ; **commandes** (relais + RGB) vers le topic `cmd`.
- **Bonus InfluxDB + Grafana** : dashboard provisionné (température, humidité,
  dernière valeur, **métrique de robustesse** « messages/10 min ») et **2
  alertes** : *absence de données* (10 min) et *anomalie température* (> 45 °C).

## 10. Adaptations matérielles

Composants indisponibles, remplacés par des substituts logiciels documentés
(sans dégrader les exigences) :

| Manquant | Substitut |
|---|---|
| OLED SSD1306 | OLED **virtuel** (série + panneau web) ; pilote U8g2 conservé, activable par `HAS_OLED=1` (vérifié à la compilation) |
| Potentiomètre | **Seuil web** (slider) persisté en NVS |
| Bouton | **Contact 2 fils** (montage électrique identique) |

Conséquence : plus aucun périphérique I²C requis par défaut (DHT22 en 1-wire).

## 11. Bilan & pistes

**Tous les badges visés sont couverts** (firmware + serveur), bonus Grafana
inclus. Améliorations possibles : MQTT **TLS** (mqtts/8883), OTA, dédoublonnage
applicatif des replays côté Node-RED, et lib DHT **RMT** non-bloquante si une
acquisition sans section critique devenait nécessaire.

| Badge | Couverture |
|---|---|
| 🟢 Sensor / 🔵 Network / 🟠 Embedded | filtrage+sanity / QoS1+reconnexion / 5 tâches FreeRTOS |
| 🔴 Security / 🟣 Full-Stack | token+validation+auth / web+Node-RED |
| ⚫ Reliability / 🟡 Performance | buffer+replay / heap+uptime+latence |
| ★ Bonus | InfluxDB + Grafana + alertes |
