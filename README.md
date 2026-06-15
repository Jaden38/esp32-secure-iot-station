# esp32-secure-iot-station

Station IoT autonome et sécurisée pour bâtiments techniques, basée sur ESP32 + FreeRTOS.
Projet du module **Système IoT** — Master II.

## Objectifs

- Acquisition multi-capteurs avec filtrage, timestamp et détection d'aberrations
- Communication MQTT robuste (QoS ≥ 1, reconnexion auto, LWT)
- Stockage local et retransmission en cas de perte réseau
- Interface Web embarquée (config + commandes + live)
- Intégration Node-RED (dashboard, NoSQL) + bonus Grafana / InfluxDB
- Mécanismes de sécurité (auth MQTT, validation JSON, protection API)
- Supervision système (heap, uptime, latence) sur OLED + série

## Matériel

| Composant | Rôle | Interface |
|---|---|---|
| ESP32 DevKit | MCU | — |
| BME280 | Température / Humidité / Pression | I²C |
| OLED SSD1306 | Affichage supervision | I²C |
| Potentiomètre | Seuil réglable | ADC |
| Bouton poussoir | Acquittement / config | GPIO + IRQ |
| LED RGB | Indicateur d'état | PWM |
| Relais 5 V | Actionneur (ventilation / éclairage) | GPIO |

## Architecture matérielle

```mermaid
flowchart LR
    subgraph ESP32["ESP32 DevKit"]
        direction TB
        MCU["Cœur Xtensa<br/>FreeRTOS"]
        I2C["Bus I²C<br/>SDA=21 / SCL=22"]
        ADC["ADC1"]
        GPIO["GPIO"]
        PWM["LEDC (PWM)"]
        WIFI["Wi-Fi / TCP-IP"]
        FS["LittleFS<br/>(flash interne)"]
    end

    BME["BME280<br/>temp / hum / press"]
    OLED["OLED SSD1306<br/>128×64"]
    POT["Potentiomètre<br/>10 kΩ"]
    BTN["Bouton poussoir<br/>+ pull-up"]
    LED["LED RGB<br/>R / G / B"]
    RLY["Relais 5V<br/>charge externe"]

    I2C -->|0x76| BME
    I2C -->|0x3C| OLED
    ADC -->|GPIO 34| POT
    GPIO -->|GPIO 27 + IRQ| BTN
    PWM -->|GPIO 25/26/33| LED
    GPIO -->|GPIO 32| RLY

    MCU --- I2C
    MCU --- ADC
    MCU --- GPIO
    MCU --- PWM
    MCU --- WIFI
    MCU --- FS

    WIFI -.->|MQTT/HTTP| NET((Réseau))
```

## Architecture logicielle (FreeRTOS)

```mermaid
flowchart TB
    subgraph CORE0["Cœur 0"]
        TSENS["Task: SensorAcquisition<br/>prio 3 — 1 Hz"]
        TSUP["Task: Supervision<br/>prio 1 — 0.1 Hz"]
    end

    subgraph CORE1["Cœur 1"]
        TNET["Task: NetworkMQTT<br/>prio 4"]
        TWEB["Task: WebServer<br/>prio 2"]
        TSTO["Task: StorageReplay<br/>prio 2"]
    end

    QSENS[["Queue<br/>sensorDataQueue<br/>(SensorSample × 16)"]]
    QCMD[["Queue<br/>actuatorCmdQueue<br/>(ActuatorCmd × 8)"]]
    QOUT[["Queue<br/>outboundJsonQueue<br/>(payload × 32)"]]
    MI2C(("Mutex<br/>i2cMutex"))
    MFS(("Mutex<br/>fsMutex"))
    EVT{{"EventGroup<br/>netState<br/>WIFI_OK / MQTT_OK"}}

    TSENS -->|push sample| QSENS
    TSENS -->|JSON formaté| QOUT
    QSENS --> TWEB
    QSENS --> TSUP

    QOUT --> TNET
    TNET -->|si MQTT KO| TSTO
    TSTO -->|replay à reconnexion| TNET

    TWEB -->|cmd actionneur| QCMD
    TNET -->|cmd MQTT in| QCMD
    QCMD --> ACT["Actuators module<br/>(handler synchrone)"]

    TSENS -.->|prend| MI2C
    TSUP -.->|prend| MI2C
    TSTO -.->|prend| MFS
    TWEB -.->|prend| MFS

    TNET -.->|set/clear| EVT
    TSTO -.->|wait| EVT
    TSUP -.->|read| EVT
```

## Flux de données end-to-end

```mermaid
flowchart LR
    BME["BME280 / POT / BTN"] --> ACQ["SensorAcquisition<br/>filtrage + timestamp<br/>+ sanity check"]
    ACQ -->|valide| FMT["Format JSON imposé<br/>{device, ts, data}"]
    FMT --> RT{"MQTT<br/>connecté ?"}

    RT -->|oui| PUB["Publish QoS 1<br/>campus/&lt;grp&gt;/&lt;dev&gt;/data"]
    RT -->|non| BUF["LittleFS<br/>buffer.jsonl"]
    BUF -.->|à la reco| PUB

    PUB --> BRK["Broker MQTT<br/>auth user/pass"]
    BRK --> NR["Node-RED<br/>validation schéma"]
    NR --> MONGO[("MongoDB<br/>NoSQL")]
    NR --> DASH["Dashboard Node-RED<br/>live + cmd"]
    NR --> INFLUX[("InfluxDB<br/>bonus")]
    INFLUX --> GRAF["Grafana<br/>+ alerte absence/anomalie"]

    DASH -->|cmd actionneur| BRK
    BRK -->|topic .../cmd| SUB["MQTT subscribe<br/>NetworkMQTT"]
    SUB --> VAL["Validation JSON<br/>+ auth"]
    VAL --> ACT["Actuators<br/>LED RGB / Relais"]

    WEB["Browser local<br/>192.168.x.x"] -->|HTTP + token| AWS["AsyncWebServer<br/>/api/*"]
    AWS --> ACT
    AWS --> ACQ
    AWS --> CFG["Config MQTT<br/>(NVS)"]
```

## Structure du projet

```
src/
  main.cpp
  sensors/      # acquisition + filtrage + sanity check
  actuators/    # LED RGB, relais, OLED
  network/      # MQTT (PubSubClient) + Wi-Fi
  storage/      # LittleFS + buffer offline + replay
  web/          # AsyncWebServer + API
  security/     # auth, validation JSON, token API
data/
  index.html
  app.js
  style.css
```

## Contrat MQTT

- **Topic publish** : `campus/<groupe>/<deviceID>/data`
- **Topic subscribe (commandes)** : `campus/<groupe>/<deviceID>/cmd`
- **QoS** : 1 minimum
- **Auth** : user / password
- **Payload** (format imposé) :

```json
{
  "device": "ESP32-X",
  "ts": 0,
  "data": {
    "temp": 0,
    "humidity": 0
  }
}
```

## Badges visés

| Badge | Critère |
|---|---|
| 🟢 Sensor Engineer | Acquisition fiable + filtrage |
| 🔵 Network Engineer | MQTT robuste |
| 🟠 Embedded Architect | Multitâche propre |
| 🔴 Security Engineer | Validation + auth |
| 🟣 Full-Stack IoT | Web + Node-RED |
| ⚫ Reliability Engineer | Survit aux pannes |
| 🟡 Performance Engineer | Optimisation mémoire |

Bonus : **Grafana** (historisation + dashboard + alerte).

## Stack

- **Firmware** : PlatformIO + Arduino-ESP32 + FreeRTOS
- **MQTT** : PubSubClient
- **JSON** : ArduinoJson
- **Capteurs** : Adafruit BME280
- **OLED** : U8g2
- **Web** : ESPAsyncWebServer + AsyncTCP
- **Serveur** : Node-RED + MongoDB
- **Bonus** : InfluxDB + Grafana
