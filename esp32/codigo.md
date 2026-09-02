# Punto 3 — Frecuencia de CPU y tiempo de ejecución en un ESP32

Código cargado al ESP32 con `arduino-cli`. Los resultados obtenidos y la
conclusión están al final del propio código.

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
 *   entonces que va a resolver las sumas de float más rápido que las de
 *   cualquier otro tipo de dato.
 *
 * CÓMO SE MIDE
 *   El mismo bucle para los tres tipos, cambiando sólo la variable. Cada tipo
 *   hace la cantidad de sumas necesaria para durar ~10 s a 240 MHz, calculada
 *   con una regla de tres sobre una muestra corta. Después se repite ese mismo
 *   trabajo a 240, 160 y 80 MHz, y se compara.
 *
 *   Dos detalles necesarios: el sumando es volatile para que el compilador no
 *   borre el bucle, y hay que reajustar el puerto serie tras cada cambio de
 *   frecuencia porque el UART depende del bus APB.
 * ===========================================================================
 */

#include <Arduino.h>

// Los enums le ponen nombre a cada prueba y sirven de índice de los arrays.
// El último elemento queda valiendo la cantidad de elementos anteriores.
enum TipoDato   { ENTERO, FLOTANTE, DOBLE, CANTIDAD_TIPOS };
enum Frecuencia { F240, F160, F80, CANTIDAD_FRECUENCIAS };

const int   MHZ[CANTIDAD_FRECUENCIAS]         = { 240, 160, 80 };
const char *NOMBRE_TIPO[CANTIDAD_TIPOS]       = { "int", "float", "double" };

const unsigned long DURACION_OBJETIVO = 10000000UL;   // 10 segundos, en microsegundos
const long          MUESTRA           = 1000000L;     // sumas de la muestra de calibración

// volatile obliga a leerlos de memoria en cada vuelta: es lo que impide que el
// compilador elimine el bucle o lo reemplace por la fórmula de Gauss.
volatile int    pasoEntero   = 1;
volatile float  pasoFlotante = 1.0f;
volatile double pasoDoble    = 1.0;

long          repeticiones[CANTIDAD_TIPOS];
unsigned long tiempos[CANTIDAD_FRECUENCIAS][CANTIDAD_TIPOS];
double        ultimoTotal = 0;

unsigned long medirBucle(TipoDato tipo, long veces);
long          calcularRepeticiones(TipoDato tipo);
void          cambiarFrecuencia(Frecuencia f);
void          imprimirTabla();

// Corre el bucle de sumas del tipo pedido y devuelve cuánto tardó, en microsegundos.
// El switch va afuera del for: adentro se ejecutaría en cada vuelta y estaríamos
// midiéndolo a él en lugar de la suma.
unsigned long medirBucle(TipoDato tipo, long veces) {
  unsigned long inicio = 0, fin = 0;
  long i;

  switch (tipo) {

    case ENTERO: {
      long total = 0;
      inicio = micros();
      for (i = 0; i < veces; i++) {
        total = total + pasoEntero;
      }
      fin = micros();
      ultimoTotal = (double)total;
      break;
    }

    // El total del float se congela en 16.777.216 (2^24, su límite de dígitos
    // exactos) aunque las sumas se sigan haciendo. No afecta el tiempo medido.
    case FLOTANTE: {
      float total = 0.0f;
      inicio = micros();
      for (i = 0; i < veces; i++) {
        total = total + pasoFlotante;
      }
      fin = micros();
      ultimoTotal = (double)total;
      break;
    }

    case DOBLE: {
      double total = 0.0;
      inicio = micros();
      for (i = 0; i < veces; i++) {
        total = total + pasoDoble;
      }
      fin = micros();
      ultimoTotal = total;
      break;
    }

    default:
      break;
  }

  return fin - inicio;
}

// Calcula cuántas sumas necesita este tipo para durar DURACION_OBJETIVO, con una
// regla de tres: si MUESTRA sumas tardaron X, para durar el objetivo hacen falta
// MUESTRA * objetivo / X.
long calcularRepeticiones(TipoDato tipo) {
  unsigned long tiempoMuestra = medirBucle(tipo, MUESTRA);
  if (tiempoMuestra == 0) {
    tiempoMuestra = 1;
  }

  double veces = (double)MUESTRA * (double)DURACION_OBJETIVO / (double)tiempoMuestra;

  Serial.printf("# %-6s : %ld sumas en %lu us  ->  usamos %.0f sumas\n",
                NOMBRE_TIPO[tipo], MUESTRA, tiempoMuestra, veces);
  return (long)veces;
}

// Cambia la frecuencia del procesador y recalcula el baudrate, que se desconfigura
// al bajar de 80 MHz porque el UART sigue al bus APB.
void cambiarFrecuencia(Frecuencia f) {
  Serial.flush();
  setCpuFrequencyMhz(MHZ[f]);
  Serial.updateBaudRate(115200);
  delay(50);
}

// Imprime las dos tablas comparativas finales a partir de la matriz de resultados.
void imprimirTabla() {
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
      Serial.printf(" %8.2f   |", (double)repeticiones[t] / (double)tiempos[f][t]);
    }
    Serial.println();
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("# ===== Punto 3: frecuencia de CPU vs tiempo de ejecucion =====");
  Serial.printf("# int=%d bytes, float=%d bytes, double=%d bytes\n",
                (int)sizeof(int), (int)sizeof(float), (int)sizeof(double));

  // Se calibra a la frecuencia más alta y esos mismos números se usan en todas
  // las demás: el trabajo tiene que ser idéntico para poder compararlo.
  cambiarFrecuencia(F240);
  Serial.printf("# Calibrando a %d MHz para durar ~%lu s por prueba:\n",
                MHZ[F240], DURACION_OBJETIVO / 1000000UL);

  repeticiones[ENTERO]   = calcularRepeticiones(ENTERO);
  repeticiones[FLOTANTE] = calcularRepeticiones(FLOTANTE);
  repeticiones[DOBLE]    = calcularRepeticiones(DOBLE);

  Serial.println("#");
  Serial.println("mhz,tipo,sumas,segundos,total");

  for (int f = 0; f < CANTIDAD_FRECUENCIAS; f++) {
    cambiarFrecuencia((Frecuencia)f);

    for (int t = 0; t < CANTIDAD_TIPOS; t++) {
      tiempos[f][t] = medirBucle((TipoDato)t, repeticiones[t]);

      Serial.printf("%d,%s,%ld,%.3f,%.0f\n",
                    MHZ[f], NOMBRE_TIPO[t], repeticiones[t],
                    tiempos[f][t] / 1000000.0, ultimoTotal);
      Serial.flush();
    }
  }

  imprimirTabla();
  cambiarFrecuencia(F240);
  Serial.println("# fin");
}

void loop() {
  delay(1000);
}

/* ===========================================================================
 * RESULTADOS OBTENIDOS
 *
 * # int=4 bytes, float=4 bytes, double=8 bytes
 * # Calibrando a 240 MHz para durar ~10 s por prueba:
 * # int    : 1000000 sumas en  54375 us  ->  usamos 183908046 sumas
 * # float  : 1000000 sumas en  71105 us  ->  usamos 140637086 sumas
 * # double : 1000000 sumas en 280270 us  ->  usamos  35679880 sumas
 *
 * mhz,tipo,sumas,segundos,total
 * 240,int,183908045,9.999,183908045
 * 240,float,140637085,9.999,16777216
 * 240,double,35679880,9.963,35679880
 * 160,int,183908045,15.027,183908045
 * 160,float,140637085,15.028,16777216
 * 160,double,35679880,14.975,35679880
 * 80,int,183908045,30.223,183908045
 * 80,float,140637085,30.225,16777216
 * 80,double,35679880,30.122,35679880
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
 *
 * CONCLUSIÓN
 *
 * Qué pasa al variar la frecuencia (la pregunta de la consigna).
 *   El tiempo resulta inversamente proporcional a la frecuencia. De 80 a
 *   160 MHz se duplica el reloj y la velocidad se duplica: 6.09 -> 12.24
 *   Msumas/s (x2.01), y el tiempo cae de 30.22 a 15.03 s. De 160 a 240 MHz
 *   el reloj sube x1.5 y la velocidad hace x1.50. La cantidad de ciclos que
 *   necesita el trabajo no cambia; lo único que cambia es cuánto dura cada
 *   ciclo. Esto vale igual para los tres tipos de dato.
 *
 * La hipótesis se cumple sólo en parte.
 *   Contra el double se confirma: a 240 MHz el float hace 14.06 Msumas/s
 *   contra 3.58 del double, o sea 3.9 veces más rápido. Ahí se ve la FPU
 *   trabajando: el float de 32 bits lo suma el hardware en una instrucción
 *   (add.s), mientras que el double de 64 bits no tiene hardware que lo
 *   resuelva y el compilador lo reemplaza por una rutina de software
 *   (__adddf3) de decenas de instrucciones enteras.
 *
 *   Contra el int, en cambio, no se cumple: el float quedó 1.3 veces más
 *   lento (14.06 contra 18.39 Msumas/s). La FPU no lo vuelve más rápido que
 *   cualquier otro tipo, como suponíamos; lo que hace es ponerlo a la par de
 *   la aritmética entera, en el mismo orden de magnitud. Que un número con
 *   coma se acerque tanto a un entero ya es consecuencia de esa aceleración
 *   por hardware, y se nota al compararlo con el double, que no la tiene.
 *
 * Detalle del total del float.
 *   Los tres tipos hacen todas sus sumas, pero el total del float queda en
 *   16777216 (2^24) porque es el entero más grande que puede representar con
 *   exactitud; de ahí en adelante sumarle 1 no lo cambia. No afecta los
 *   tiempos medidos.
 * ===========================================================================
 */
```
