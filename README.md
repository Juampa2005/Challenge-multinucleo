# DO 08 — Sistema multitarea cooperativo (Raspberry Pi Pico W)

Juan Pablo Ruiz GIl
Marco Uriel Castaneda Avila
Implementación en C con Raspberry Pi Pico SDK de un sistema que lee un valor analógico, lo suaviza con un promedio móvil y señala tres niveles mediante LEDs.

## Funciones

- Lectura del ADC cada 20 ms (50 Hz).
- Promedio móvil de las últimas 8 muestras, usando un buffer circular.
- Conversión del valor filtrado a porcentaje.
- Clasificación del sistema:
  - `NORMAL`: porcentaje menor que 50%.
  - `WARNING`: porcentaje desde 50% y menor que 75%.
  - `ALARM`: porcentaje igual o mayor que 75%.
- Actualización de LEDs y heartbeat cada 200 ms (5 Hz).
- Registro por consola cada 1000 ms (1 Hz).
- Lectura del botón cada 50 ms (20 Hz).
- Reconocimiento de alarma (`ACK`) mediante detección del flanco de bajada del botón.
- Planificador cooperativo: las tareas se ejecutan cuando vence su periodo, sin RTOS.

## Conexiones

| Componente | GPIO de la Pico W | Observaciones |
|---|---:|---|
| Entrada analógica / potenciómetro o sensor | GP26 / ADC0 | El cursor del potenciómetro va a GP26; sus extremos a 3V3 y GND. |
| LED NORMAL | GP13 | Usar resistencia limitadora (por ejemplo, 220–1 kΩ). |
| LED WARNING | GP14 | Usar resistencia limitadora. |
| LED ALARM | GP15 | Usar resistencia limitadora. |
| Heartbeat | GP12 | Usar resistencia si es un LED externo. |
| Botón | GP16 y GND | El programa activa la resistencia interna `PULL_UP`; presionado = nivel bajo. |

**Importante:** no conectes ningún GPIO directamente a 5 V. La entrada analógica debe permanecer entre GND y 3V3.

## Estructura del proyecto

```text
do08_pico_w/
├── CMakeLists.txt
└── main.c
```

## Requisitos

- Raspberry Pi Pico SDK instalado y configurado.
- CMake y una cadena de compilación compatible con RP2040.
- Una Raspberry Pi Pico W seleccionada como placa de compilación.

## Compilar

Desde una terminal situada en la carpeta del proyecto:

```bash
mkdir build
cd build
cmake .. -DPICO_BOARD=pico_w
cmake --build .
```

El resultado habitual es `do08_pico_w.uf2` dentro de `build/` (o en la carpeta de salida configurada por tu entorno). Copia el archivo UF2 a la Pico W en modo BOOTSEL.

Si ya tienes un proyecto creado en VS Code con la extensión Raspberry Pi Pico, puedes copiar `main.c` a tu proyecto y conservar la configuración de SDK que ya funciona.

## Probar

1. Conecta el potenciómetro o sensor analógico a GP26.
2. Conecta los LEDs y el botón de acuerdo con la tabla.
3. Flashea el programa y abre el monitor serial.
4. Gira el potenciómetro para cambiar el porcentaje:
   - Menos de 50%: LED NORMAL encendido.
   - De 50% a menos de 75%: LED WARNING encendido.
   - 75% o más: LED ALARM parpadea.
5. Presiona el botón mientras el estado sea `ALARM`: se activa `ACK=True` y el LED de alarma queda encendido sin parpadear.
6. Cuando el valor vuelve a un estado distinto de `ALARM`, el ACK se borra automáticamente.
7. La consola imprime valores `raw`, `filtered`, `percent`, `state`, `samples` y `ack`.

## Cómo funciona el planificador

El bucle principal consulta el tiempo transcurrido y ejecuta cada tarea cuando vence su periodo. Las tareas no esperan durante cientos de milisegundos; al terminar, el bucle vuelve a comprobar las demás tareas. `sleep_ms(1)` solo reduce el uso de CPU y no reemplaza al planificador.

| Tarea | Periodo | Frecuencia aproximada |
|---|---:|---:|
| Sensor y filtro | 20 ms | 50 Hz |
| Interfaz de LEDs | 200 ms | 5 Hz |
| Registro serial | 1000 ms | 1 Hz |
| Botón | 50 ms | 20 Hz |

## Notas de implementación

- El ADC del RP2040 devuelve un entero de 12 bits (`0` a `4095`), mientras que MicroPython `read_u16()` devuelve un valor escalado de `0` a `65535`. La conversión a porcentaje es equivalente usando el máximo correcto para cada plataforma.
- El buffer circular evita desplazar los datos en cada lectura.
- El botón se muestrea cada 50 ms, como en la versión original. Esto ofrece un filtrado temporal básico, pero no es un sistema antirrebote robusto para todas las situaciones.
- El heartbeat cambia de nivel cada vez que se ejecuta `ui_task`, por lo que alterna cada 200 ms.
- La versión C usa `printf` para enviar los registros por USB serial. Asegúrate de que la salida serial USB esté habilitada en la configuración de tu proyecto/SDK.
