# esp32-secure-iot-station

Station IoT autonome et sécurisée — **supervision climatique & sécurité d'armoires
techniques** (ESP32 + FreeRTOS). Mesure température/humidité, pilote la ventilation
(relais) par seuil, signale l'état (LEDs), gère un arrêt d'urgence, et remonte vers
un serveur central (MQTT → Node-RED → MongoDB/InfluxDB/Grafana) avec survie aux pannes.

Projet du module **Système IoT** — Master II.

## 📚 Documentation

| Doc | Contenu |
|---|---|
| [`docs/RAPPORT-TECHNIQUE.md`](docs/RAPPORT-TECHNIQUE.md) | Rapport de synthèse (≤ 5 pages) + couverture des badges |
| [`docs/SPEC-METIER.md`](docs/SPEC-METIER.md) | Contexte métier + découpage des opérations FreeRTOS |
| [`docs/SPEC-TECHNIQUE.md`](docs/SPEC-TECHNIQUE.md) | Architecture, stack, diagrammes, contrat MQTT, sécurité/perf |
| [`docs/SETUP.md`](docs/SETUP.md) | Mise en route pas à pas (câblage + flash + serveur) |
| [`server/README.md`](server/README.md) | Stack serveur Docker (broker, Node-RED, bases, Grafana) |

## Fonctionnalités

- **Acquisition** DHT22 filtrée + timestamp + détection d'aberrations
- **Régulation** ventilation par hystérésis + voyants d'état (🟢🟠🔴)
- **Arrêt d'urgence** par contact 2 fils (réaction < 50 ms)
- **MQTT robuste** : QoS 1, LWT, reconnexion auto, **mode offline** (buffer + replay)
- **Interface web** embarquée sécurisée (token) : live, config à chaud, commandes
- **Serveur** Node-RED + MongoDB + dashboard, **bonus** InfluxDB + Grafana (+ alertes)
- **8 tâches FreeRTOS** (acquisition / régulation / publication découplées)

## Matériel

| Composant | Rôle | Interface |
|---|---|---|
| ESP32-WROOM-32 | MCU (Xtensa LX6 dual-core @ 240 MHz) | — |
| DHT22 (AM2302) | Température / Humidité | 1-wire (GPIO 4) |
| 3 LEDs (rouge/orange/verte) | Voyant d'état | PWM (GPIO 25/26/33) |
| Relais 5 V | Ventilation | GPIO 32 |
| Contact 2 fils | Arrêt d'urgence | GPIO 27 + IRQ |

> Composants non disponibles remplacés en logiciel : **OLED → OLED virtuel** (web),
> **potentiomètre → seuil web** (NVS). Détails dans la spec technique.

## Démarrage rapide

1. **Firmware** : `cp include/secrets.example.h include/secrets.h` (remplir Wi-Fi/MQTT),
   puis `pio run -t upload && pio run -t uploadfs`.
2. **Serveur** : `cd server && docker compose up -d`.

Procédure complète (câblage compris) : [`docs/SETUP.md`](docs/SETUP.md).

## Badges visés

🟢 Sensor · 🔵 Network · 🟠 Embedded · 🔴 Security · 🟣 Full-Stack · ⚫ Reliability ·
🟡 Performance · ★ Bonus Grafana
