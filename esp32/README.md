# Punto 3 — Frecuencia de CPU y tiempo de ejecución (ESP32)

## Qué se hizo

Se desarrolló un programa que ejecuta tres cargas de trabajo idénticas en estructura, variando únicamente el tipo de dato de la operación: `int32_t`, `float` y `double`. El programa calibra automáticamente cuántas iteraciones hacen falta para que cada prueba dure aproximadamente 10 segundos a 240 MHz, y luego repite exactamente el mismo trabajo a 160, 80 y 40 MHz. La salida se imprime en formato CSV por el puerto serie.

El desarrollo, la carga a la placa y la captura de resultados se hicieron íntegramente desde la terminal de Linux con `arduino-cli`.

## Por qué un microcontrolador

La frecuencia de reloj marca el ritmo al que el procesador ejecuta su ciclo de trabajo: a mayor cantidad de ciclos por segundo, mayor cantidad de instrucciones completadas en el mismo tiempo. En un procesador de escritorio esta frecuencia la administra el sistema operativo de forma automática, lo que dificulta medir su efecto de manera controlada. El ESP32 permite fijarla por software desde el propio programa mediante `setCpuFrequencyMhz()`, lo que hace posible observar de forma aislada cómo cambia la capacidad de cómputo cuando lo único que se modifica es la velocidad del reloj.

Se distinguió entre enteros y punto flotante porque se resuelven por unidades distintas del procesador y no necesariamente escalan de la misma manera.

## Archivos

| Archivo | Contenido |
| :-- | :-- |
| `README.md` | Este documento: qué se hizo, resultados y conclusiones |
| `codigo.md` | Código fuente del benchmark y salida cruda del monitor serie |

## Decisiones de implementación

**Medición del tiempo.** Se usa `esp_timer_get_time()`, que cuenta a 1 MHz con un timer independiente de la CPU. Si se midiera en ciclos de procesador, el resultado sería el mismo a cualquier frecuencia y el experimento no mostraría nada.

**Evitar que el compilador borre el bucle.** Un `for` que acumula un valor que después no se usa es código muerto, y GCC lo elimina; peor aún, una suma de enteros la puede reemplazar por la fórmula de Gauss y resolverla en tiempo constante. Por eso el paso que se suma está declarado `volatile`, lo que obliga a releerlo de memoria en cada vuelta, y el acumulador se guarda al final en otra variable `volatile`. El costo extra que esto introduce es el mismo en las tres pruebas, así que la comparación entre tipos sigue siendo justa.

**El UART al bajar la frecuencia.** Por encima de 80 MHz el bus APB queda fijo, pero por debajo pasa a seguir a la CPU, y el UART cuelga del APB. Sin corregirlo, el monitor serie muestra basura a 40 MHz. Por eso después de cada cambio de frecuencia se llama a `Serial.updateBaudRate(115200)`.

## Comandos utilizados

```bash
# Compilar
arduino-cli compile --fqbn esp32:esp32:esp32 bench_freq

# Cargar a la placa
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 bench_freq

# Capturar la salida
arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200 | tee resultados.csv
```

## Resultados

| Frecuencia | Tipo | Iteraciones | Tiempo (s) | Miter/s | Ciclos/iter |
| :-- | :-- | --: | --: | --: | --: |
| 240 MHz | int | 125.836.814 | 10,000 | 12,58 | 19,1 |
| 240 MHz | float | 140.635.108 | 9,999 | 14,06 | 17,1 |
| 240 MHz | double | 29.516.635 | 9,967 | 2,96 | 81,1 |
| 160 MHz | int | 125.836.814 | 15,028 | 8,37 | 19,1 |
| 160 MHz | float | 140.635.108 | 15,028 | 9,36 | 17,1 |
| 160 MHz | double | 29.516.635 | 14,980 | 1,97 | 81,2 |
| 80 MHz | int | 125.836.814 | 30,227 | 4,16 | 19,2 |
| 80 MHz | float | 140.635.108 | 30,225 | 4,65 | 17,2 |
| 80 MHz | double | 29.516.635 | 30,135 | 0,98 | 81,6 |
| 40 MHz | int | 125.836.814 | 61,145 | 2,06 | 19,4 |
| 40 MHz | float | 140.635.108 | 61,146 | 2,30 | 17,4 |
| 40 MHz | double | 29.516.635 | 60,981 | 0,48 | 83,3 |

La columna de ciclos por iteración no sale del CSV: se calculó como `frecuencia / Miter_s` para verificar el escalado.

### Escalado del tiempo respecto de 240 MHz

| Frecuencia | Factor teórico | Factor medido (int) |
| :-- | --: | --: |
| 240 MHz | 1,00× | 1,00× |
| 160 MHz | 1,50× | 1,50× |
| 80 MHz | 3,00× | 3,02× |
| 40 MHz | 6,00× | 6,11× |

## Conclusiones

El tiempo de ejecución escala casi perfectamente con la inversa de la frecuencia: al variar el reloj en un factor de 6, los ciclos por iteración se mantienen constantes, lo que confirma que el trabajo es CPU-bound puro, sin cuellos de botella de memoria. La única desviación aparece a 40 MHz, con 61,15 s medidos contra 60,0 s teóricos (~1,9 % de sobrecosto), atribuible al acceso a flash, que no escala junto con la CPU.

La precisión doble costó 4,75 veces más que la simple (2,96 contra 14,06 Miter/s), algo esperable dado que el ESP32 tiene FPU de simple precisión por hardware y emula la doble precisión por software.

Por último, `float` resultó más rápido que `int` (14,06 contra 12,58 Miter/s). Aunque parezca contraintuitivo, se explica porque la FPU trabaja en paralelo a la ALU, mientras que el bucle de enteros arrastra una dependencia acumulada sobre `step_i` que obliga a esperar el resultado de cada iteración antes de continuar.

## Verificación en el ensamblador

Para confirmar que las diferencias entre tipos vienen de las instrucciones que efectivamente genera el compilador, se desensambló el binario:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 bench_freq --build-path /tmp/build_bench
OBJDUMP=$(find ~/.arduino15/packages/esp32/tools -name 'xtensa-esp32-elf-objdump' | head -1)
$OBJDUMP -d /tmp/build_bench/bench_freq.ino.elf > bench.asm
sed -n '/<_Z11bench_floaty>:/,/^$/p'  bench.asm
sed -n '/<_Z12bench_doubley>:/,/^$/p' bench.asm
```

| Prueba | Instrucción que hace la suma en el bucle |
| :-- | :-- |
| `int` | `add.n a5, a5, a10` — ALU entera |
| `float` | `add.s f0, f0, f1` — una sola instrucción de la FPU |
| `double` | `callx8` a `__adddf3` — llamada a una rutina de software |

Esto explica directamente la diferencia medida: `float` cuesta una instrucción, mientras que `double` cuesta una llamada a función con decenas de instrucciones enteras adentro. De paso se comprueba que los tres bucles siguen existiendo en el binario, es decir que el compilador no los optimizó afuera.

## Nota sobre las frecuencias disponibles

En el ESP32 clásico son válidas 240, 160 y 80 MHz (derivadas del PLL) y 40, 20 y 10 MHz (divisiones del cristal de 40 MHz). Se eligieron las primeras cuatro. `setCpuFrequencyMhz()` devuelve `false` si se pide una frecuencia inexistente, y el programa la saltea imprimiendo un aviso.
