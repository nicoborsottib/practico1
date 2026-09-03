# Punto 3 — Frecuencia de CPU y tiempo de ejecución en un ESP32

Sketch de Arduino cargado al ESP32 desde la terminal de Linux con `arduino-cli`.
Los resultados medidos y la conclusión están al final del propio código.

```cpp
/* ===========================================================================
 * Sistemas de Computación — TP1 — Punto 3
 * Frecuencia de CPU y tiempo de ejecución en un ESP32
 *
 * CONSIGNA
 *   Ejecutar un código que demore alrededor de 10 segundos: un bucle for con
 *   sumas de enteros, otro con floats y otro con doubles. ¿Qué sucede con el
 *   tiempo del programa al duplicar (variar) la frecuencia? Notar que el ESP32
 *   tiene aceleración por hardware para float.
 *
 * HIPÓTESIS
 *   El ESP32 tiene un módulo de punto flotante (FPU) por hardware. Suponemos
 *   que las operaciones algebraicas con este tipo de datos se resolverán más
 *   veloz que con cualquier otro tipo de dato.
 *
 * CÓMO SE MIDE
 *   El mismo bucle para los tres tipos, cambiando sólo la variable. Cada tipo
 *   hace la cantidad de sumas necesaria para durar ~10 s a 240 MHz; esos
 *   números están fijos en la constante REPETICIONES y se obtuvieron una sola
 *   vez con una regla de tres (el cálculo está explicado ahí abajo). Después
 *   se repite ese mismo trabajo a 240, 160 y 80 MHz, y se compara.
 *
 *   Dos detalles necesarios: el sumando es volatile para que el compilador no
 *   borre el bucle, y hay que reajustar el puerto serie tras cada cambio de
 *   frecuencia porque el UART depende del bus APB.
 *
 * HERRAMIENTAS
 *   El programa es un sketch de Arduino para ESP32. Se trabajó desde la
 *   terminal de Linux con arduino-cli, usando el core esp32:esp32 de Espressif:
 *
 *     arduino-cli compile --fqbn esp32:esp32:esp32 medicion
 *     arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 medicion
 *     arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200
 *
 *   De Arduino se usan Serial (puerto serie), micros() (reloj en microsegundos)
 *   y setCpuFrequencyMhz(), que es la función propia del ESP32 que permite
 *   cambiar la frecuencia del procesador desde el propio programa.
 * ===========================================================================
 */

#include <Arduino.h>


/* --- ENUMS -----------------------------------------------------------------
 * Se usan como tipo de dato de los parámetros de las funciones y como índice
 * de los arrays. El último elemento queda valiendo la cantidad de elementos
 * anteriores, así el tamaño de los arrays se corrige solo.
 */
enum TipoDato   { ENTERO, FLOTANTE, DOBLE, CANTIDAD_TIPOS };
enum Frecuencia { F240, F160, F80, CANTIDAD_FRECUENCIAS };


/* --- CONSTANTES ---------------------------------------------------------- */

const int   MHZ[CANTIDAD_FRECUENCIAS]   = { 240, 160, 80 };
const char *NOMBRE_TIPO[CANTIDAD_TIPOS] = { "int", "float", "double" };

/* Cuántas sumas hace cada tipo para durar ~10 segundos a 240 MHz.
 *
 * Estos números se calcularon una sola vez, con una medición previa y una
 * regla de tres. Ejemplo con el int: se midió que 1.000.000 de sumas tardaban
 * 54.375 microsegundos, y como el tiempo es proporcional a la cantidad de
 * vueltas, para llegar a los 10.000.000 de microsegundos que buscamos:
 *
 *     1.000.000 sumas  ->     54.375 us
 *             X sumas  ->  10.000.000 us
 *
 *     X = 1.000.000 * 10.000.000 / 54.375 = 183.908.045 sumas
 *
 * Los otros dos salen igual (el float tardaba 71.105 us y el double 280.270 us
 * para ese mismo millón de sumas). Cada tipo necesita un número distinto
 * justamente porque cada uno trabaja a distinta velocidad.
 */
const long REPETICIONES[CANTIDAD_TIPOS] = { 183908045, 140637085, 35679880 };


/* --- FUNCIONES -------------------------------------------------------------
 * medirBucle        : corre el bucle de sumas del tipo pedido y devuelve
 *                     cuánto tardó, en microsegundos.
 * cambiarFrecuencia : fija la frecuencia del procesador y reacomoda el
 *                     puerto serie.
 * imprimirTabla     : muestra las tablas comparativas con todo lo medido.
 */
unsigned long medirBucle(TipoDato tipo, long veces, double &total);
void          cambiarFrecuencia(Frecuencia f);
void          imprimirTabla(unsigned long tiempos[][CANTIDAD_TIPOS],
                            double totales[]);


/* ---------------------------------------------------------------------------
 * Corre el bucle de sumas del tipo pedido.
 */
unsigned long medirBucle(TipoDato tipo, long veces, double &total) {
  unsigned long inicio = 0, fin = 0;
  long i;

  switch (tipo) {

    /* Caso entero. El sumando es volatile: eso le prohíbe al compilador
     * guardarlo en un registro y lo obliga a leerlo de memoria en cada vuelta.
     * Sin eso, el compilador borraría el bucle entero por no usarse el
     * resultado, o reemplazaría las sumas por la fórmula de Gauss.
     * El acumulador, en cambio, es una variable normal, así se queda en un
     * registro del procesador durante todo el bucle. */
    case ENTERO: {
      volatile int paso = 1;
      long acumulado = 0;
      inicio = micros();
      for (i = 0; i < veces; i++) {
        acumulado = acumulado + paso;
      }
      fin = micros();
      total = (double)acumulado;
      break;
    }

    /* Caso float, idéntico al anterior pero con la variable de 32 bits con
     * coma, que es la que resuelve la FPU del ESP32.
     * El acumulado se congela en 16.777.216 (2^24) porque es el entero más
     * grande que un float puede representar con exactitud; de ahí en adelante
     * sumarle 1 devuelve el mismo número. Las sumas se siguen haciendo igual,
     * así que el tiempo medido es válido. */
    case FLOTANTE: {
      volatile float paso = 1.0f;
      float acumulado = 0.0f;
      inicio = micros();
      for (i = 0; i < veces; i++) {
        acumulado = acumulado + paso;
      }
      fin = micros();
      total = (double)acumulado;
      break;
    }

    /* Caso double, la variable de 64 bits con coma. Para este tamaño el ESP32
     * no tiene hardware, así que el compilador reemplaza cada suma por una
     * llamada a una rutina de software. */
    case DOBLE: {
      volatile double paso = 1.0;
      double acumulado = 0.0;
      inicio = micros();
      for (i = 0; i < veces; i++) {
        acumulado = acumulado + paso;
      }
      fin = micros();
      total = acumulado;
      break;
    }

    default:
      break;
  }

  return fin - inicio;
}


/* Fija la frecuencia del procesador y recalcula el baudrate, que se desconfigura
 * al bajar de 80 MHz porque el puerto serie sigue al bus APB. */
void cambiarFrecuencia(Frecuencia f) {
  Serial.flush();
  setCpuFrequencyMhz(MHZ[f]);
  Serial.updateBaudRate(115200);
  delay(50);
}


/* Imprime las tablas comparativas a partir de todo lo medido. */
void imprimirTabla(unsigned long tiempos[][CANTIDAD_TIPOS], double totales[]) {
  Serial.println();
  Serial.println("            TIEMPO DE CADA PRUEBA (segundos)");
  Serial.println("  MHz  |     int    |    float   |   double");
  Serial.println("-------+------------+------------+------------");
  for (int f = 0; f < CANTIDAD_FRECUENCIAS; f++) {
    Serial.printf("  %3d  |", MHZ[f]);
    for (int t = 0; t < CANTIDAD_TIPOS; t++) {
      Serial.printf(" %8.3f   |", tiempos[f][t] / 1000000.0);
    }
    Serial.println();
  }

  Serial.println();
  Serial.println("       VELOCIDAD (millones de sumas por segundo)");
  Serial.println("  MHz  |     int    |    float   |   double");
  Serial.println("-------+------------+------------+------------");
  for (int f = 0; f < CANTIDAD_FRECUENCIAS; f++) {
    Serial.printf("  %3d  |", MHZ[f]);
    for (int t = 0; t < CANTIDAD_TIPOS; t++) {
      Serial.printf(" %8.2f   |", (double)REPETICIONES[t] / (double)tiempos[f][t]);
    }
    Serial.println();
  }

  Serial.println();
  Serial.println("  Sumas hechas y total acumulado por cada tipo:");
  for (int t = 0; t < CANTIDAD_TIPOS; t++) {
    Serial.printf("    %-6s : %ld sumas  ->  total %.0f\n",
                  NOMBRE_TIPO[t], REPETICIONES[t], totales[t]);
  }
  Serial.println();
}


void setup() {
  Serial.begin(115200);
  delay(2000);

  // Todo lo medido vive acá, dentro de setup, y se pasa por parámetro.
  unsigned long tiempos[CANTIDAD_FRECUENCIAS][CANTIDAD_TIPOS];
  double        totales[CANTIDAD_TIPOS];

  Serial.println();
  Serial.println("# Punto 3: frecuencia de CPU vs tiempo de ejecucion");
  Serial.printf("# int=%d bytes, float=%d bytes, double=%d bytes\n",
                (int)sizeof(int), (int)sizeof(float), (int)sizeof(double));
  Serial.println("# Midiendo, tarda unos 3 minutos...");

  for (int f = 0; f < CANTIDAD_FRECUENCIAS; f++) {
    cambiarFrecuencia((Frecuencia)f);
    for (int t = 0; t < CANTIDAD_TIPOS; t++) {
      tiempos[f][t] = medirBucle((TipoDato)t, REPETICIONES[t], totales[t]);
    }
  }

  imprimirTabla(tiempos, totales);

  cambiarFrecuencia(F240);
  Serial.println("# fin");
}


void loop() {
  delay(1000);
}


/* ===========================================================================
 * RESULTADOS OBTENIDOS
 *
 *             TIEMPO DE CADA PRUEBA (segundos)
 *   MHz  |     int    |    float   |   double
 * -------+------------+------------+------------
 *   240  |    9.999   |    9.999   |    9.963   |
 *   160  |   15.027   |   15.028   |   14.975   |
 *    80  |   30.223   |   30.225   |   30.122   |
 *
 *        VELOCIDAD (millones de sumas por segundo)
 *   MHz  |     int    |    float   |   double
 * -------+------------+------------+------------
 *   240  |    18.39   |    14.06   |     3.58   |
 *   160  |    12.24   |     9.36   |     2.38   |
 *    80  |     6.09   |     4.65   |     1.18   |
 *
 *   Sumas hechas y total acumulado por cada tipo:
 *     int    : 183908045 sumas  ->  total 183908045
 *     float  : 140637085 sumas  ->  total 16777216
 *     double :  35679880 sumas  ->  total 35679880
 *
 *
 * CONCLUSIÓN
 *
 * Lo que se observó fue que contra el double se confirma que es más veloz: a
 * 240 MHz el float hace 14.06 Msumas/s contra 3.58 del double, o sea 3.9 veces
 * más rápido. Ahí se ve la FPU trabajando.
 *
 * Contra el int, en cambio, no se cumple: el float quedó 1.3 veces más lento
 * (14.06 contra 18.39 Msumas/s). La FPU no lo vuelve más rápido que cualquier
 * otro tipo, como suponíamos; lo que hace es ponerlo a la par de la aritmética
 * entera, en el mismo orden de magnitud.
 *
 * Los tres tipos hacen todas sus sumas, pero el total del float queda en
 * 16777216 (2^24) porque es el entero más grande que puede representar con
 * exactitud; de ahí en adelante sumarle 1 no lo cambia. No afecta los tiempos
 * medidos.
 * ===========================================================================
 */
```
