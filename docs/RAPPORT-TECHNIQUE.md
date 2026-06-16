# Rapport technique — Station IoT sécurisée et autonome

**Module** : Système IoT — Master II · **Cible** : ESP32-WROOM-32 + FreeRTOS
**Dépôt** : `esp32-secure-iot-station` · **Groupe** : g1 · **Device** : ESP32-1

> 📄 *Document de synthèse (≤ 5 pages). Le câblage détaillé est dans
> [`SETUP.md`](SETUP.md), l'architecture complète dans
> [`SPEC-TECHNIQUE.md`](SPEC-TECHNIQUE.md), le contexte métier dans
> [`SPEC-METIER.md`](SPEC-METIER.md).*

---

## 1. Contexte & objectif

La station supervise une **armoire technique** (local contenant des équipements
sensibles à la chaleur). Elle doit : mesurer température/humidité, **réguler la
ventilation** par seuil, signaler son état, offrir un **arrêt d'urgence**,
remonter ses données à un serveur central, et **continuer à fonctionner même
réseau coupé**. L'enjeu transverse est la **fiabilité** (autonomie en cas de
panne) et la **sécurité** (auth + validation des entrées).

> 📷 *Placeholder — photo du montage réel (ESP32 + DHT22 + 3 LEDs + relais + contact).*

---

## 2. Architecture logicielle

Le firmware respecte le découpage modulaire imposé (`src/sensors`, `actuators`,
`network`, `storage`, `web`, `security`, plus `control` et `supervision`).
`loop()` est **vide** (un simple `vTaskDelay`) : toute la logique vit dans
**8 tâches FreeRTOS** épinglées sur les 2 cœurs, communiquant par **queues**,
**mutex** et **event-groups** — aucun état mutable partagé sans protection.

> 📷 *Placeholder — diagramme d'architecture logicielle (tâches / cœurs /
> queues / mutex / event-groups). Source : diagramme Mermaid de `SPEC-TECHNIQUE.md`.*

| Tâche | Prio | Cœur | Rôle |
|---|---|---|---|
| `Safety` | 5 | 0 | Arrêt d'urgence (réveil par sémaphore donné en ISR) |
| `NetworkMQTT` | 4 | 1 | Wi-Fi + MQTT (reconnexion, publication, commandes) |
| `SensorAcq` | 3 | 0 | Acquisition DHT22 + contact |
| `Control` | 3 | 0 | Régulation ventilation (hystérésis) + voyants d'état |
| `WebServer` | 2 | 1 | API + UI embarquée |
| `StorageReplay` | 2 | 1 | Rejeu du buffer offline |
| `Telemetry` | 2 | 1 | Publication périodique découplée |
| `Supervision` | 1 | 0 | heap / uptime / latence |

**Hiérarchie des priorités** : *sécurité > réseau > métier temps réel > confort >
observation*. Les priorités reflètent **l'ordre de passage en cas de conflit**,
pas une vitesse d'exécution. La répartition cœurs isole le temps réel local
(cœur 0) des piles réseau potentiellement bloquantes (cœur 1).

**Primitives de synchronisation utilisées** :
- **Queues** : `actuatorCmdQueue` (web/MQTT → régulation), `outboundJsonQueue`
  (télémétrie → réseau).
- **Mutex** : cache mesure (`sensorsGetLatest`), LittleFS, trame OLED.
- **Event-groups** : `netState` (WIFI_OK / MQTT_OK), `appState` (ESTOP).
- **Sémaphore binaire** : `estopSem`, donné par l'**ISR du contact** pour
  réveiller `Safety` (réaction < 50 ms).

---

## 3. Acquisition fiable des capteurs

Chaque mesure DHT22 est : **filtrée** (moyenne glissante sur 8 échantillons),
**horodatée** (epoch NTP, repli sur l'uptime si non synchronisé), et soumise à
une **détection d'aberrations** (bornes −40..80 °C / 0..100 %). Les valeurs hors
bornes sont marquées `valid=false` et **ne polluent pas le filtre**. Le dernier
échantillon est diffusé via un **cache protégé par mutex** (lu par
régulation / web / télémétrie / supervision).

---

## 4. Régulation & actionneurs

Régulation **par hystérésis** (défauts : ON ≥ 28 °C, OFF ≤ 26 °C) pilotant le
relais de ventilation. Deux modes (`auto` / `manuel`) ; une commande manuelle
bascule automatiquement en `manuel`. Les **3 LEDs discrètes** servent de voyants
d'état exclusifs : 🟢 nominal · 🟠 ventilation active ou humidité haute ·
🔴 défaut capteur / arrêt d'urgence (clignotant). Les seuils et périodes sont
**modifiables à chaud** via l'UI web et **persistés en NVS**.

> 📷 *Placeholder — capture de l'interface web (live + régulation + OLED virtuel).*

---

## 5. Communication MQTT & mode offline

Publication sur `campus/g1/ESP32-1/data` en **QoS 1**, avec **LWT retained**
(`status`), **reconnexion automatique** et **souscription aux commandes**
(`cmd`). Le format JSON imposé est respecté, enrichi de champs métier :

```json
{ "device": "ESP32-1", "ts": 0, "data": { "temp": 0, "humidity": 0 },
  "estop": false, "relay": true, "mode": "auto" }
```

**Mode offline (fiabilité)** : si MQTT est indisponible, chaque mesure est
**bufferisée** en JSONL sur LittleFS (compaction au-delà de 256 Ko). À la
reconnexion, le buffer est **renommé `.replay`** (les nouvelles mesures vont dans
un buffer neuf → zéro perte) puis **rejoué** en QoS 1, garantissant une livraison
*at-least-once*.

> 📷 *Placeholder — logs du scénario de panne : `MQTT hors-ligne -> mesure
> bufferisée`, puis `reconnecté -> replay de N mesure(s)`, puis arrivée dans MongoDB.*

---

## 6. Sécurité

- **Auth MQTT** user/password (broker Mosquitto, `allow_anonymous false`).
- **Validation JSON** des commandes entrantes : parsing strict, type vérifié,
  rejet des payloads > 200 o, seule la commande `relay` est acceptée.
- **API web** protégée par **token Bearer** comparé en **temps quasi constant**
  (anti timing-attack) ; seuls les fichiers statiques de l'UI sont publics.
- Secrets isolés dans `include/secrets.h` (gitignoré).

---

## 7. Optimisation & supervision

La tâche `Supervision` affiche **périodiquement** (série + **OLED virtuel** web) :
**heap libre**, **uptime**, **latence de publication MQTT** (mesurée autour du
`publish`). Empreinte mesurée : **RAM ≈ 15 %**, **Flash ≈ 38 %** (partition
`huge_app.csv`, 3 Mo). Horloges : CPU 240 MHz, PWM LEDs 5 kHz, tick FreeRTOS 1 kHz.

> 📷 *Placeholder — sortie série de supervision (heap / uptime / latence).*

---

## 8. Serveur central (Node-RED + bases) & bonus Grafana

Node-RED **reçoit** le MQTT, **valide le schéma**, **stocke en MongoDB**
(NoSQL), alimente un **dashboard** (jauges + historique) et **renvoie des
commandes** vers `cmd`. La stack est conteneurisée (`docker compose`) :
Mosquitto (auth), MongoDB, Node-RED, InfluxDB, Grafana.

**Bonus** : historisation dans **InfluxDB** + **dashboard Grafana** (état station,
mesures, actionneurs, métrique de robustesse) avec **alerte** sur absence de
données / anomalie capteur.

> 📷 *Placeholder — dashboard Node-RED.*
> 📷 *Placeholder — dashboard Grafana + alerte configurée.*

---

## 9. Choix techniques & arbitrages

| Décision | Justification |
|---|---|
| **256dpi/arduino-mqtt** vs PubSubClient | QoS 1 réel (PubSubClient limité au QoS 0) |
| **pioarduino + core 3.x** | Le core Arduino-ESP32 officiel est resté en 2.x |
| **Cache + mutex** pour le live, **queues** pour commandes/sortie | "Dernière valeur" ≠ flux producteur/consommateur |
| **Partition `huge_app.csv`** | Marge applicative (Flash 38 %), large FS, pas d'OTA requis |
| **8 tâches sur 2 cœurs** | Découple les rythmes (0,5 Hz capteur / 10 s pub / web / estop) |

**Substitutions matérielles** (équivalences fonctionnelles, pas des manques) :
BME280 → **DHT22** · potentiomètre → **seuil web** (NVS) · bouton → **contact
2 fils** (même montage : entrée + pull-up + IRQ) · LED RGB → **3 LEDs discrètes**
(voyants d'état) · OLED SSD1306 → **OLED virtuel** (série + panneau web ; pilote
U8g2 conservé sous `HAS_OLED`).

---

## 10. Couverture des badges

| Badge | Preuve |
|---|---|
| 🟢 Sensor | Filtrage moyenne glissante + timestamp + bornes aberrantes (`sensors.cpp`) |
| 🔵 Network | QoS 1 + LWT + reconnexion + souscription `cmd` (`network.cpp`) |
| 🟠 Embedded | 8 tâches / 2 cœurs + queues/mutex/event-groups/ISR (`main.cpp`, `rtos_shared`) |
| 🔴 Security | Token temps-constant + validation JSON + auth MQTT (`security.cpp`) |
| 🟣 Full-Stack | UI embarquée + Node-RED + MongoDB (`web/`, `server/`) |
| ⚫ Reliability | Buffer LittleFS + replay `.replay` *at-least-once* + compaction (`storage.cpp`) |
| 🟡 Performance | heap / uptime / latence pub périodiques (`supervision.cpp`) |
| ★ Bonus | Historisation InfluxDB + dashboard Grafana + alerte |

---

## 11. Limites & perspectives

- **Sécurité transport** : MQTT en clair (auth seule). Évolution : **TLS** broker
  + certificats device.
- **Horloge** : timestamp NTP, repli uptime si NTP indisponible au boot.
- **OTA** : non activé (partition sans seconde app) ; possible en repassant sur
  un schéma de partition OTA au prix de l'espace applicatif.
- **Multi-stations** : le contrat de topic (`campus/<groupe>/<deviceID>`) et le
  flow Node-RED (wildcards `+`) supportent déjà la montée en charge.

> 📷 *Placeholder — diagramme de flux end-to-end capteur → MQTT → Node-RED →
> bases → dashboards. Source : diagramme Mermaid de `SPEC-TECHNIQUE.md`.*
