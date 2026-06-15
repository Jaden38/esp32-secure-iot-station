// ============================================================================
//  supervision/ — Supervision système (cf. cahier des charges §9).
//  Compose périodiquement une "trame écran" (heap, uptime, latence MQTT,
//  état Wi-Fi/MQTT) et la diffuse sur 3 sorties :
//    1. Serial (toujours)
//    2. OLED SSD1306 réel via U8g2   -> seulement si HAS_OLED=1 (i2cMutex)
//    3. OLED "virtuel" : la trame est exposée à web/ (/api/supervision) qui la
//       rend dans un panneau façon 128x64. -> écran manquant compensé en logiciel
//  Choix OLED : U8g2 (driver présent, activable par flag).
// ============================================================================
#pragma once

#include <Arduino.h>

// Géométrie de la trame texte (≈ SSD1306 128x64 en petite police).
static constexpr int SUP_LINES = 6;
static constexpr int SUP_COLS  = 21;

bool supervisionInit();

// Publie la dernière latence de publication MQTT mesurée (ms), thread-safe.
void supervisionReportPublishLatency(uint32_t ms);

// Copie la trame courante (lignes jointes par '\n') pour l'OLED virtuel web.
// Retourne le nombre d'octets écrits (hors '\0'). Thread-safe.
size_t supervisionCopyFrame(char* out, size_t maxLen);

// Tâche FreeRTOS de supervision (CORE_SUPERVISION, PRIO_SUPERVISION).
void supervisionTask(void* pv);
