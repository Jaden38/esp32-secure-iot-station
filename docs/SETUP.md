# Guide de mise en route complet

De zéro à une démo fonctionnelle : firmware ESP32 + serveur central (Docker).
Deux parties indépendantes — tu peux tester le serveur seul (sans carte) ou la
carte seule (sans serveur, elle bufferise en offline).

```
[ESP32] --Wi-Fi/MQTT--> [PC : Mosquitto -> Node-RED -> MongoDB/InfluxDB -> Grafana]
   |__ UI web embarquée (http://<ip-esp32>/)
```

---

## 0. Prérequis logiciels (Windows)

| Outil | Pour quoi | Vérifier |
|---|---|---|
| **PlatformIO Core** | compiler/flasher | `pio --version` |
| **Pilote USB-série** ESP32 (CP210x ou CH340) | voir la carte sur un COM | apparition d'un `COMx` |
| **Docker Desktop** | stack serveur | `docker --version` |
| **Git** | récupérer le projet | `git --version` |

> Pilote : carte « CP2102 » → driver *Silicon Labs CP210x* ; carte « CH340 » →
> driver *WCH CH340*. Après installation, branche l'ESP32 : un `COMx` apparaît
> dans le Gestionnaire de périphériques.

---

## 1. Câblage matériel (pas à pas)

⚠️ **ESP32 débranché (aucun USB) pendant tout le câblage.**

Breadboard : colonnes **0→60**, lignes **A-E** (banc haut) / **F-J** (banc bas)
séparées par la rainure. Rails : **+ haut = 3,3 V**, **− haut = GND**, **+ bas = 5 V**.

> Note : sur certaines breadboards les rails d'alimentation sont coupés au milieu.
> Si c'est le cas, ponte les deux moitiés avec un cavalier sur le rail **+** et un
> sur le rail **−**.

### Table de connexions (netlist)

| Composant | Broche → ESP32 | Résistance |
|---|---|---|
| DHT22 VCC / DATA / GND | 3V3 / **GPIO4** / GND | **10 kΩ** DATA↔3V3 *(sauf module 3 broches qui l'intègre)* |
| LED **rouge** (alarme) | anode → **GPIO25** | **470 Ω** en série |
| LED **orange** (actif) | anode → **GPIO26** | **470 Ω** en série |
| LED **verte** (nominal) | anode → **GPIO33** | **470 Ω** en série |
| LEDs (cathode) | → GND (rail −) | — |
| Relais IN / VCC / GND | **GPIO32** / 5V(VIN) / GND | — |
| Contact 2 fils | **GPIO27** et GND | aucune (pull-up interne) |

### Étapes (ESP32 hors breadboard, relié par cavaliers en ligne A)

**A. Rails**
1. **Pont central** (si rails coupés) : 1 cavalier sur le rail **+ haut**, 1 sur le rail **− haut**.
2. Câble **3,3 V** : ESP **3V3** → rail **+ haut**.
3. Câble **5 V** : ESP **VIN/5V** → rail **+ bas**.
4. Câble **GND** : ESP **GND** → rail **− haut**.

**B. DHT22** (module 3 broches `− / + / out`)
5. Enfonce le DHT22 : `−`→**E10**, `+`→**E11**, `out`→**E12**.
6. **A10** → rail **− haut** (GND) · **A11** → rail **+ haut** (3,3 V) · ESP **GPIO4** → **A12**.

**C. Relais** (`VCC / GND / IN`)
7. Enfonce le relais : `VCC`→**E20**, `GND`→**E21**, `IN`→**E22**.
8. **A20** → rail **+ bas** (5 V) · **A21** → rail **− haut** (GND) · ESP **GPIO32** → **A22**.

**D. LED rouge** (alarme)
9. Résistance **470 Ω** : **B25 ↔ B30**.
10. LED rouge : **anode (patte longue)** → **E30**, **cathode (patte courte)** → **E31**.
11. ESP **GPIO25** → **A25** · **A31** → rail **− haut** (GND).

**E. LED orange** (actif)
12. Résistance **470 Ω** : **B35 ↔ B40**.
13. LED orange : anode → **E40**, cathode → **E41**.
14. ESP **GPIO26** → **A35** · **A41** → rail **− haut**.

**F. LED verte** (nominal)
15. Résistance **470 Ω** : **B45 ↔ B50**.
16. LED verte : anode → **E50**, cathode → **E51**.
17. ESP **GPIO33** → **A45** · **A51** → rail **− haut**.

**G. Contact 2 fils** (arrêt d'urgence)
18. ESP **GPIO27** → **A55**.
19. Fil n°1 : **B55** → bout libre. Fil n°2 : rail **− haut** (GND) → bout libre.
20. Toucher les 2 bouts ensemble = contact fermé → arrêt d'urgence.

**H. Vérifs AVANT l'USB**
21. **Pont central** des rails en place (si la breadboard est coupée au milieu).
22. Chaque **résistance** enjambe **2 colonnes différentes** (25↔30 / 35↔40 / 45↔50) — sinon court-circuitée.
23. **Anode** (patte longue) côté résistance, **cathode** (patte courte) côté GND.
24. Aucun fil ne relie le **5 V** au **3,3 V** ni à une GPIO.
25. Branche l'USB et **touche l'ESP ~15 s** : il doit rester **froid**. S'il chauffe → débranche et revérifie (souvent VCC/GND inversés).

### BOM résistances
- **3 × 470 Ω** (limitation LEDs) · **1 × 10 kΩ** (pull-up DHT22, si non intégré).

> **Pièges fréquents** : ① rail coupé au milieu (cf. note plus haut) ; ② LED à l'envers
> (ne s'allume jamais) ; ③ contact câblé sur la mauvaise GPIO (doit être **27**) ;
> ④ module relais **actif-haut** alors que le firmware est actif-bas → passer
> `RELAY_ACTIVE_LOW=false` dans `config.h`.
>
> Sans DHT22 branché, les lectures sont invalides et **rien n'est publié** : pour
> une démo serveur seule, utilise l'injecteur MQTT (§8).

---

## 2. Récupérer le projet

```bash
git clone git@github.com:Jaden38/esp32-secure-iot-station.git
cd esp32-secure-iot-station
```

---

## 3. Configurer les secrets (firmware)

Le fichier `include/secrets.h` est **gitignoré** ; pars du modèle :

```bash
cp include/secrets.example.h include/secrets.h
```

Édite `include/secrets.h` :

```c
#define WIFI_SSID       "your-ssid"        // ton Wi-Fi / hotspot
#define WIFI_PASSWORD   "your-wifi-password"

#define MQTT_HOST     "192.168.x.y"        // IP LAN du PC (voir §4)
#define MQTT_PORT     1883
#define MQTT_USER     "iot"                // = identifiants du broker (§6)
#define MQTT_PASS     "change-me"

#define API_TOKEN     "un-token-long-aleatoire"  // protège l'UI web
```

> `MQTT_HOST`/`MQTT_USER`/`MQTT_PASS` peuvent aussi être changés ensuite **depuis
> l'UI web** (persistés en NVS). Mais le SSID/mot de passe Wi-Fi et l'`API_TOKEN`
> ne se règlent qu'ici.

---

## 4. Trouver l'IP du PC (réseau du hotspot)

Le broker tourne sur le PC : l'ESP32 doit l'atteindre via l'IP du PC **sur le
même réseau Wi-Fi**.

```bash
ipconfig
```
Repère l'adaptateur **Wi-Fi** connecté au hotspot → ligne **IPv4** (ex.
`192.168.43.50`). C'est ton `MQTT_HOST`.

> ⚠️ **Isolation AP** : certains partages de connexion Android isolent les
> clients entre eux (l'ESP32 ne « voit » pas le PC). Si la connexion MQTT échoue
> alors que tout est correct, teste avec un **routeur/box Wi-Fi** classique, ou
> active « autoriser les autres utilisateurs » sur le hotspot si l'option existe.

---

## 5. Flasher le firmware

Branche l'ESP32 en USB, puis depuis la racine du projet :

```bash
pio run                 # compile
pio run -t upload       # flashe le firmware (COM auto-détecté)
pio run -t uploadfs     # envoie l'UI web (/data) dans LittleFS  ← NE PAS OUBLIER
```

> Plusieurs cartes/ports ? `pio device list` puis
> `pio run -t upload --upload-port COMx`.

Ouvre le moniteur série pour voir le démarrage et **l'IP de l'ESP32** :

```bash
pio device monitor -b 115200
```
Tu dois voir : `=== tâches lancées ===`, puis
`[net] Wi-Fi OK — IP: 192.168.x.z (UI web: http://192.168.x.z/)`, puis
`[net] MQTT connecté`, et les lignes `[sup] ...` toutes les 10 s.

---

## 6. Démarrer le serveur central (Docker)

```bash
cd server
docker compose up -d --build      # build l'image Node-RED + lance toute la stack
docker compose ps                 # tout doit être "running"/"healthy"
docker compose logs -f            # (optionnel) suivre les logs
```

Services exposés :

| Service | URL | Login |
|---|---|---|
| Node-RED (éditeur) | http://localhost:1880 | — |
| Dashboard Node-RED | http://localhost:1880/ui | — |
| Grafana | http://localhost:3000 | `admin` / `esp32admin` |
| InfluxDB | http://localhost:8086 | `admin` / `esp32admin` |
| MQTT | `localhost:1883` | `iot` / `change-me` |

---

## 7. Configurer Node-RED (une seule fois)

Node-RED importe le flux automatiquement mais **pas les secrets** :

1. Ouvre http://localhost:1880
2. Double-clic sur le nœud **`data` (mqtt in)** → champ *Server* → crayon →
   onglet **Security** → user `iot`, password `change-me` → *Update*.
3. Double-clic sur le nœud **`InfluxDB`** → crayon sur le serveur → **Token**
   `esp32-iot-token` (org `esi`) → *Update*.
4. Clique **Deploy** (bouton rouge en haut à droite).

Vérifie : le nœud `data` doit afficher **« connected »** en vert.

---

## 8. Vérifier la chaîne (avec ou sans carte)

**Sans ESP32** — injecte une mesure de test :
```bash
docker exec -it iot-mosquitto mosquitto_pub -u iot -P change-me \
  -t campus/g1/ESP32-1/data \
  -m "{\"device\":\"ESP32-1\",\"ts\":0,\"data\":{\"temp\":23.4,\"humidity\":56.7}}"
```

**Avec ESP32** : les mesures partent automatiquement toutes les 2 s.

Dans les deux cas tu dois voir la donnée :
- **Dashboard Node-RED** : http://localhost:1880/ui (jauges + graphe)
- **MongoDB** : `docker exec -it iot-mongodb mongosh iot --eval "db.measurements.find().limit(3)"`
- **Grafana** : http://localhost:3000 → dashboard *ESP32 IoT Station*

---

## 9. UI web embarquée (ESP32)

1. Ouvre `http://<ip-esp32>/` (l'IP vue au §5 sur le série).
2. Au 1er chargement, saisis l'**`API_TOKEN`** (celui de `secrets.h`) — il est
   mémorisé dans le navigateur.
3. Tu peux : voir les mesures live, l'état Wi-Fi/MQTT, l'OLED virtuel, régler le
   **seuil**, **commander** relais et LED RGB, et modifier la **config MQTT**.

---

## 10. Démo « survie aux pannes » (Reliability)

1. Station connectée et publiant.
2. Coupe le broker : `docker compose stop mosquitto` → l'ESP32 passe offline et
   **bufferise** (voir `[net] échec MQTT` au série).
3. Relance : `docker compose start mosquitto` → la station **rejoue** le buffer ;
   les mesures manquantes réapparaissent dans Grafana/Node-RED.

---

## 11. Commander les actionneurs

- **Depuis l'UI web** : boutons relais + sélecteur de couleur LED.
- **Depuis Node-RED** : interrupteur Relais + sliders R/G/B (onglet *Commandes*).
- **En MQTT direct** :
  ```bash
  docker exec -it iot-mosquitto mosquitto_pub -u iot -P change-me \
    -t campus/g1/ESP32-1/cmd -m "{\"type\":\"relay\",\"on\":true}"
  ```

---

## 12. Dépannage rapide

| Symptôme | Piste |
|---|---|
| `pio run -t upload` ne trouve pas la carte | pilote USB manquant ; `pio device list` ; tenir BOOT enfoncé au flash |
| UI web vide / 404 | `pio run -t uploadfs` oublié |
| UI web : 401 | mauvais `API_TOKEN` (vider le localStorage du navigateur) |
| ESP `MQTT KO` mais Wi-Fi OK | `MQTT_HOST` ≠ IP PC, ou **isolation AP** du hotspot (§4) |
| Node-RED `data` non connecté | mot de passe MQTT non saisi (§7) |
| Grafana « No data » | token InfluxDB non saisi dans Node-RED (§7) ; attendre quelques points |
| Pas de mesure publiée | DHT22 non câblé → lectures invalides (normal) |
