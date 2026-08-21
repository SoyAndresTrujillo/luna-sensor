# luna_sensor

Detector de salida de cama para Luna (perrita senior con demencia senil).
FSR bajo el cojín de su cama → ESP32 → mensaje a Telegram de quien esté
de turno (+ buzzer opcional) cuando ella se levanta, para ir a auxiliarla
antes de que se desoriente.

## Lista de compra (referencias exactas)

Buscar así en MercadoLibre Colombia o tienda de electrónica local:

| # | Buscar exactamente | Qué verificar | Precio aprox COP |
|---|---|---|---|
| 1 | `NodeMCU ESP32 ESP-32S DevKit` o `ESP32 DevKit V1` | Que diga ESP-WROOM-32; chip USB CP2102 o CH340; 30 u 38 pines (ambos sirven) | $25.000–40.000 |
| 2 | `Sensor FSR 406` | Cuadrado ~38×38 mm. NO el FSR 402 (redondo, zona muy pequeña) | $34.000–55.000 |
| 3 | `resistencia 10k ohm 1/4W` | Una sola basta; kit surtido sirve | $500–10.000 |
| 4 | `cables dupont macho-hembra` | Paquete de 40 | $6.000–10.000 |
| 5 | `protoboard 400 puntos` | Opcional pero facilita el montaje | $8.000 |
| 6 | Cable micro-USB **de datos** + cargador 5V de celular | Ojo: cable solo-de-carga NO sirve para programar | (seguro ya tienes) |
| 7 | `buzzer activo 5V` | Opcional: alarma sonora local | $6.000 |

Total sin opcionales: ~$75.000–95.000 COP (~18–23 USD).

Referencia: [ESP32 en MercadoLibre CO](https://listado.mercadolibre.com.co/nodemcu-esp32), [FSR 406 en MercadoLibre CO](https://listado.mercadolibre.com.co/sensor-fsr-406).

## Cableado

Divisor de voltaje (más peso → lectura ADC más alta):

```
3.3V ----[FSR]----+---- GPIO34
                  |
               [10 kΩ]
                  |
GND --------------+

Buzzer (opcional): GPIO26 ----[buzzer]---- GND
```

- FSR va **bajo el cojín/colchón**, en la zona donde Luna normalmente duerme.
- No doblar el FSR en ángulo brusco; la cola conecta hacia afuera de la cama.

## Crear el bot de Telegram (pasos exactos)

1. Abrir Telegram y buscar **@BotFather** (el oficial, con verificación azul).
2. Enviar `/start`, luego `/newbot`.
3. Te pide un **nombre visible**: escribe ej. `Luna Sensor`.
4. Te pide un **username** único terminado en `bot`: escribe ej. `luna_familia_bot`.
5. BotFather responde con el **token**, formato `7123456789:AAHjK...`.
   → cópialo en la constante `BOT_TOKEN` del `.ino`.
6. **Cada persona que recibirá alertas** (tú y tu hermana): abrir el bot
   en su Telegram y presionar **Iniciar** (o enviar `/start`).
   Sin esto el bot no puede escribirles.
7. Sacar el `chat_id` de cada una:
   - Fácil: cada una le escribe a **@userinfobot** y les da su número.
   - O: abrir en navegador `https://api.telegram.org/bot<TOKEN>/getUpdates`
     (reemplazar `<TOKEN>`) y buscar `"chat":{"id":` — se identifica por `first_name`.
8. Los dos números van en `CHAT_IDS` del `.ino`:
   `const char* CHAT_IDS[] = { "111111111", "222222222" };`

## Subir el firmware

1. Arduino IDE → instalar soporte ESP32 (Espressif, via Boards Manager).
   No requiere librerías externas.
2. Editar `WIFI_SSID`, `WIFI_PASS`, `BOT_TOKEN`, `CHAT_IDS` en `luna_sensor.ino`.
3. Subir al ESP32 con Luna **acostada en su cama** (calibra al arrancar).
   Deben llegar los mensajes "luna_sensor listo. Baseline=NNNN" a ambos Telegram.

## Uso

- Luna sale de la cama >5 s → "ALERTA: Luna salio de la cama." a ambos.
  Se repite cada 2 min hasta que vuelva. Si no hay WiFi, reintenta solo.
- Luna vuelve → "Luna volvio a la cama."
- Recalibrar (cambio de cojín, falsas alarmas): acostar a Luna y mandarle
  `cal` al bot desde Telegram.

## Turnos: quién despierta cada noche

El sensor avisa a **ambos** teléfonos siempre. El turno se maneja por teléfono,
sin tocar código:

- **Quien NO está de turno**: silenciar el chat del bot
  (chat del bot → tocar el nombre arriba → Silenciar → Siempre).
  Su teléfono no suena nunca.
- **Quien SÍ está de turno**: quitar el silencio + configuración anti-silencio
  de abajo.
- Cambio de turno = cada quien ajusta su propio teléfono. Listo.

### iPhone (tú) — que suene en modo Sueño

1. Ajustes → **Enfoque** → **Sueño** → **Apps** → permitir notificaciones de
   → agregar **Telegram**. (Repetir en "No molestar" si también lo usas.)
2. Telegram → chat del bot → tocar el nombre → **Notificaciones** →
   sonido personalizado: elegir uno largo/fuerte.
3. Antes de dormir: volumen del timbre al máximo.
4. **Prueba de fuego**: activar modo Sueño, levantar el cojín 10 s,
   esperar → el iPhone debe sonar. Hacerla la primera noche.

### Android (tu hermana, fase futura)

Ajustes → Notificaciones → **No molestar** → Excepciones de apps → Telegram.
Mismo sistema de turnos: silenciar el chat cuando no le toca.

## Constantes del código (`luna_sensor.ino`)

| Constante | Default | Qué hace |
|---|---|---|
| `WIFI_SSID` / `WIFI_PASS` | — | Tu red WiFi de casa. |
| `BOT_TOKEN` | — | Token que te dio @BotFather. |
| `CHAT_IDS` | — | chat_id de cada persona que recibe alertas. |
| `OUT_FACTOR` | 0.5 | Lectura < 50% del baseline ⇒ salió. Subir si no detecta, bajar si hay falsas alarmas. |
| `DEBOUNCE_MS` | 5000 | Tiempo sostenido para confirmar cambio. Subir si Luna se reacomoda mucho. |
| `REPEAT_MS` | 120000 | Cada cuánto repite la alerta si sigue fuera. |
| `CMD_POLL_MS` | 3000 | Cada cuánto revisa comandos Telegram (`cal`). |

## Test de la lógica

`selftest.cpp` prueba la máquina de estados (debounce, falsos picos, wraparound
de `millis()`) en PC, sin Arduino:

```
g++ -std=c++11 selftest.cpp -o selftest && ./selftest
```

## Fase 2 (ideas, no hechas)

- Comando `/turno` en el bot para que solo suene un teléfono sin silenciar manual.
- Llamada real que despierte: ESP32 → Twilio → llamada al cel
  (ambas plataformas dejan pasar llamadas de contactos favoritos en Enfoque/No molestar).
- Segundo ESP32 con buzzer en el cuarto de quien esté de turno, vía ESP-NOW.
- WhatsApp vía CallMeBot (gratis, pero más lento y sin llamada).
