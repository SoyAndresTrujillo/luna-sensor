// selftest.cpp — Prueba de la lógica de luna_logic.h en PC, sin Arduino.
// Compilar y correr:  g++ -std=c++11 selftest.cpp -o selftest && ./selftest
#include <cassert>
#include <cstdio>
#include "luna_logic.h"

int main() {
  LunaDet d;
  d.baseline = 3000;      // umbral = 3000 * 0.5 = 1500
  d.out_factor = 0.5f;
  d.debounce_ms = 5000;

  // 1. Lecturas altas sostenidas -> nunca evento
  for (uint32_t t = 0; t <= 10000; t += 100)
    assert(luna_update(d, 2500, t) == EV_NONE);
  assert(d.in_bed);

  // 2. Lectura baja sostenida: nada antes de 5 s, EV_LEFT justo al cumplirse
  uint32_t t = 20000;
  for (; t < 25000; t += 100) assert(luna_update(d, 500, t) == EV_NONE);
  assert(luna_update(d, 500, t) == EV_LEFT);   // t == 25000
  assert(!d.in_bed);
  // 3. Sigue fuera: no repite evento
  for (t += 100; t < 30000; t += 100) assert(luna_update(d, 500, t) == EV_NONE);

  // 4. Pico breve alto (se mueve y vuelve a bajar) -> no confirma regreso
  assert(luna_update(d, 2500, t) == EV_NONE);  // t = 30000, inicia candidato
  for (t = 30100; t < 34000; t += 100) assert(luna_update(d, 500, t) == EV_NONE);
  assert(!d.in_bed);

  // 5. Lectura alta sostenida -> EV_RETURNED al cumplirse debounce
  //    (el candidato de regreso arranca en t = 34000, confirma en 39000)
  for (; t < 39000; t += 100) assert(luna_update(d, 2500, t) == EV_NONE);
  assert(luna_update(d, 2500, t) == EV_RETURNED);  // t == 39000
  assert(d.in_bed);

  // 6. Salida inmediata tras regreso -> EV_LEFT otra vez
  //    (candidato arranca en 39100, confirma en 44100)
  for (t += 100; t < 44100; t += 100) assert(luna_update(d, 500, t) == EV_NONE);
  assert(luna_update(d, 500, t) == EV_LEFT);  // t == 44100

  // 7. Wraparound de millis(): candidato cerca de UINT32_MAX
  LunaDet d2 = d;
  d2.in_bed = true; d2.pending = false;
  uint32_t w = 0xFFFFF000u;
  assert(luna_update(d2, 500, w) == EV_NONE);          // inicia candidato
  assert(luna_update(d2, 500, w + 6000) == EV_LEFT);   // cruza el wrap
  printf("selftest OK\n");
  return 0;
}
