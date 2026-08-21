// luna_logic.h — Lógica pura del detector de salida de cama.
// Sin dependencias Arduino: se puede compilar y probar en PC (ver selftest.cpp).
#pragma once
#include <stdint.h>

enum LunaEvent {
  EV_NONE = 0,
  EV_LEFT,      // Luna salió de la cama (confirmado tras debounce)
  EV_RETURNED   // Luna volvió a la cama
};

struct LunaDet {
  bool     in_bed = true;        // estado actual confirmado
  bool     pending = false;      // hay un cambio candidato en curso
  bool     pending_out = false;  // dirección del cambio candidato (true = fuera)
  uint32_t pending_since = 0;    // millis() cuando empezó el candidato
  uint16_t baseline = 0;         // lectura ADC con Luna acostada (calibración)
  float    out_factor = 0.5f;    // lectura < baseline*out_factor => "fuera"
  uint32_t debounce_ms = 5000;   // tiempo sostenido para confirmar un cambio
};

// Llamar con cada lectura del FSR. `now` = millis().
// Debounce simétrico: un cambio se confirma solo si la lectura se sostiene
// en contra del estado actual durante debounce_ms sin interrupción.
// ponytail: umbral fijo derivado de la calibración de arranque; los FSR
// derivan con temperatura/edad — si hay falsas alarmas, recalibrar (/cal).
inline LunaEvent luna_update(LunaDet& d, uint16_t reading, uint32_t now) {
  bool out = reading < (uint16_t)(d.baseline * d.out_factor);
  bool currently_out = !d.in_bed;

  if (out == currently_out) {          // lectura coincide con estado: sin cambio
    d.pending = false;
    return EV_NONE;
  }
  if (!d.pending || d.pending_out != out) {  // cambio candidato nuevo
    d.pending = true;
    d.pending_out = out;
    d.pending_since = now;
    return EV_NONE;
  }
  if (now - d.pending_since < d.debounce_ms) return EV_NONE;  // aún no confirma

  d.pending = false;
  d.in_bed = !out;
  return out ? EV_LEFT : EV_RETURNED;
}
