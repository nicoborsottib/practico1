# Punto 3 — Frecuencia de CPU vs tiempo de ejecución (ESP32)

Todo desde la terminal de Linux, con `arduino-cli` (no hace falta la IDE gráfica).

---

## 0. Estado actual

Ya está hecho por vos:

- `arduino-cli` instalado en `~/.local/bin/arduino-cli`
- core `esp32:esp32` instalándose
- sketch listo en `bench_freq/bench_freq.ino`

Falta lo que requiere tu contraseña y la placa enchufada (pasos 1 y 2).

---

## 1. Poner `arduino-cli` en el PATH

```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
arduino-cli version
```

---

## 2. Permisos del puerto serie (esto es LO típico que falla en Linux)

En Windows era un COMx y andaba solo. En Linux el puerto es `/dev/ttyUSB0` (chips
CP2102 / CH340) o `/dev/ttyACM0` (USB nativo del S3/C3), y **pertenece al grupo
`dialout`**. Vos no estás en ese grupo todavía.

```bash
sudo usermod -aG dialout $USER
```

Después **cerrá sesión y volvé a entrar** (o reiniciá). Para probar sin
desloguearte, en la terminal donde vas a trabajar:

```bash
newgrp dialout
id -nG          # tiene que aparecer "dialout"
```

Enchufá la placa y verificá:

```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
dmesg | tail -20            # muestra qué driver la tomó (cp210x, ch341, cdc_acm)
arduino-cli board list
```

> Si el puerto no aparece: probá otro cable USB (muchos son sólo de carga, sin datos).
> `brltty` no está instalado en tu sistema, así que ese problema clásico con los
> CH340 no te va a pasar.

---

## 3. Identificar tu placa

```bash
arduino-cli board listall esp32 | head -30
```

FQBN habituales:

| Placa | FQBN |
|---|---|
| ESP32 genérica (WROOM-32, DevKit V1) | `esp32:esp32:esp32` |
| ESP32-S3 DevKit | `esp32:esp32:esp32s3` |
| ESP32-C3 DevKit | `esp32:esp32:esp32c3` |

**Importante para el TP:** el ESP32 clásico y el S3 son Xtensa y **tienen FPU de
simple precisión por hardware**. El **C3 es RISC-V sin FPU**: ahí float *también*
se emula por software y no vas a ver la diferencia float/double tan marcada.

---

## 4. Compilar

```bash
cd ~/Escritorio/SdeComp/paso_3_esp32
arduino-cli compile --fqbn esp32:esp32:esp32 bench_freq
```

## 5. Cargar a la placa

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 bench_freq
```

> Si se queda en `Connecting........_____`: mantené apretado **BOOT**, tocá
> **EN/RST**, soltá BOOT, y reintentá.

## 6. Ver los resultados

```bash
arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200
```

Para guardar la salida en un archivo y después graficarla:

```bash
arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200 | tee resultados.csv
```

(Cortás con `Ctrl+C` cuando imprime `# fin`.)

El barrido completo tarda unos 3–4 minutos: 4 frecuencias × 3 pruebas de ~10 s
a 240 MHz, y las pruebas se hacen más largas a medida que baja la frecuencia.

---

## Qué hace el código

- Calibra automáticamente cuántas iteraciones hacen falta para que cada prueba
  dure **~10 s a 240 MHz**, y después repite **exactamente el mismo trabajo** a
  160, 80 y 40 MHz.
- Tres bucles idénticos en estructura, cambiando sólo el tipo: `int32_t`,
  `float`, `double`.
- Mide con `esp_timer_get_time()`, que cuenta a 1 MHz con un timer **independiente
  de la frecuencia de CPU** — si midieras con ciclos de CPU no verías nada.
- Imprime CSV: `freq_mhz,tipo,iteraciones,tiempo_s,Miter_s`.

### Dos trampas que el código ya evita

1. **El compilador borra el bucle.** Un `for` que suma y cuyo resultado no se usa
   es código muerto; GCC lo elimina y te da 0 segundos. Peor: una suma de enteros
   la puede reemplazar por la fórmula de Gauss. Por eso el "paso" que se suma es
   `volatile` (obliga a releerlo cada vuelta) y el acumulador se guarda al final
   en otra variable `volatile`.
2. **El UART se descuadra al bajar la frecuencia.** Arriba de 80 MHz el bus APB
   queda fijo en 80 MHz, pero por debajo el APB sigue a la CPU, y el UART cuelga
   del APB. Por eso después de cada `setCpuFrequencyMhz()` se llama a
   `Serial.updateBaudRate(115200)`. Si igual ves basura a 40 MHz, sacá el `40`
   de la lista `FREQS[]`.

---

## Qué tenés que observar (respuesta al ejercicio)

### a) Tiempo vs frecuencia

El tiempo escala **inversamente con la frecuencia**: al **duplicar** la frecuencia,
el tiempo se va **a la mitad**. Esperá algo así:

| Frecuencia | Tiempo (mismo trabajo) | Factor |
|---|---|---|
| 240 MHz | ~10 s | 1× |
| 160 MHz | ~15 s | 1,5× |
| 80 MHz | ~30 s | 3× |
| 40 MHz | ~60 s | 6× |

Es decir `t ≈ (ciclos de la tarea) / f_cpu`. La cantidad de **ciclos** no cambia:
lo único que cambia es cuánto dura cada ciclo. Si graficás `1/tiempo` contra la
frecuencia te tiene que dar una **recta que pasa por el origen**.

**Dónde se rompe la proporción:** si la tarea tocara mucha memoria externa (flash
/ PSRAM) o periféricos, esos no siguen al reloj de CPU y la mejora sería menor que
lineal — el clásico "el procesador espera a la memoria". Esta prueba es a propósito
puro cómputo en RAM interna para que la relación se vea limpia.

### b) int vs float vs double

Comparando `Miter_s` (millones de iteraciones por segundo) a una misma frecuencia:

- **int** y **float** dan valores **parecidos**: el ESP32 (Xtensa LX6) tiene
  **FPU de simple precisión por hardware**, así que un `add.s` de float cuesta
  prácticamente lo mismo que un `add` entero.
- **double** cae **muy por debajo** (típicamente entre 5× y 20× más lento): el
  ESP32 **no tiene** FPU de doble precisión, así que cada suma se resuelve con una
  rutina de software del compilador (`__adddf3`) — decenas de instrucciones
  enteras para hacer una sola suma.

Conclusión para el informe: **la frecuencia cambia cuántos ciclos por segundo
ejecutás; el soporte de hardware cambia cuántos ciclos necesita cada operación.**
Son dos ejes independientes, y por eso pasar de `double` a `float` en un ESP32
puede darte más aceleración que duplicar el reloj.

### Verificación en el ensamblador (esto queda muy bien en el informe)

No hace falta ni la placa: se ve en el binario compilado.

```bash
cd ~/Escritorio/SdeComp/paso_3_esp32
arduino-cli compile --fqbn esp32:esp32:esp32 bench_freq --build-path /tmp/build_bench
OBJDUMP=$(find ~/.arduino15/packages/esp32/tools -name 'xtensa-esp32-elf-objdump' | head -1)
$OBJDUMP -d /tmp/build_bench/bench_freq.ino.elf > bench.asm

# los simbolos estan "manglados" por C++: bench_float -> _Z11bench_floaty
sed -n '/<_Z11bench_floaty>:/,/^$/p'  bench.asm    # float
sed -n '/<_Z12bench_doubley>:/,/^$/p' bench.asm    # double
```

Resultado ya verificado con el core esp32 3.3.11:

| Prueba | Instrucción que hace la suma en el bucle |
|---|---|
| `int` | `add.n a5, a5, a10` — ALU entera |
| `float` | **`add.s f0, f0, f1`** — una sola instrucción de la FPU |
| `double` | **`callx8` a `__adddf3`** — llamada a una rutina de software |

Esa tabla *es* la explicación del punto b): `float` cuesta una instrucción,
`double` cuesta una llamada a función con decenas de instrucciones enteras
adentro. Y de paso se comprueba que los tres bucles siguen existiendo, o sea que
el compilador no los optimizó afuera.

---

## Otras frecuencias

Válidas en el ESP32 clásico: **240, 160, 80** (desde el PLL) y **40, 20, 10**
(divisiones del cristal de 40 MHz). Editá el array `FREQS[]` arriba del sketch.
`setCpuFrequencyMhz()` devuelve `false` si pedís una que no existe, y el sketch
la saltea e imprime un aviso.
