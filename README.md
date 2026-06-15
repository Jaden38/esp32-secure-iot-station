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
        MCU["CPU Xtensa LX6<br/>dual-core @ 240 MHz<br/>FreeRTOS SMP"]
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

> **À propos du « CPU Xtensa LX6 »**
> L'ESP32 (modèle classique, ex. ESP32-WROOM-32) embarque un processeur **Xtensa LX6** conçu par **Tensilica / Cadence**. C'est une architecture 32 bits **configurable** (Tensilica vend des cœurs « à la carte »), spécialement adaptée au DSP et au calcul embarqué.
> Le SoC est **dual-core** : deux cœurs Xtensa LX6 identiques cadencés jusqu'à **240 MHz**, exposés à FreeRTOS comme **Cœur 0 (PRO_CPU)** et **Cœur 1 (APP_CPU)**.
> - **Cœur 0 (PRO_CPU)** héberge par défaut la stack Wi-Fi / Bluetooth / IP de l'IDF. Mais avec Arduino-ESP32, **le sketch (`setup()` / `loop()`) tourne sur le Cœur 1 (APP_CPU)** et la stack réseau garde le Cœur 0.
> - C'est pour cette raison qu'on **épingle (`xTaskCreatePinnedToCore`) `NetworkMQTT` sur le Cœur 1** (à côté de la stack réseau Arduino) et qu'on isole l'**acquisition capteurs** sur le Cœur 0 pour qu'elle ne soit pas perturbée par les interruptions Wi-Fi.
> - Le runtime est **FreeRTOS SMP** (ordonnanceur multi-cœurs préemptif), c'est pour ça que `vTaskDelay()` est obligatoire à la place de `delay()` : `delay()` Arduino fait un yield FreeRTOS, mais sur du code multitâche bien écrit on veut être explicite.
>
> ℹ️ Sur les modèles plus récents (ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2), Espressif est passé à des cœurs **Xtensa LX7** ou **RISC-V**. Le code FreeRTOS de ce projet reste portable sur ces variantes moyennant un changement de `board` dans `platformio.ini`.

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
