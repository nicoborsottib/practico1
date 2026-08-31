# Trabajo Práctico N°1 — Rendimiento

Universidad Nacional de Córdoba — Facultad de Ciencias Exactas, Físicas y Naturales
Cátedra de Sistemas de Computación — Prof. Ing. Javier Jorge

## Integrantes

- Nicolas Borsotti Bosco
- Santiago Valentin Ciacci
- Ignacio Ariel Leguizamon

## Estructura del repositorio

```
practico1/
├── README.md
└── timer_profiling/
    ├── Nicolas Borsotti/
    ├── Santiago Ciacci/
    └── Ignacio Leguizamon/
└── esp32/
    ├──README.md/
    └──codigo.md/
```

Cada integrante tiene su propia carpeta dentro de `timer_profiling/` con los resultados del punto 4, documentados con capturas de pantalla de cada paso del tutorial.

---

## 1. Benchmarks de terceros

Se analizaron las tareas de uso cotidiano de cada integrante para elegir un benchmark representativo:

| Integrante | Tarea | Benchmark más representativo | Carga dominante |
| :-- | :-- | :-- | :-- |
| Ciacci | Fusion 360 | Blender Benchmark / Cinebench | CPU + GPU |
| Ciacci | Proteus 8 | Geekbench / 7-Zip | CPU |
| Ciacci | League of Legends | 3DMark | GPU |
| Borsotti | Proteus 8 | Geekbench / 7-Zip | CPU |
| Borsotti | Itero | 7-Zip / Geekbench | CPU |
| Borsotti | League of Legends | 3DMark | GPU |
| Leguizamon | Fusion 360 | Blender Benchmark | CPU + GPU |
| Leguizamon | CapCut | Blender Benchmark | CPU + GPU |
| Leguizamon | LTspice | Geekbench | CPU single-thread |
| Leguizamon | League of Legends | 3DMark | GPU |

Se seleccionó **7-Zip Benchmark** por ser liviano, correr de forma nativa tanto en Windows como en Linux, no requerir GPU ni cuentas externas, y permitir comparar directamente el rendimiento de CPU mediante una carga multinúcleo estandarizada. Cada integrante lo corrió con la configuración por defecto (diccionario de 32 MB, hilos detectados automáticamente).

### Resultados

| Integrante | CPU | Hilos | Rating total | Rating por hilo |
| :-- | :-- | :-- | :-- | :-- |
| Ciacci | Intel i9-14900HX | 32 | 143.323 GIPS | 4.479 GIPS/hilo |
| Borsotti | AMD Ryzen 7 5700U | 16 | 32.183 GIPS | 2.011 GIPS/hilo |
| Leguizamon | Intel i7-1355U | 12 | 17.380 GIPS | 1.448 GIPS/hilo |

### Conclusión

El i9-14900HX obtuvo aproximadamente 4,5 veces el rating total del Ryzen 7 5700U y 8 veces más que el i7-1355U. Normalizando por hilo, la diferencia no se explica solo por la cantidad de núcleos: el i9 es un chip de notebook de alto rendimiento frente a dos procesadores de bajo consumo pensados para autonomía, lo que confirma que el rendimiento depende de frecuencia, CPI y núcleos combinados.

Entre los dos procesadores de bajo consumo, el Ryzen 7 5700U (16 hilos) superó al i7-1355U (12 hilos) tanto en rating total como por hilo, consistente con que el Ryzen tiene 8 núcleos físicos completos con SMT mientras que el i7-1355U usa una arquitectura híbrida de núcleos P y E, que reduce el rendimiento promedio por hilo.

---

## 2. OpenBenchmarking — Timed Linux Kernel Compilation

### a) Análisis de rendimiento

| CPU | Tiempo de compilación | Núcleos/Hilos |
| :-- | :-- | :-- |
| Intel Core i5-13600K | 700 s (±1.16, N=15) | 14 / 20 |
| AMD Ryzen 9 5900X | 775 s (±25.65, N=9) | 12 / 24 |

El i5-13600K compila el kernel un 10,7 % más rápido que el Ryzen 9 5900X pese a tener menos hilos (20 frente a 24), y además con resultados más consistentes. El rendimiento no depende únicamente de la cantidad de hilos, sino también de la arquitectura y la frecuencia.

### b) Comparación con AMD Ryzen 9 7950X

| CPU | Aceleración (Speedup) |
| :-- | :-- |
| Ryzen 9 7950X vs Intel Core i5-13600K | 700 / 457 = 1,53x |
| Ryzen 9 7950X vs AMD Ryzen 9 5900X | 775 / 457 = 1,70x |

El Ryzen 9 7950X presenta el mejor rendimiento de los tres, gracias a su mayor cantidad de núcleos e hilos y a una arquitectura más moderna.

### c) Eficiencia en el aprovechamiento de los núcleos

| CPU | Tiempo × núcleos |
| :-- | :-- |
| Ryzen 9 7950X | 457 × 16 = 7.312 |
| Ryzen 9 5900X | 775 × 12 = 9.300 |
| Intel i5-13600K | 700 × 14 = 9.800 |

Un menor valor indica mejor aprovechamiento de los recursos. El Ryzen 9 7950X es el más eficiente; el i5-13600K obtiene el valor más alto, en parte por su arquitectura híbrida con núcleos P y E de distinto rendimiento.

### d) Eficiencia en costo y consumo energético

| CPU | Precio aproximado | TDP / consumo |
| :-- | :-- | :-- |
| Intel Core i5-13600K | ARS $395.350 | 125 W base / 181 W turbo |
| AMD Ryzen 9 5900X | ARS $552.400 | 105 W |
| AMD Ryzen 9 7950X | ARS $1.192.034 | 170 W / 247 W pico |

El i5-13600K presenta la mejor relación precio-rendimiento. El Ryzen 9 5900X es la peor opción de las tres, al ser más caro y más lento que el Intel en esta prueba. El Ryzen 9 7950X es el más rápido, pero su precio y consumo son considerablemente mayores, por lo que conviene solo cuando se necesita minimizar el tiempo de procesamiento de forma frecuente.

---

## 3. ESP32 — Frecuencia variable

Se consiguió un ESP32 y se desarrolló un programa que ejecuta un bucle durante un intervalo fijo de 10 segundos, contabilizando la cantidad de operaciones completadas en ese lapso. La prueba se corrió en dos variantes, una con operaciones sobre números enteros y otra con operaciones de punto flotante, repitiendo la medición para cada una de las frecuencias que admite el dispositivo y manteniendo constante el resto de las condiciones.

La frecuencia de reloj marca el ritmo al que el procesador ejecuta su ciclo de trabajo. En un procesador de escritorio la administra el sistema operativo de forma automática, lo que dificulta medir su efecto de manera controlada; en cambio, un microcontrolador como el ESP32 permite fijarla por software desde el propio programa, lo que hace posible observar de forma aislada cómo cambia la capacidad de cómputo cuando lo único que se modifica es la velocidad del reloj. Se distinguió entre enteros y flotantes porque se resuelven por unidades distintas del procesador y no necesariamente escalan de la misma manera.

### Conclusiones

El tiempo de ejecución escala casi perfectamente con la inversa de la frecuencia: al variar el reloj en un factor de 6, los ciclos por iteración se mantienen constantes, lo que confirma que el trabajo es CPU-bound puro, sin cuellos de botella de memoria. La única desviación aparece a 40 MHz, con 61,15 s medidos contra 60,0 s teóricos (~1,9 % de sobrecosto), atribuible al acceso a flash, que no escala junto con la CPU.

La precisión doble costó 4,75 veces más que la simple (2,96 contra 14,06 Miter/s), algo esperable dado que el ESP32 tiene FPU de simple precisión por hardware y emula la doble precisión por software.

Por último, `float` resultó más rápido que `int` (14,06 contra 12,58 Miter/s). Aunque parezca contraintuitivo, se explica porque la FPU trabaja en paralelo a la ALU, mientras que el bucle de enteros arrastra una dependencia acumulada sobre `step_i` que obliga a esperar el resultado de cada iteración antes de continuar.

---

## 4. Timer profiling (gprof / perf)

Cada integrante realizó el tutorial completo de forma individual en su propia máquina Linux, documentando con capturas de pantalla cada paso: compilación con la flag `-pg`, ejecución del binario, generación del `analysis.txt` con `gprof`, y el análisis con los distintos flags de personalización (`-a`, `-b`, `-p`, filtrado por función específica), además del call graph generado con `gprof2dot`. Como complemento se corrió el mismo programa con `perf`, que usa muestreo en vez de instrumentación, para comparar ambos enfoques sobre el mismo binario.

### Comandos utilizados

```bash
# Compilar y ejecutar
gcc -Wall -pg test_gprof.c test_gprof_new.c -o test_gprof
./test_gprof
gprof test_gprof gmon.out > analysis.txt

# Flags de personalización
gprof -a test_gprof gmon.out > analysis_a.txt   # suprime funciones estáticas
gprof -b test_gprof gmon.out > analysis_b.txt   # elimina textos detallados
gprof -p -b test_gprof gmon.out > analysis_p.txt # solo perfil plano
gprof -pfunc1 -b test_gprof gmon.out > analysis_func1.txt

# Gráfico del call graph
gprof test_gprof gmon.out | gprof2dot -f prof | dot -Tpng -o grafico.png

# Profiling con perf
sudo perf record -o perf.data ./test_gprof
sudo perf report -i perf.data --stdio > perf_report.txt
```

### Conclusiones

Al comparar los resultados entre los tres integrantes se observaron diferencias en los tiempos absolutos según el hardware de cada uno, pero un patrón consistente en qué funciones consumían proporcionalmente más tiempo de CPU: `main` casi no gasta tiempo propio y todo su costo se propaga a través de sus hijos en el call graph.

También se verificó que la duración de la ejecución afecta la precisión del perfil. En las corridas más cortas, `gprof` toma pocas muestras (una cada 0,01 s) y el resultado queda con mucho ruido estadístico, al punto de que el orden entre funciones de tiempos parecidos puede invertirse entre una corrida y otra. Con `perf`, al tomar cientos de muestras, el reparto resultó más creíble.

Con el flag `-a` se confirmó que `func2` desaparece del perfil plano, ya que está declarada como `static` y por lo tanto es privada a su archivo. En los reportes de `perf` se notó además que `new_func1` no aparece, lo más probable porque el compilador la inlineó dentro de `func1` al ser chica y llamarse una sola vez.

En síntesis, `gprof` requiere recompilar el código y pierde precisión en programas de ejecución corta, mientras que `perf` no necesita tocar el binario y entrega un perfil más confiable, aunque mezcla actividad del sistema operativo que `gprof` no muestra.

---

## 5. Red LAN y videollamada

### a) Medición de ancho de banda en la LAN

Esta parte de la consigna no pudo realizarse. La medición de ancho de banda entre dos máquinas de una misma red local requiere que ambos equipos estén conectados al mismo router, y los integrantes del grupo trabajan desde localidades distintas, por lo que no fue posible reunir dos computadoras en una misma LAN. Por el mismo motivo tampoco se realizó la medición opcional de tasa de transferencia con `iperf3`.

### b) Mediciones de velocidad de internet

Cada integrante midió la velocidad de su conexión mediante fast.com, desde ubicaciones y tipos de conexión distintos.

| Integrante | Ubicación / tipo de conexión | Velocidad medida |
| :-- | :-- | :-- |
| Leguizamon | Yacanto — Wi-Fi satelital | (completar) |
| Borsotti | Córdoba | (completar) |
| Ciacci | Córdoba | (completar) |

### c) Latencia en distintos tipos de red

Se realizó una videollamada grupal por Google Meet y, en paralelo, cada integrante midió la latencia contra el DNS público de Google (8.8.8.8) enviando 20 paquetes:

```bash
ping -n 20 8.8.8.8   # Windows
ping -c 20 8.8.8.8   # Linux
```

| Tipo de red | Mínimo | Media | Máximo | Pérdida |
| :-- | :-- | :-- | :-- | :-- |
| Red celular | 33 ms | 38 ms | 54 ms | 0 % |
| Wi-Fi | 14,0 ms | 18,2 ms | 50,0 ms | 0 % |
| Cableada | 17 ms | (completar) | (completar) | 0 % |

Ninguna de las tres pruebas presentó pérdida de paquetes. La red celular mostró la latencia media más alta (38 ms), consistente con que el enlace móvil agrega etapas adicionales de procesamiento y transmisión respecto de un acceso fijo. El Wi-Fi obtuvo la media más baja (18,2 ms), pero con la mayor dispersión: su máximo llegó a 50 ms con un desvío de 8,04 ms, reflejo de la variabilidad propia de un medio compartido y sensible a la interferencia. Esa inestabilidad, más que el valor promedio, es lo que suele percibirse como cortes o congelamientos durante una videollamada.
