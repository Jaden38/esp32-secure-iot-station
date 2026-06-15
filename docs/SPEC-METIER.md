# Spécification métier & opérations FreeRTOS

## Contexte métier

**Supervision climatique & sécurité d'armoires techniques / mini-locaux serveurs.**

Dans une armoire électrique/réseau, la chaleur et l'humidité menacent les
équipements. La station autonome :

- mesure **température & humidité** (DHT22) ;
- **refroidit automatiquement** via un **relais** (ventilateur) selon un seuil
  réglable, avec **hystérésis** ;
- signale l'état localement par une **LED RGB** (🟢 nominal · 🟠 ventilation/
  humidité · 🔴 alarme/arrêt d'urgence) ;
- offre un **arrêt d'urgence** par **contact 2 fils** (maintenance/sécurité) ;
- **publie** vers un serveur central (MQTT → Node-RED → MongoDB/InfluxDB/Grafana)
  et **survit aux coupures** (buffer offline + replay).

### Règles métier

| Règle | Détail |
|---|---|
| Ventilation ON | température ≥ `tempOn` |
| Ventilation OFF | température ≤ `tempOn − hysteresis` (anti-battement) |
| Alerte humidité | humidité > `humAlert` → LED orange + (remontée MQTT) |
| Arrêt d'urgence | contact fermé → relais **coupé** immédiatement, LED rouge, reset manuel |
| Mode manuel | l'opérateur force le relais ; la régulation auto est suspendue |

## Découpage FreeRTOS (opérations isolées & configurables)

Chaque opération est une tâche indépendante, avec sa **cadence propre** et sa
**config modifiable à chaud** (UI web → NVS → relue en début de boucle).

| Tâche | Cadence | Prio | Cœur | Rôle |
|---|---|---|---|---|
| **SensorAcquisition** | `acqPeriodMs` (déf. 2 s) | 3 | 0 | DHT22 + contact, filtrage, sanity, cache |
| **Control** (régulation) | `ctrlPeriodMs` (déf. 1 s) | 3 | 0 | hystérésis relais + LED état + commandes manuelles |
| **Safety** (arrêt d'urgence) | **événementiel** (< 50 ms) | **5** | 0 | contact → relais OFF + `BIT_ESTOP` |
| **Telemetry** | `pubPeriodMs` (déf. 10 s) | 2 | 1 | publie un instantané MQTT (découplé de l'acquisition) |
| **NetworkMQTT** | continue | 4 | 1 | Wi-Fi/MQTT, transport, reconnexion, offline |
| **WebServer** | callbacks | 2 | 1 | UI + API (config à chaud) |
| **StorageReplay** | sur `MQTT_OK` | 2 | 1 | rejoue le buffer offline |
| **Supervision** | 10 s | 1 | 0 | heap/uptime/latence (série + OLED virtuel) |

**Point clé** : l'**acquisition (2 s)**, la **régulation (1 s)** et la
**publication (10 s)** sont **découplées** ; l'**arrêt d'urgence** est
**prioritaire et instantané** (réveil par interruption, pas de polling).

### Synchronisation inter-tâches

- **Cache** `sensorsGetLatest()` (mutex) : fan-out de la dernière mesure vers
  Control, Telemetry, Web, Supervision.
- **EventGroup** `appState` / `BIT_ESTOP` : Safety positionne, Control & Telemetry
  & Supervision lisent.
- **Sémaphore** `estopSem` : donné par l'ISR du contact → réveille Safety.
- **Queue** `actuatorCmdQueue` : commandes web/MQTT → drainées par Control.
- **Queue** `outboundJsonQueue` : Telemetry → Network → MQTT/offline.

## Paramètres configurables (UI web `/api/control`, NVS)

```
acqPeriodMs, ctrlPeriodMs, pubPeriodMs   (périodes des opérations)
tempOn, hysteresis, humAlert             (seuils de régulation)
mode (auto|manuel), relayManual          (commande)
estopAutoReset                           (réarmement estop)
```

Modifier une valeur dans la page web l'applique **sans reflasher** : chaque
tâche relit sa configuration au début de sa boucle.
